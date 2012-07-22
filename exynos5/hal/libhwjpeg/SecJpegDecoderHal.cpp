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

#define JPEG_ERROR_ALOG(fmt,...)

#define NUM_PLANES (1)
#define NUM_BUFFERS (1)

SecJpegDecoderHal::SecJpegDecoderHal()
{
    t_iJpegFd = -1;
    t_bFlagCreate = false;
}

SecJpegDecoderHal::~SecJpegDecoderHal()
{
    if (t_bFlagCreate == true) {
        this->destroy();
    }
}

int SecJpegDecoderHal::create(void)
{
    if (t_bFlagCreate == true) {
        return ERROR_JPEG_DEVICE_ALREADY_CREATE;
    }

    int iRet = ERROR_NONE;

    t_iJpegFd = open(JPEG_DEC_NODE, O_RDWR, 0);

    if (t_iJpegFd < 0) {
        t_iJpegFd = -1;
        JPEG_ERROR_ALOG("[%s]: JPEG_DEC_NODE open failed", __func__);
        return ERROR_CANNOT_OPEN_JPEG_DEVICE;
    }

    if (t_iJpegFd <= 0) {
        t_iJpegFd = -1;
        JPEG_ERROR_ALOG("ERR(%s):JPEG device was closed\n", __func__);
        return ERROR_JPEG_DEVICE_ALREADY_CLOSED;
    }

    iRet = t_v4l2Querycap(t_iJpegFd);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s]: QUERYCAP failed", __func__);
        close(t_iJpegFd);
        return ERROR_CANNOT_OPEN_JPEG_DEVICE;
    }

    memset(&t_stJpegConfig, 0, sizeof(struct CONFIG));
    memset(&t_stJpegInbuf, 0, sizeof(struct BUFFER));
    memset(&t_stJpegOutbuf, 0, sizeof(struct BUFFER));

    t_stJpegConfig.mode = MODE_DECODE;

    t_bFlagCreate = true;
    t_bFlagCreateInBuf = false;
    t_bFlagCreateOutBuf = false;
    t_bFlagExcute = false;

    t_iPlaneNum = 0;

    return ERROR_NONE;
}

int SecJpegDecoderHal::destroy(void)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_ALREADY_DESTROY;
    }

    if (t_iJpegFd > 0) {
        struct BUF_INFO stBufInfo;
        int iRet = ERROR_NONE;

        if (t_bFlagExcute) {
            iRet = t_v4l2StreamOff(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
        }

        if (t_bFlagExcute) {
            stBufInfo.numOfPlanes = NUM_PLANES;
            stBufInfo.memory = V4L2_MEMORY_MMAP;

            stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            iRet = t_v4l2Reqbufs(t_iJpegFd, 0, &stBufInfo);

            stBufInfo.numOfPlanes = t_iPlaneNum;
            stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            iRet = t_v4l2Reqbufs(t_iJpegFd, 0, &stBufInfo);
        }

        iRet = close(t_iJpegFd);
        if (iRet < 0) {
            JPEG_ERROR_ALOG("[%s:%d]: JPEG_DEC_NODE close failed", __func__, iRet);
        }
    }

    t_iJpegFd = -1;
    t_bFlagCreate = false;
    return ERROR_NONE;
}

int SecJpegDecoderHal::setSize(int iW, int iH)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (iW < 0 || MAX_JPG_WIDTH < iW) {
        return ERROR_INVALID_IMAGE_SIZE;
    }

    if (iH < 0 || MAX_JPG_HEIGHT < iH) {
        return ERROR_INVALID_IMAGE_SIZE;
    }

    t_stJpegConfig.width = iW;
    t_stJpegConfig.height = iH;

    return ERROR_NONE;
}

int SecJpegDecoderHal::setJpegConfig(void *pConfig)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (pConfig == NULL) {
        return ERROR_JPEG_CONFIG_POINTER_NULL;
    }

    memcpy(&t_stJpegConfig, pConfig, sizeof(struct CONFIG));

    if (t_stJpegConfig.pix.dec_fmt.out_fmt == V4L2_PIX_FMT_NV12) {
        t_iPlaneNum = 2;
    } else {
        t_iPlaneNum = 1;
    }

    return ERROR_NONE;
}

