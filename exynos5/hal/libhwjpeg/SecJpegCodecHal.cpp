/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <sys/poll.h>

#include <cutils/log.h>

#include <utils/Log.h>

#include "SecJpegCodecHal.h"

#define JPEG_ERROR_LOG(fmt,...)

SecJpegCodecHal::SecJpegCodecHal()
{
}

SecJpegCodecHal::~SecJpegCodecHal()
{
}

int SecJpegCodecHal::t_v4l2Querycap(int iFd)
{
    struct v4l2_capability cap;
    int iRet = ERROR_NONE;

    iRet = ioctl(iFd, VIDIOC_QUERYCAP, &cap);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_QUERYCAP failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2SetJpegcomp(int iFd, int iQuality)
{
    struct v4l2_jpegcompression arg;
    int iRet = ERROR_NONE;

    arg.quality = iQuality;

    iRet = ioctl(iFd, VIDIOC_S_JPEGCOMP, &arg);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_S_JPEGCOMP failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2SetFmt(int iFd, enum v4l2_buf_type eType, struct CONFIG *pstConfig)
{
    struct v4l2_format fmt;
    int iRet = ERROR_NONE;

    fmt.type = eType;
    fmt.fmt.pix_mp.width = pstConfig->width;
    fmt.fmt.pix_mp.height = pstConfig->height;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.num_planes = pstConfig->numOfPlanes;

    if (pstConfig->mode == MODE_ENCODE)
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:    // fall through
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        if (pstConfig->mode == MODE_ENCODE) {
            fmt.fmt.pix_mp.pixelformat = pstConfig->pix.enc_fmt.in_fmt;
        } else {
            fmt.fmt.pix_mp.pixelformat = pstConfig->pix.dec_fmt.in_fmt;
            fmt.fmt.pix_mp.plane_fmt[0].sizeimage = pstConfig->sizeJpeg;
        }
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        if (pstConfig->mode == MODE_ENCODE) {
            fmt.fmt.pix_mp.pixelformat = pstConfig->pix.enc_fmt.out_fmt;
        } else {
            fmt.fmt.pix_mp.pixelformat = pstConfig->pix.dec_fmt.out_fmt;
            fmt.fmt.pix_mp.width = pstConfig->scaled_width;
            fmt.fmt.pix_mp.height = pstConfig->scaled_height;
        }
        break;
    default:
            return -ERROR_INVALID_V4l2_BUF_TYPE;
            break;
    }

    iRet = ioctl(iFd, VIDIOC_S_FMT, &fmt);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_S_FMT failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2GetFmt(int iFd, enum v4l2_buf_type eType, struct CONFIG *pstConfig)
{
    struct v4l2_format fmt;
    int iRet = ERROR_NONE;

    fmt.type = eType;
    iRet = ioctl(iFd, VIDIOC_G_FMT, &fmt);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_G_FMT failed", __func__, iRet);
        return iRet;
    }

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:    // fall through
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        pstConfig->width = fmt.fmt.pix.width;
        pstConfig->height = fmt.fmt.pix.height;
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        pstConfig->width = fmt.fmt.pix_mp.width;
        pstConfig->height = fmt.fmt.pix_mp.height;
        if (pstConfig->mode == MODE_ENCODE)
            pstConfig->pix.enc_fmt.in_fmt = fmt.fmt.pix_mp.pixelformat;
        else
            pstConfig->pix.dec_fmt.in_fmt = fmt.fmt.pix_mp.pixelformat;
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        pstConfig->width = fmt.fmt.pix_mp.width;
        pstConfig->height = fmt.fmt.pix_mp.height;
        if (pstConfig->mode == MODE_ENCODE)
            pstConfig->pix.enc_fmt.out_fmt = fmt.fmt.pix_mp.pixelformat;
        else
            pstConfig->pix.dec_fmt.out_fmt = fmt.fmt.pix_mp.pixelformat;
        break;
    default:
        return -ERROR_INVALID_V4l2_BUF_TYPE;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2Reqbufs(int iFd, int iBufCount, struct BUF_INFO *pstBufInfo)
{
    struct v4l2_requestbuffers req;
    int iRet = ERROR_NONE;

    memset(&req, 0, sizeof(req));

    req.type = pstBufInfo->buf_type;
    req.memory = pstBufInfo->memory;

    //if (pstBufInfo->memory == V4L2_MEMORY_MMAP)
        req.count = iBufCount;

    iRet = ioctl(iFd, VIDIOC_REQBUFS, &req);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_REQBUFS failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2Querybuf(int iFd, struct BUF_INFO *pstBufInfo, struct BUFFER *pstBuf)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane plane[JPEG_MAX_PLANE_CNT];
    int iRet = ERROR_NONE;
    int i;

    memset(plane, 0, (int)JPEG_MAX_PLANE_CNT * sizeof(struct v4l2_plane));

    v4l2_buf.index = 0;
    v4l2_buf.type = pstBufInfo->buf_type;
    v4l2_buf.memory = pstBufInfo->memory;
    v4l2_buf.length = pstBufInfo->numOfPlanes;
    v4l2_buf.m.planes = plane;

    iRet = ioctl(iFd, VIDIOC_QUERYBUF, &v4l2_buf);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d]: VIDIOC_QUERYBUF failed", __func__, iRet);
        return iRet;
    }

    for (i= 0; i < v4l2_buf.length; i++) {
        pstBuf->size[i] = v4l2_buf.m.planes[i].length;
        pstBuf->addr[i] = (char *) mmap(0,
            pstBuf->size[i],
            PROT_READ | PROT_WRITE, MAP_SHARED, iFd,
            v4l2_buf.m.planes[i].m.mem_offset);

        if (pstBuf->addr[i] == MAP_FAILED) {
            JPEG_ERROR_LOG("[%s]: mmap failed", __func__);
            return -ERROR_MMAP_FAILED;
        }
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2Qbuf(int iFd, struct BUF_INFO *pstBufInfo, struct BUFFER *pstBuf)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane plane[JPEG_MAX_PLANE_CNT];
    int i;
    int iRet = ERROR_NONE;

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(plane, 0, (int)JPEG_MAX_PLANE_CNT * sizeof(struct v4l2_plane));

    v4l2_buf.index = 0;
    v4l2_buf.type = pstBufInfo->buf_type;
    v4l2_buf.memory = pstBufInfo->memory;
    v4l2_buf.length = pstBufInfo->numOfPlanes;
    v4l2_buf.m.planes = plane;

    if (pstBufInfo->memory == V4L2_MEMORY_USERPTR) {
        for (i = 0; i < pstBufInfo->numOfPlanes; i++) {
            v4l2_buf.m.planes[i].m.userptr = (unsigned long)pstBuf->addr[i];
            v4l2_buf.m.planes[i].length = pstBuf->size[i];
        }
    }

    iRet = ioctl(iFd, VIDIOC_QBUF, &v4l2_buf);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d] VIDIOC_QBUF failed", __func__, iRet);
        pstBuf->numOfPlanes = 0;
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2Dqbuf(int iFd, enum v4l2_buf_type eType, enum v4l2_memory eMemory)
{
    struct v4l2_buffer buf;
    int iRet = ERROR_NONE;

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    buf.type = eType;
    buf.memory = eMemory;

    iRet = ioctl(iFd, VIDIOC_DQBUF, &buf);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d] VIDIOC_DQBUF failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2StreamOn(int iFd, enum v4l2_buf_type eType)
{
    int iRet = ERROR_NONE;

    iRet = ioctl(iFd, VIDIOC_STREAMON, &eType);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d] VIDIOC_STREAMON failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2StreamOff(int iFd, enum v4l2_buf_type eType)
{
    int iRet = ERROR_NONE;

    iRet = ioctl(iFd, VIDIOC_STREAMOFF, &eType);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s:%d] VIDIOC_STREAMOFF failed", __func__, iRet);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2SetCtrl(int iFd, int iCid, int iValue)
{
    struct v4l2_control vc;
    int iRet = ERROR_NONE;

    vc.id = iCid;
    vc.value = iValue;

    iRet = ioctl(iFd, VIDIOC_S_CTRL, &vc);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s] VIDIOC_S_CTRL failed : cid(%d), value(%d)\n", __func__, iCid, iValue);
        return iRet;
    }

    return iRet;
}

int SecJpegCodecHal::t_v4l2GetCtrl(int iFd, int iCid)
{
    struct v4l2_control ctrl;
    int iRet = ERROR_NONE;

    ctrl.id = iCid;

    iRet = ioctl(iFd, VIDIOC_G_CTRL, &ctrl);
    if (iRet < 0) {
        JPEG_ERROR_LOG("[%s] VIDIOC_G_CTRL failed : cid(%d)\n", __func__, ctrl.id);
        return iRet;
    }

    return ctrl.value;
}