void *SecJpegDecoderHal::getJpegConfig(void)
{
    if (t_bFlagCreate == false) {
        return NULL;
    }

    return &t_stJpegConfig;
}

char *SecJpegDecoderHal::getInBuf(int *piInputSize)
{
    if (t_bFlagCreate == false) {
        return NULL;
    }

     if (t_bFlagCreateInBuf == false) {

        *piInputSize = 0;
        return NULL;
    }

    *piInputSize = t_stJpegInbuf.size[0];

    return (char *)(t_stJpegInbuf.addr[0]);
}

char **SecJpegDecoderHal::getOutBuf(int *piOutputSize)
{
    if (t_bFlagCreate == false) {
        return NULL;
    }

    if (t_bFlagCreateOutBuf == false) {

        *piOutputSize = 0;
        return NULL;
    }

    for (int i=0;i<t_iPlaneNum;i++) {
        piOutputSize[i] = t_stJpegOutbuf.size[i];
    }

    return t_stJpegOutbuf.addr;
}

int  SecJpegDecoderHal::setInBuf(char *pcBuf, int iSize)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (pcBuf == NULL) {
        return ERROR_BUFFR_IS_NULL;
    }

    if (iSize<=0) {
        return ERROR_BUFFER_TOO_SMALL;
    }

    t_stJpegInbuf.addr[0] = pcBuf;
    t_stJpegInbuf.size[0] = iSize;
    t_stJpegInbuf.numOfPlanes = NUM_PLANES;

    t_bFlagCreateInBuf = true;

    return ERROR_NONE;
}

int  SecJpegDecoderHal::setOutBuf(char **pcBuf, int *iSize)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (pcBuf == NULL) {
        return ERROR_BUFFR_IS_NULL;
    }

    if (iSize<=0) {
        return ERROR_BUFFER_TOO_SMALL;
    }

    for(int i=0;i<t_iPlaneNum;i++) {
        t_stJpegOutbuf.addr[i] = pcBuf[i];
        t_stJpegOutbuf.size[i] = iSize[i];
    }

    t_stJpegOutbuf.numOfPlanes = t_iPlaneNum;

    t_bFlagCreateOutBuf = true;

    return ERROR_NONE;
}

int SecJpegDecoderHal::setCache(int iValue)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (t_v4l2SetCtrl(t_iJpegFd, V4L2_CID_CACHEABLE, iValue)<0) {
        JPEG_ERROR_ALOG("%s::cache setting failed\n", __func__);
        return ERROR_CANNOT_CHANGE_CACHE_SETTING;
    }

    return ERROR_NONE;
}

int SecJpegDecoderHal::getSize(int *piW, int *piH)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    int iRet = t_v4l2GetFmt(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &t_stJpegConfig);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s,%d]: get image size failed", __func__,iRet);
        return ERROR_GET_SIZE_FAIL;
    }

    *piW = t_stJpegConfig.width;
    *piH = t_stJpegConfig.height;

    return ERROR_NONE;
}

int SecJpegDecoderHal::setColorFormat(int iV4l2ColorFormat)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    switch(iV4l2ColorFormat) {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_RGB565X:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB32:
        t_stJpegConfig.pix.dec_fmt.out_fmt = iV4l2ColorFormat;
        break;
    default:
        t_iPlaneNum = 0;
        return ERROR_INVALID_COLOR_FORMAT;
        break;
    }

    if (iV4l2ColorFormat == V4L2_PIX_FMT_NV12) {
        t_iPlaneNum = 2;
    } else {
        t_iPlaneNum = 1;
    }

    return ERROR_NONE;
}

int SecJpegDecoderHal::setJpegFormat(int iV4l2JpegFormat)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    switch(iV4l2JpegFormat) {
    case V4L2_PIX_FMT_JPEG_444:
    case V4L2_PIX_FMT_JPEG_422:
    case V4L2_PIX_FMT_JPEG_420:
    case V4L2_PIX_FMT_JPEG_GRAY:
        t_stJpegConfig.pix.dec_fmt.in_fmt = iV4l2JpegFormat;
        break;
    default:
        return ERROR_INVALID_JPEG_FORMAT;
        break;
    }

    return ERROR_NONE;
}

int SecJpegDecoderHal::updateConfig(void)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    int iRet = ERROR_NONE;

    t_stJpegConfig.numOfPlanes = NUM_PLANES;

    t_stJpegConfig.mode = MODE_DECODE;

    iRet = t_v4l2SetFmt(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, &t_stJpegConfig);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s,%d]: jpeg input S_FMT failed", __func__,iRet);
        return ERROR_INVALID_JPEG_CONFIG;
    }

    struct BUF_INFO stBufInfo;

    stBufInfo.numOfPlanes = NUM_PLANES;
    stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    stBufInfo.memory = V4L2_MEMORY_USERPTR;

    iRet = t_v4l2Reqbufs(t_iJpegFd, NUM_BUFFERS, &stBufInfo);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Input REQBUFS failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }

    t_stJpegConfig.numOfPlanes = t_iPlaneNum;
    iRet = t_v4l2SetFmt(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, &t_stJpegConfig);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s,%d]: jpeg output S_FMT failed", __func__,iRet);
        return ERROR_INVALID_JPEG_CONFIG;
    }

    stBufInfo.numOfPlanes = t_iPlaneNum;
    stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    iRet = t_v4l2Reqbufs(t_iJpegFd, NUM_BUFFERS, &stBufInfo);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Output REQBUFS failed", __func__, iRet);
        return ERROR_REQBUF_FAIL;
    }

    return ERROR_NONE;
}

int SecJpegDecoderHal::setScaledSize(int iW, int iH)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (iW < 0 || MAX_JPG_WIDTH < iW) {
        return ERROR_INVALID_IMAGE_SIZE;
    }

    if (iH < 0 || MAX_JPG_HEIGHT < iH) {
        return ERROR_INVALID_IMAGE_SIZE;
    }

    t_stJpegConfig.scaled_width = iW;
    t_stJpegConfig.scaled_height = iH;

    return ERROR_NONE;
}

int SecJpegDecoderHal::setJpegSize(int iJpegSize)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    if (iJpegSize<=0) {
        return ERROR_JPEG_SIZE_TOO_SMALL;
    }

    t_stJpegConfig.sizeJpeg = iJpegSize;

    return ERROR_NONE;
}

int SecJpegDecoderHal::decode(void)
{
    if (t_bFlagCreate == false) {
        return ERROR_JPEG_DEVICE_NOT_CREATE_YET;
    }

    struct BUF_INFO stBufInfo;
    int iRet = ERROR_NONE;

    t_bFlagExcute = true;

    stBufInfo.numOfPlanes = NUM_PLANES;
    stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    stBufInfo.memory = V4L2_MEMORY_USERPTR;

    iRet = t_v4l2Qbuf(t_iJpegFd, &stBufInfo, &t_stJpegInbuf);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Input QBUF failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }

    stBufInfo.numOfPlanes = t_iPlaneNum;
    stBufInfo.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    iRet = t_v4l2Qbuf(t_iJpegFd, &stBufInfo, &t_stJpegOutbuf);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Output QBUF failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }

    stBufInfo.numOfPlanes = NUM_PLANES;
    iRet = t_v4l2StreamOn(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: input stream on failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }
    stBufInfo.numOfPlanes = t_iPlaneNum;
    iRet = t_v4l2StreamOn(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: output stream on failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }

    stBufInfo.numOfPlanes = NUM_PLANES;
    iRet = t_v4l2Dqbuf(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Intput DQBUF failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }
    stBufInfo.numOfPlanes = t_iPlaneNum;
    iRet = t_v4l2Dqbuf(t_iJpegFd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP);
    if (iRet < 0) {
        JPEG_ERROR_ALOG("[%s:%d]: Output DQBUF failed", __func__, iRet);
        return ERROR_EXCUTE_FAIL;
    }

    return ERROR_NONE;
}

