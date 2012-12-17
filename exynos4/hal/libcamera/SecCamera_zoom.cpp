/*
 * Copyright 2008, The Android Open Source Project
 * Copyright 2010, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
************************************
* Filename: SecCamera.cpp
* Author:   Sachin P. Kamat
* Purpose:  This file interacts with the Camera and JPEG drivers.
*************************************
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "SecCamera"

#include <utils/Log.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include "SecCamera.h"
#include "cutils/properties.h"

using namespace android;

#define CHECK(return_value)                                          \
    if (return_value < 0) {                                          \
        ALOGE("%s::%d fail. errno: %s, m_camera_id = %d",             \
             __func__, __LINE__, strerror(errno), m_camera_id);      \
        return -1;                                                   \
    }

#define CHECK_PTR(return_value)                                      \
    if (return_value < 0) {                                          \
        ALOGE("%s::%d fail, errno: %s, m_camera_id = %d",             \
             __func__,__LINE__, strerror(errno), m_camera_id);       \
        return NULL;                                                 \
    }

namespace android {

static struct timeval time_start;
static struct timeval time_stop;

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
unsigned long measure_time_camera(struct timeval *start, struct timeval *stop)
{
    unsigned long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;

    if (stop->tv_usec >= start->tv_usec) {
        usec = stop->tv_usec - start->tv_usec;
    } else {
        usec = stop->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }

    time = (sec * 1000000) + usec;

    return time;
}
#endif

static int close_buffers(struct SecBuffer *buffers, int num_of_buf)
{
    int ret;

    for (int i = 0; i < num_of_buf; i++) {
        for(int j = 0; j < MAX_PLANES; j++) {
            if (buffers[i].virt.extP[j]) {
                ret = munmap(buffers[i].virt.extP[j], buffers[i].size.extS[j]);
                ALOGV("munmap():buffers[%d].virt.extP[%d]: 0x%x size = %d",
                        i, j, (unsigned int) buffers[i].virt.extP[j],
                        buffers[i].size.extS[j]);
                buffers[i].virt.extP[j] = NULL;
            }
        }
    }

    return 0;
}

static int get_pixel_depth(unsigned int fmt)
{
    int depth = 0;

    switch (fmt) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:
        depth = 12;
        break;

    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_YUV422P:
        depth = 16;
        break;

    case V4L2_PIX_FMT_RGB32:
        depth = 32;
        break;
    }

    return depth;
}

static int fimc_poll(struct pollfd *events)
{
    int ret;

    /* 10 second delay is because sensor can take a long time
     * to do auto focus and capture in dark settings
     */
    ret = poll(events, 1, 10000);
    if (ret < 0) {
        ALOGE("ERR(%s):poll error", __func__);
        return ret;
    }

    if (ret == 0) {
        ALOGE("ERR(%s):No data in 10 secs..", __func__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_querycap(int fp)
{
    struct v4l2_capability cap;

    if (ioctl(fp, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("ERR(%s):VIDIOC_QUERYCAP failed", __func__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("ERR(%s):no capture devices", __func__);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_querycap_m2m(int fp)
{
    struct v4l2_capability cap;

    if (ioctl(fp, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("ERR(%s):VIDIOC_QUERYCAP failed", __func__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%d has no streaming support", fp);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        ALOGE("%d is no video output", fp);
        return -1;
    }

    return 0;
}

static const __u8* fimc_v4l2_enuminput(int fp, int index)
{
    static struct v4l2_input input;

    input.index = index;
    if (ioctl(fp, VIDIOC_ENUMINPUT, &input) != 0) {
        ALOGE("ERR(%s):No matching index found", __func__);
        return NULL;
    }
    ALOGI("Name of input channel[%d] is %s", input.index, input.name);

    return input.name;
}

static int fimc_v4l2_s_input(int fp, int index)
{
    struct v4l2_input input;

    input.index = index;

    if (ioctl(fp, VIDIOC_S_INPUT, &input) < 0) {
        ALOGE("ERR(%s):VIDIOC_S_INPUT failed", __func__);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_s_fmt(int fp, int width, int height, unsigned int fmt, enum v4l2_field field, unsigned int num_plane)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format pixfmt;
    unsigned int framesize;

    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_BUF_TYPE;

    memset(&pixfmt, 0, sizeof(pixfmt));

    pixfmt.width = width;
    pixfmt.height = height;
    pixfmt.pixelformat = fmt;
    pixfmt.field = V4L2_FIELD_NONE;

    v4l2_fmt.fmt.pix = pixfmt;
    ALOGV("fimc_v4l2_s_fmt : width(%d) height(%d)", width, height);

    /* Set up for capture */
    if (ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt) < 0) {
        ALOGE("ERR(%s):VIDIOC_S_FMT failed", __func__);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_s_fmt_cap(int fp, int width, int height, unsigned int fmt)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format pixfmt;

    memset(&pixfmt, 0, sizeof(pixfmt));

    v4l2_fmt.type = V4L2_BUF_TYPE;

    pixfmt.width = width;
    pixfmt.height = height;
    pixfmt.pixelformat = fmt;
    if (fmt == V4L2_PIX_FMT_JPEG)
        pixfmt.colorspace = V4L2_COLORSPACE_JPEG;

    v4l2_fmt.fmt.pix = pixfmt;
    ALOGV("fimc_v4l2_s_fmt_cap : width(%d) height(%d)", width, height);

    /* Set up for capture */
    if (ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt) < 0) {
        ALOGE("ERR(%s):VIDIOC_S_FMT failed", __func__);
        return -1;
    }

    return 0;
}

int fimc_v4l2_s_fmt_is(int fp, int width, int height, unsigned int fmt, enum v4l2_field field)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format pixfmt;

    memset(&pixfmt, 0, sizeof(pixfmt));

    v4l2_fmt.type = V4L2_BUF_TYPE_PRIVATE;

    pixfmt.width = width;
    pixfmt.height = height;
    pixfmt.pixelformat = fmt;
    pixfmt.field = field;

    v4l2_fmt.fmt.pix = pixfmt;
    ALOGV("fimc_v4l2_s_fmt_is : width(%d) height(%d)", width, height);

    /* Set up for capture */
    if (ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt) < 0) {
        ALOGE("ERR(%s):VIDIOC_S_FMT failed", __func__);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_enum_fmt(int fp, unsigned int fmt)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    fmtdesc.type = V4L2_BUF_TYPE;
    fmtdesc.index = 0;

    while (ioctl(fp, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        if (fmtdesc.pixelformat == fmt) {
            ALOGV("passed fmt = %#x found pixel format[%d]: %s", fmt, fmtdesc.index, fmtdesc.description);
            found = 1;
            break;
        }

        fmtdesc.index++;
    }

    if (!found) {
        ALOGE("unsupported pixel format");
        return -1;
    }

    return 0;
}

static int fimc_v4l2_reqbufs(int fp, enum v4l2_buf_type type, int nr_bufs)
{
    struct v4l2_requestbuffers req;

    req.count = nr_bufs;
    req.type = type;
    req.memory = V4L2_MEMORY_TYPE;

    if (ioctl(fp, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("ERR(%s):VIDIOC_REQBUFS failed", __func__);
        return -1;
    }

    return req.count;
}

static int fimc_v4l2_querybuf(int fp, struct SecBuffer *buffers, enum v4l2_buf_type type, int nr_frames, int num_plane)
{
    struct v4l2_buffer v4l2_buf;
    int i, ret, plane_index;

    for (i = 0; i < nr_frames; i++) {
        v4l2_buf.type = type;
        v4l2_buf.memory = V4L2_MEMORY_TYPE;
        v4l2_buf.index = i;

        ret = ioctl(fp, VIDIOC_QUERYBUF, &v4l2_buf);
        if (ret < 0) {
            ALOGE("ERR(%s):VIDIOC_QUERYBUF failed", __func__);
            return -1;
        }

        buffers[i].size.s = v4l2_buf.length;

        if ((buffers[i].virt.p = (char *)mmap(0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fp, v4l2_buf.m.offset)) < 0) {
            ALOGE("%s %d] mmap() failed",__func__, __LINE__);
            return -1;
        }
        ALOGV("buffers[%d].virt.p = %p v4l2_buf.length = %d", i, buffers[i].virt.p, v4l2_buf.length);
    }
    return 0;
}

static int fimc_v4l2_streamon(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE;
    int ret;

    ret = ioctl(fp, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMON failed", __func__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_streamoff(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE;
    int ret;

    ALOGV("%s :", __func__);
    ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMOFF failed", __func__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_streamon_userptr(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;

    ret = ioctl(fp, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMON failed", __func__);
        return ret;
    }

    return ret;
}

static int fimc_v4l2_streamoff_userptr(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;

//    ALOGV("%s :", __func__);
    ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMOFF failed", __func__);
        return ret;
    }

    return ret;
}



static int fimc_v4l2_qbuf(int fp, int width, int height, struct SecBuffer *vaddr, int index, int num_plane, int mode)
{
    struct v4l2_buffer v4l2_buf;
    int ret;

    v4l2_buf.type = V4L2_BUF_TYPE;
    v4l2_buf.memory = V4L2_MEMORY_TYPE;
    v4l2_buf.index = index;

    ret = ioctl(fp, VIDIOC_QBUF, &v4l2_buf);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_QBUF failed", __func__);
        return ret;
    }

    return 0;
}

static int fimc_v4l2_qbuf_userptr(int fp, struct fimc_buf *fimc_buf, int index)
{
    struct v4l2_buffer buf;
    int ret;

    buf.length      = 0;
    buf.m.userptr   = (unsigned long)fimc_buf;
    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.index       = index;
    buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    ret = ioctl(fp, VIDIOC_QBUF, &buf);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_QBUF failed", __func__);
        return ret;
    }

    return 0;
}

static int fimc_v4l2_dqbuf(int fp, int num_plane)
{
    struct v4l2_buffer v4l2_buf;
    int ret;

    v4l2_buf.type = V4L2_BUF_TYPE;
    v4l2_buf.memory = V4L2_MEMORY_TYPE;

    ret = ioctl(fp, VIDIOC_DQBUF, &v4l2_buf);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_DQBUF failed, dropped frame", __func__);
        return ret;
    }

    return v4l2_buf.index;
}

static int fimc_v4l2_dqbuf_userptr(int fp)
{
    struct v4l2_buffer v4l2_buf;
    int ret;

    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    v4l2_buf.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(fp, VIDIOC_DQBUF, &v4l2_buf);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_DQBUF failed, dropped frame", __func__);
        return ret;
    }

    return v4l2_buf.index;
}

static int fimc_v4l2_g_ctrl(int fp, unsigned int id)
{
    struct v4l2_control ctrl;
    int ret;

    ctrl.id = id;

    ret = ioctl(fp, VIDIOC_G_CTRL, &ctrl);
    if (ret < 0) {
        ALOGE("ERR(%s): VIDIOC_G_CTRL(id = 0x%x (%d)) failed, ret = %d",
             __func__, id, id-V4L2_CID_PRIVATE_BASE, ret);
        return ret;
    }

    return ctrl.value;
}

static int fimc_v4l2_s_ctrl(int fp, unsigned int id, unsigned int value)
{
    struct v4l2_control ctrl;
    int ret;

    ctrl.id = id;
    ctrl.value = value;

    ret = ioctl(fp, VIDIOC_S_CTRL, &ctrl);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_CTRL(id = %#x (%d), value = %d) failed ret = %d",
             __func__, id, id-V4L2_CID_PRIVATE_BASE, value, ret);

        return ret;
    }

    return ctrl.value;
}

static int fimc_v4l2_s_ext_ctrl(int fp, unsigned int id, void *value)
{
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ctrl;
    int ret;

    ctrl.id = id;

    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ctrls.count = 1;
    ctrls.controls = &ctrl;

    ret = ioctl(fp, VIDIOC_S_EXT_CTRLS, &ctrls);
    if (ret < 0)
        ALOGE("ERR(%s):VIDIOC_S_EXT_CTRLS failed", __func__);

    return ret;
}

static int fimc_v4l2_s_ext_ctrl_face_detection(int fp, unsigned int id, void *value)
{
    struct v4l2_ext_control ext_ctrl_fd[111];
    struct v4l2_ext_controls ext_ctrls_fd;
    struct v4l2_ext_controls *ctrls;
    camera_frame_metadata_t *facedata = (camera_frame_metadata_t *)value;
    int i, ret;

    ext_ctrl_fd[0].id = V4L2_CID_IS_FD_GET_FACE_COUNT;
    for (i = 0; i < 5; i++) {
        ext_ctrl_fd[22*i+1].id = V4L2_CID_IS_FD_GET_FACE_FRAME_NUMBER;
        ext_ctrl_fd[22*i+2].id = V4L2_CID_IS_FD_GET_FACE_CONFIDENCE;
        ext_ctrl_fd[22*i+3].id = V4L2_CID_IS_FD_GET_FACE_SMILE_LEVEL;
        ext_ctrl_fd[22*i+4].id = V4L2_CID_IS_FD_GET_FACE_BLINK_LEVEL;
        ext_ctrl_fd[22*i+5].id = V4L2_CID_IS_FD_GET_FACE_TOPLEFT_X;
        ext_ctrl_fd[22*i+6].id = V4L2_CID_IS_FD_GET_FACE_TOPLEFT_Y;
        ext_ctrl_fd[22*i+7].id = V4L2_CID_IS_FD_GET_FACE_BOTTOMRIGHT_X;
        ext_ctrl_fd[22*i+8].id = V4L2_CID_IS_FD_GET_FACE_BOTTOMRIGHT_Y;
        ext_ctrl_fd[22*i+9].id = V4L2_CID_IS_FD_GET_LEFT_EYE_TOPLEFT_X;
        ext_ctrl_fd[22*i+10].id = V4L2_CID_IS_FD_GET_LEFT_EYE_TOPLEFT_Y;
        ext_ctrl_fd[22*i+11].id = V4L2_CID_IS_FD_GET_LEFT_EYE_BOTTOMRIGHT_X;
        ext_ctrl_fd[22*i+12].id = V4L2_CID_IS_FD_GET_LEFT_EYE_BOTTOMRIGHT_Y;
        ext_ctrl_fd[22*i+13].id = V4L2_CID_IS_FD_GET_RIGHT_EYE_TOPLEFT_X;
        ext_ctrl_fd[22*i+14].id = V4L2_CID_IS_FD_GET_RIGHT_EYE_TOPLEFT_Y;
        ext_ctrl_fd[22*i+15].id = V4L2_CID_IS_FD_GET_RIGHT_EYE_BOTTOMRIGHT_X;
        ext_ctrl_fd[22*i+16].id = V4L2_CID_IS_FD_GET_RIGHT_EYE_BOTTOMRIGHT_Y;
        ext_ctrl_fd[22*i+17].id = V4L2_CID_IS_FD_GET_MOUTH_TOPLEFT_X;
        ext_ctrl_fd[22*i+18].id = V4L2_CID_IS_FD_GET_MOUTH_TOPLEFT_Y;
        ext_ctrl_fd[22*i+19].id = V4L2_CID_IS_FD_GET_MOUTH_BOTTOMRIGHT_X;
        ext_ctrl_fd[22*i+20].id = V4L2_CID_IS_FD_GET_MOUTH_BOTTOMRIGHT_Y;
        ext_ctrl_fd[22*i+21].id = V4L2_CID_IS_FD_GET_ANGLE;
        ext_ctrl_fd[22*i+22].id = V4L2_CID_IS_FD_GET_NEXT;
    }

    ext_ctrls_fd.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ext_ctrls_fd.count = 111;
    ext_ctrls_fd.controls = ext_ctrl_fd;
    ctrls = &ext_ctrls_fd;

    ret = ioctl(fp, VIDIOC_G_EXT_CTRLS, &ext_ctrls_fd);

    facedata->number_of_faces = ext_ctrls_fd.controls[0].value;

    for(i = 0; i < facedata->number_of_faces; i++) {
        facedata->faces[i].rect[0]      = ext_ctrl_fd[22*i+5].value;
        facedata->faces[i].rect[1]      = ext_ctrl_fd[22*i+6].value;
        facedata->faces[i].rect[2]      = ext_ctrl_fd[22*i+7].value;
        facedata->faces[i].rect[3]      = ext_ctrl_fd[22*i+8].value;
        facedata->faces[i].score        = ext_ctrl_fd[22*i+2].value;
/* TODO : id is unique value for each face. We need to suppot this. */
        facedata->faces[i].id           = 0;
        facedata->faces[i].left_eye[0]  = (ext_ctrl_fd[22*i+9].value + ext_ctrl_fd[22*i+11].value) / 2;
        facedata->faces[i].left_eye[1]  = (ext_ctrl_fd[22*i+10].value + ext_ctrl_fd[22*i+12].value) / 2;
        facedata->faces[i].right_eye[0] = (ext_ctrl_fd[22*i+13].value + ext_ctrl_fd[22*i+15].value) / 2;
        facedata->faces[i].right_eye[1] = (ext_ctrl_fd[22*i+14].value + ext_ctrl_fd[22*i+16].value) / 2;
        facedata->faces[i].mouth[0]     = (ext_ctrl_fd[22*i+17].value + ext_ctrl_fd[22*i+19].value) / 2;
        facedata->faces[i].mouth[1]     = (ext_ctrl_fd[22*i+18].value + ext_ctrl_fd[22*i+20].value) / 2;
    }

    return ret;
}

static int fimc_v4l2_g_parm(int fp, struct v4l2_streamparm *streamparm)
{
    int ret;

    streamparm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fp, VIDIOC_G_PARM, streamparm);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_G_PARM failed", __func__);
        return -1;
    }

    ALOGV("%s : timeperframe: numerator %d, denominator %d", __func__,
            streamparm->parm.capture.timeperframe.numerator,
            streamparm->parm.capture.timeperframe.denominator);

    return 0;
}

static int fimc_v4l2_s_parm(int fp, struct v4l2_streamparm *streamparm)
{
    int ret;

    streamparm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fp, VIDIOC_S_PARM, streamparm);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_PARM failed", __func__);
        return ret;
    }

    return 0;
}

SecCamera::SecCamera() :
            m_flagCreate(0),
            m_preview_state(0),
            m_snapshot_state(0),
            m_camera_id(CAMERA_ID_BACK),
            m_camera_use_ISP(0),
            m_cam_fd(-1),
            m_cam_fd2(-1),
            m_cam_fd3(-1),
            m_prev_fd(-1),
            m_prev_mapped_addr(NULL),
            m_rec_mapped_addr(NULL),
            m_cap_mapped_addr(NULL),
            m_exynos_mem_fd_prev(0),
            m_exynos_mem_fd_rec(0),
            m_exynos_mem_fd_snap(0),
            m_cap_fd(-1),
            m_rec_fd(-1),
            m_jpeg_fd(-1),
#ifdef IS_FW_DEBUG
            m_mem_fd(-1),
#endif
            m_flag_record_start(0),
            m_preview_v4lformat(V4L2_PIX_FMT_YVU420),
            m_preview_width      (0),
            m_preview_height     (0),
            m_preview_max_width  (MAX_BACK_CAMERA_PREVIEW_WIDTH),
            m_preview_max_height (MAX_BACK_CAMERA_PREVIEW_HEIGHT),
            m_snapshot_v4lformat(V4L2_PIX_FMT_NV16),
            m_snapshot_width      (0),
            m_snapshot_height     (0),
            m_num_capbuf          (0),
            m_sensor_width (0),
            m_sensor_height(0),
            m_snapshot_max_width  (MAX_BACK_CAMERA_SNAPSHOT_WIDTH),
            m_snapshot_max_height (MAX_BACK_CAMERA_SNAPSHOT_HEIGHT),
            m_recording_en        (0),
            m_record_hint         (0),
            m_recording_width     (1280),
            m_recording_height    (720),
            m_angle(-1),
            m_anti_banding(0),
            m_wdr(0),
            m_anti_shake(0),
            m_zoom_level(0),
            m_object_tracking(-1),
            m_smart_auto(-1),
            m_beauty_shot(-1),
            m_vintage_mode(-1),
            m_face_detect(0),
            m_object_tracking_start_stop(-1),
            m_gps_latitude(-1),
            m_gps_longitude(-1),
            m_gps_altitude(-1),
            m_gps_timestamp(-1),
            m_sensor_mode(-1),
            m_shot_mode(-1),
            m_exif_orientation(-1),
            m_chk_dataline(-1),
            m_video_gamma(0),
            m_slow_ae(0),
            m_camera_af_flag(-1),
            m_flag_camera_start(0),
            m_jpeg_thumbnail_width (0),
            m_jpeg_thumbnail_height(0),
            m_jpeg_thumbnail_quality(100),
            m_jpeg_quality(100),
            m_touch_af_start_stop(-1),
            m_postview_offset(0),
            m_auto_focus_state(0),
            m_isTouchMetering(false),
            m_snapshot_phys_addr(0)
#ifdef ENABLE_ESD_PREVIEW_CHECK
            ,
            m_esd_check_count(0)
#endif // ENABLE_ESD_PREVIEW_CHECK
{
    initParameters(0);
    memset(&mExifInfo, 0, sizeof(mExifInfo));

    memset(&m_events_c, 0, sizeof(m_events_c));
    memset(&m_events_c2, 0, sizeof(m_events_c2));
    memset(&m_events_c3, 0, sizeof(m_events_c3));
}

SecCamera::~SecCamera()
{
    ALOGV("%s :", __func__);
    DestroyCamera();
}

bool SecCamera::CreateCamera(int index)
{
    ALOGV("%s :", __func__);
    int ret = 0;

    if (!m_flagCreate) {
        /* Arun C
         * Reset the lense position only during camera starts; don't do
         * reset between shot to shot
         */
        m_flagCreate = 1;
        m_snapshot_state = 0;
        m_camera_af_flag = -1;
        m_camera_id = index;
        m_recording_en = 0;

        /* FIMC0 open */
        ret = createFimc(&m_cam_fd, CAMERA_DEV_NAME, V4L2_BUF_TYPE_VIDEO_CAPTURE, index);
        CHECK(ret);

        m_camera_use_ISP = getUseInternalISP();

        if (setMaxSize() == false) {
            ALOGE("ERR(%s) Not supported sensor", __func__);
            return  -1;
        }

        if (m_camera_use_ISP) {
            if (!m_recording_en)
                fimc_v4l2_s_fmt_is(m_cam_fd, m_preview_max_width, m_preview_max_height,
                        m_preview_v4lformat, (enum v4l2_field) IS_MODE_PREVIEW_STILL);
            else
                fimc_v4l2_s_fmt_is(m_cam_fd, m_preview_max_width, m_preview_max_height,
                        m_preview_v4lformat, (enum v4l2_field) IS_MODE_PREVIEW_VIDEO);
        }

        ret = fimc_v4l2_s_fmt(m_cam_fd, m_preview_max_width, m_preview_max_height,
                      m_preview_v4lformat, V4L2_FIELD_ANY, PREVIEW_NUM_PLANE);
        CHECK(ret);

        initParameters(m_camera_use_ISP);

        if (m_camera_use_ISP) {
            /* FIMC2 open for recording and zsl, video snapshot m2m */
            ret = createFimc(&m_cam_fd3, CAMERA_DEV_NAME2, V4L2_BUF_TYPE_VIDEO_OUTPUT, index);
            CHECK(ret);

            /* FIMC1 open for preview m2m */
            ret = createFimc(&m_cam_fd2, CAMERA_DEV_NAME3, V4L2_BUF_TYPE_VIDEO_OUTPUT, index);
            CHECK(ret);
        } else {
            /* FIMC1 open */
            ret = createFimc(&m_cam_fd2, CAMERA_DEV_NAME3, V4L2_BUF_TYPE_VIDEO_CAPTURE, index);
            CHECK(ret);
        }

        setExifFixedAttribute();

        if (m_camera_use_ISP) {
            m_prev_fd = m_cam_fd2;
            m_cap_fd = m_cam_fd3;
            m_rec_fd = m_cam_fd3;
            m_num_capbuf = CAP_BUFFERS;
        } else {
            m_prev_fd = m_cam_fd;
            m_cap_fd = m_cam_fd;
            m_rec_fd = m_cam_fd2;
            m_num_capbuf = 1;
        }
    }

    return 0;
}

int SecCamera::createFimc(int *fp, char *dev_name, int mode, int index)
{
    struct v4l2_format fmt;
    int ret = 0;

    *fp = open(dev_name, O_RDWR);
    if (fp < 0) {
        ALOGE("ERR(%s):Cannot open %s (error : %s)", __func__, dev_name, strerror(errno));
        return -1;
    }
    ALOGV("%s: open(%s) --> fp %d", __func__, dev_name, *fp);

    if (mode == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        ret = fimc_v4l2_querycap(*fp);
        CHECK(ret);

        if (!fimc_v4l2_enuminput(*fp, index)) {
            ALOGE("m_cam_fd(%d) fimc_v4l2_enuminput fail", *fp);
            return -1;
        }

        ret = fimc_v4l2_s_input(*fp, index);
        CHECK(ret);
    } else if (mode == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        ret = fimc_v4l2_querycap_m2m(*fp);
        CHECK(ret);

        /* malloc fimc_outinfo structure */
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl(*fp, VIDIOC_G_FMT, &fmt) < 0) {
            ALOGE("%s::Error in video VIDIOC_G_FMT", __func__);
            return -1;
        }
    }

    return ret;
}

void SecCamera::resetCamera()
{
    ALOGV("%s :", __func__);
    DestroyCamera();
    CreateCamera(m_camera_id);
}

bool SecCamera::DestroyCamera()
{
    ALOGV("%s :", __func__);

    if (m_flagCreate) {

        stopRecord();

        /* unmap all FIMC's reserved memory */
        if (m_prev_mapped_addr != NULL) {
            munmap(m_prev_mapped_addr, DEV_NAME3_RESERVED_SIZE * 1024);
            m_prev_mapped_addr = NULL;
        }

        if (m_cap_mapped_addr != NULL) {
            munmap(m_cap_mapped_addr, DEV_NAME2_RESERVED_SIZE * 1024);
            m_cap_mapped_addr = NULL;
        }

        if (m_rec_mapped_addr != NULL) {
            munmap(m_rec_mapped_addr, DEV_NAME2_RESERVED_SIZE * 1024);
            m_rec_mapped_addr = NULL;
        }

        /* close m_cam_fd after stopRecord() because stopRecord()
         * uses m_cam_fd to change frame rate
         */
        ALOGI("DestroyCamera: m_cam_fd(%d)", m_cam_fd);
        if (m_cam_fd > -1) {
            if (close(m_cam_fd) < 0)
                ALOGE("++ fail close fd %d", m_cam_fd);
            m_cam_fd = -1;
        }

        ALOGI("DestroyCamera: m_cam_fd2(%d)", m_cam_fd2);
        if (m_cam_fd2 > -1) {
            if (close(m_cam_fd2) < 0)
                ALOGE("++ fail close fd %d", m_cam_fd2);
            m_cam_fd2 = -1;
        }

        ALOGI("DestroyCamera: m_cam_fd3(%d)", m_cam_fd3);
        if (m_cam_fd3 > -1) {
            if (close(m_cam_fd3) < 0)
                ALOGE("++ fail close fd %d", m_cam_fd3);
            m_cam_fd3 = -1;
        }

#ifdef IS_FW_DEBUG
        if (m_camera_use_ISP) {
            munmap((void *)m_debug_paddr, FIMC_IS_FW_DEBUG_REGION_SIZE);
            ALOGI("Destroy exynos-mem: m_mem_fd(%d)", m_mem_fd);
            if (m_mem_fd > -1) {
                close(m_mem_fd);
                m_mem_fd = -1;
            }
        }
#endif

        m_flagCreate = 0;
    } else
        ALOGI("%s : already deinitialized", __func__);

    return 0;
}

void SecCamera::initParameters(int internalISP)
{
    memset(&m_streamparm, 0, sizeof(m_streamparm));
    m_params = (struct sec_cam_parm*)&m_streamparm.parm.raw_data;
    struct v4l2_captureparm capture;

    m_params->capture.timeperframe.numerator = 1;
    m_params->capture.timeperframe.denominator = FRAME_RATE_AUTO;
    m_params->flash_mode = FLASH_MODE_AUTO;
    m_params->iso = ISO_AUTO;
    m_params->saturation = SATURATION_DEFAULT;
    m_params->scene_mode = SCENE_MODE_NONE;
    m_params->sharpness = SHARPNESS_DEFAULT;
    m_params->white_balance = WHITE_BALANCE_AUTO;
    m_params->anti_banding = ANTI_BANDING_OFF;
    m_params->effects = IMAGE_EFFECT_NONE;
    m_params->focus_mode = FOCUS_MODE_AUTO;

    if (internalISP) {
        m_params->contrast = IS_CONTRAST_DEFAULT;
        m_params->brightness = IS_BRIGHTNESS_DEFAULT;
        m_params->exposure = IS_EXPOSURE_DEFAULT;
        m_params->metering = IS_METERING_AVERAGE;
        m_params->hue = IS_HUE_DEFAULT;
        m_params->aeawb_mode = AE_UNLOCK_AWB_UNLOCK;
    } else {
        m_params->contrast = CONTRAST_DEFAULT;
        m_params->brightness = EV_DEFAULT;
        m_params->exposure = EV_DEFAULT;
        m_params->metering = METERING_MATRIX;
        m_params->hue = -1;
        m_params->aeawb_mode = -1;
    }
}

int SecCamera::setMode(int recording_en)
{
    ALOGV("%s :", __func__);
    int mode;

    m_recording_en  = recording_en;

    if (m_camera_use_ISP) {
        if (!recording_en)
            mode = IS_MODE_PREVIEW_STILL;
        else
            mode = IS_MODE_PREVIEW_VIDEO;

        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_S_FORMAT_SCENARIO, mode) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_IS_S_FORMAT_SCENARIO", __func__);
            return -1;
        }
    }

    return 0;
}

int SecCamera::getCameraFd(enum CAM_MODE mode)
{
    int ret = -1;

    switch (mode) {
    case PREVIEW:
        ret = m_prev_fd;
        break;
    case PICTURE:
        ret = m_cap_fd;
        break;
    default:
        ret = m_prev_fd;
        break;
    }

    return ret;
}

int SecCamera::startPreview(void)
{
    v4l2_streamparm streamparm;
    struct sec_cam_parm *parms;
    parms = (struct sec_cam_parm*)&streamparm.parm.raw_data;
    ALOGV("%s :", __func__);
    int ret;

    // aleady started
    if (m_flag_camera_start > 0) {
        ALOGE("ERR(%s):Preview was already started", __func__);
        return 0;
    }

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    ret = setFimc();
    CHECK(ret);

    ret = fimc_v4l2_streamon(m_cam_fd);
    CHECK(ret);

    if (m_camera_use_ISP) {
        ret = setFimcForPreview();
        CHECK(ret);
    }

#ifdef USE_FACE_DETECTION
    if (m_camera_use_ISP) {
        ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CMD_FD, IS_FD_COMMAND_START);
        CHECK(ret);
    }
#endif

#ifdef ZERO_SHUTTER_LAG
    if (m_camera_use_ISP)
        setFimcForSnapshot();
#endif

    m_flag_camera_start = 1;

    ALOGV("%s: got the first frame of the preview", __func__);

    return 0;
}

int SecCamera::setFimc(void)
{
    ALOGV("%s", __func__);
    int ret;

    memset(&m_events_c, 0, sizeof(m_events_c));
    m_events_c.fd = m_cam_fd;
    m_events_c.events = POLLIN | POLLERR;

    if (m_camera_use_ISP) {
        ALOGV("m_camera_use_ISP(%d), %s", m_camera_use_ISP, (const char*)getCameraSensorName());

        /* enum_fmt to sanpshot fmt, currently YUYV */
        int ret = fimc_v4l2_enum_fmt(m_cam_fd, m_snapshot_v4lformat);
        CHECK(ret);

        /* set ISP output fmt and fimc input fmt */
        if (!m_recording_en) {
            fimc_v4l2_s_fmt_is(m_cam_fd, m_sensor_width, m_sensor_height,
                    m_snapshot_v4lformat, (enum v4l2_field) IS_MODE_PREVIEW_STILL);
        } else {
            fimc_v4l2_s_fmt_is(m_cam_fd, m_sensor_width, m_sensor_height,
                    m_snapshot_v4lformat, (enum v4l2_field) IS_MODE_PREVIEW_VIDEO);
        }

        /* s_fmt to max capture size and capture fmt */
        ret = fimc_v4l2_s_fmt(m_cam_fd, m_snapshot_width, m_snapshot_height,
                    m_snapshot_v4lformat, V4L2_FIELD_ANY, PREVIEW_NUM_PLANE);
        CHECK(ret);

        /* set scenario mode */
        if (!m_recording_en) {
            ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_S_SCENARIO_MODE, IS_MODE_PREVIEW_STILL);
            CHECK(ret);
        } else {
            ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_S_SCENARIO_MODE, IS_MODE_PREVIEW_VIDEO);
            CHECK(ret);
        }
    } else {
        /* enum_fmt to preview fmt */
        int ret = fimc_v4l2_enum_fmt(m_cam_fd, m_preview_v4lformat);
        CHECK(ret);

        /* s_fmt to preview size and fmt */
        ret = fimc_v4l2_s_fmt(m_cam_fd, m_preview_width, m_preview_height,
                    m_preview_v4lformat, V4L2_FIELD_ANY, PREVIEW_NUM_PLANE);
        CHECK(ret);

        /* set ISP output fmt and fimc input fmt */
        fimc_v4l2_s_fmt_is(m_cam_fd, m_preview_width, m_preview_height,
                m_preview_v4lformat, (enum v4l2_field) IS_MODE_PREVIEW_STILL);
    }

    ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CACHEABLE, 1);
    CHECK(ret);

    ret = fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE, MAX_BUFFERS);
    CHECK(ret);

    ret = fimc_v4l2_querybuf(m_cam_fd, m_buffers_share, V4L2_BUF_TYPE, MAX_BUFFERS, PREVIEW_NUM_PLANE);
    CHECK(ret);

    ALOGV("%s : m_preview_width: %d m_preview_height: %d m_angle: %d",
            __func__, m_preview_width, m_preview_height, m_angle);

    ALOGV("m_camera_id : %d", m_camera_id);

    /* start with all buffers in queue
     * TODO: m_preview_width and height are only used V4L2_ION
     *       currently this codes possibly does not work in V4L2_ION
     */
    for (int i = 0; i < MAX_BUFFERS; i++) {
        ret = fimc_v4l2_qbuf(m_cam_fd, m_preview_width, m_preview_height, m_buffers_share, i, PREVIEW_NUM_PLANE, PREVIEW_MODE);
        CHECK(ret);
    }

    return 0;
}

int SecCamera::openExynosMemDev(int *fp)
{
    ALOGV("%s", __func__);

    if (*fp == 0) {
        *fp = open(DEV_EXYNOS_MEM, O_RDWR);
        if (*fp < 0) {
            ALOGE("ERR(%s): %s exynos-mem opne fail\n", __func__, DEV_EXYNOS_MEM);
            return -1;
        }
    }

    return 0;
}

int SecCamera::setFimcForPreview(void)
{
    int ret = 0;
    unsigned int paddr = 0;
    struct v4l2_control vc;

    ALOGV("%s", __func__);

    ret = openExynosMemDev(&m_exynos_mem_fd_prev);
    CHECK(ret);

    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;
    ret = ioctl(m_prev_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Err(%s): VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)", __func__, ret);
        return false;
    }
    paddr = (unsigned int)vc.value;

    size_t size = DEV_NAME3_RESERVED_SIZE * 1024;

    void *mappedAddr;

    if (m_prev_mapped_addr == NULL) {
        mappedAddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, m_exynos_mem_fd_prev, paddr);
        m_prev_mapped_addr = mappedAddr;
    }

    ret = setFimcDst(m_prev_fd, m_preview_width, m_preview_height, m_preview_v4lformat, (unsigned int)paddr);
    CHECK(ret);

    return 0;
}

int SecCamera::setFimcForRecord()
{
    int ret = 0;
    unsigned int paddr = 0;
    struct v4l2_control vc;

    ALOGV("%s", __func__);

    ret = openExynosMemDev(&m_exynos_mem_fd_rec);
    CHECK(ret);

    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;
    ret = ioctl(m_rec_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Err(%s): VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)", __func__, ret);
        return false;
    }
    paddr = (unsigned int)vc.value;

    size_t size = DEV_NAME2_RESERVED_SIZE * 1024;

    void *mappedAddr;

    if(m_rec_mapped_addr == NULL) {
        mappedAddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, m_exynos_mem_fd_rec, paddr);
        m_rec_mapped_addr = mappedAddr;
    }

    int ySize = (ALIGN(ALIGN(m_recording_width, 16) * ALIGN(m_recording_height, 16), 4096));
    int uvSize = (ALIGN(ALIGN(m_recording_width, 16) * ALIGN(m_recording_height >> 1, 8), 4096));
    int frame_size = ySize + uvSize;

    // set addr
    for (int i = 0; i < 8; i++) {
        /* For 4k align */
        m_buffers_record[i].virt.extP[0] = (char *)m_rec_mapped_addr + (frame_size * i);
        m_buffers_record[i].virt.extP[1] = m_buffers_record[i].virt.extP[0] + ySize;
        m_buffers_record[i].phys.extP[0] = (unsigned int)(paddr + (frame_size * i));
        m_buffers_record[i].phys.extP[1] = m_buffers_record[i].phys.extP[0] + ySize;
        m_buffers_record[i].size.extS[0] = frame_size;
    }

    m_flag_record_start = 1;

    return 0;
}

int SecCamera::setFimcForSnapshot()
{
    int ret = 0;
    unsigned int paddr = 0;
    struct v4l2_control vc;

    ALOGV("%s", __func__);

    ret = openExynosMemDev(&m_exynos_mem_fd_snap);
    CHECK(ret);

    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;
    ret = ioctl(m_cap_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Err(%s): VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)", __func__, ret);
        return false;
    }
    paddr = (unsigned int)vc.value;
    m_snapshot_phys_addr = paddr + (DEV_NAME2_RESERVED_SIZE * 1024);

    size_t size = VIDEO_SNAPSHOT_RESERVED_SIZE * 1024;

    void *mappedAddr;
    if (m_cap_mapped_addr == NULL) {
        mappedAddr = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, m_exynos_mem_fd_snap, m_snapshot_phys_addr);
        m_cap_mapped_addr = mappedAddr;
    }

    if (!m_recording_en) {
        ret = setFimcDst(m_cap_fd, m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat, m_snapshot_phys_addr);
        CHECK(ret);
    }

    return 0;
}

char *SecCamera::getMappedAddr(void)
{
    return (char *)m_prev_mapped_addr;
}

int SecCamera::runPreviewFimcOneshot(int index, camera_frame_metadata_t *facedata)
{
    int ret = 0;
    struct fimc_buf src_buf;
    struct v4l2_control vc;
    unsigned int paddr;

    ALOGV("%s", __func__);

    ret = getShareBufferAddr(index, &src_buf);

    ret = setFimcSrc(m_prev_fd, m_snapshot_width, m_snapshot_height, facedata);
    CHECK(ret);

    exynos_mem_flush_range mem;
    mem.start = src_buf.base[0];
    mem.length = m_snapshot_width * m_snapshot_height * 2;

    ret = ioctl(m_exynos_mem_fd_prev, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
    if (ret < 0) {
        ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
        return false;
    }

    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;
    ret = ioctl(m_prev_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Err(%s): VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)", __func__, ret);
        return false;
    }
    paddr = (unsigned int)vc.value;

    mem.start = paddr;
    mem.length = m_preview_width * m_preview_height * 3 / 2;

    ret = ioctl(m_exynos_mem_fd_prev, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
    if (ret < 0) {
        ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
        return false;
    }

    ret = fimc_v4l2_streamon_userptr(m_prev_fd);
    CHECK(ret);

    ret = fimc_v4l2_qbuf_userptr(m_prev_fd, &src_buf, 0);
    if (ret < 0) {
        ALOGE("ERR(%s): fd(%d) qbuf fail", __func__, m_prev_fd);

        ret = fimc_v4l2_streamoff_userptr(m_prev_fd);
        CHECK(ret);

        ret = clearFimcBuf(m_prev_fd);
        CHECK(ret);

        return -1;
    }

    ret = fimc_v4l2_dqbuf_userptr(m_prev_fd);
    CHECK(ret);

    ret = fimc_v4l2_streamoff_userptr(m_prev_fd);
    CHECK(ret);

    ret = clearFimcBuf(m_prev_fd);
    CHECK(ret);

    return 0;
}

int SecCamera::runRecordFimcOneshot(int index)
{
    int ret = 0;
    struct fimc_buf src_buf;
    char *paddr;

    ALOGV("%s", __func__);

    int ySize = m_recording_width * m_recording_height;
    int uvSize = m_recording_width * m_recording_height / 2;

    ret = getShareBufferAddr(index, &src_buf);

    ret = setFimcSrc(m_rec_fd, m_snapshot_width, m_snapshot_height, NULL);
    CHECK(ret);

    paddr = (char *)m_buffers_record[index].phys.extP[0];

    ret = setFimcDst(m_rec_fd, ALIGN(m_recording_width, 16), ALIGN(m_recording_height, 16), V4L2_PIX_FMT_NV12M, (unsigned int)paddr);
    CHECK(ret);

    ret = fimc_v4l2_streamon_userptr(m_rec_fd);
    CHECK(ret);

    exynos_mem_flush_range mem;
    mem.start = src_buf.base[0];
    mem.length = m_snapshot_width * m_snapshot_height * 2;

    ret = ioctl(m_exynos_mem_fd_rec, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
    if (ret < 0) {
        ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
        return false;
    }

    mem.start = (unsigned int)paddr;
    mem.length = ySize + uvSize;

    ret = ioctl(m_exynos_mem_fd_rec, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
    if (ret < 0) {
        ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
        return false;
    }

    ret = fimc_v4l2_qbuf_userptr(m_rec_fd, &src_buf, 0);
    if (ret < 0) {
        ALOGE("ERR(%s): fd(%d) qbuf fail", __func__, m_rec_fd);

        ret = fimc_v4l2_streamoff_userptr(m_rec_fd);
        CHECK(ret);

        ret = clearFimcBuf(m_rec_fd);
        CHECK(ret);

        return -1;
    }

    ret = fimc_v4l2_dqbuf_userptr(m_rec_fd);
    CHECK(ret);

    ret = fimc_v4l2_streamoff_userptr(m_rec_fd);
    CHECK(ret);

    ret = clearFimcBuf(m_rec_fd);
    CHECK(ret);

    return 0;
}

int SecCamera::runSnapshotFimcOneshot(int index)
{
    int ret = 0;
    struct fimc_buf src_buf;

    ALOGV("%s", __func__);

    ret = getShareBufferAddr(index, &src_buf);

    if (m_flag_record_start == 0) {
        /* H/W scaler - FIMC */
        ret = setFimcSrc(m_cap_fd, m_snapshot_width, m_snapshot_height, NULL);
        CHECK(ret);

        ret = fimc_v4l2_streamon_userptr(m_cap_fd);
        CHECK(ret);

        exynos_mem_flush_range mem;
        mem.start = src_buf.base[0];
        mem.length = m_snapshot_width * m_snapshot_height * 2;

        ret = ioctl(m_exynos_mem_fd_snap, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
        if (ret < 0) {
            ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
            return false;
        }

        ret = fimc_v4l2_qbuf_userptr(m_cap_fd, &src_buf, 0);
        if (ret < 0) {
            ALOGE("ERR(%s): fd(%d) qbuf fail", __func__, m_cap_fd);

            ret = fimc_v4l2_streamoff_userptr(m_cap_fd);
            CHECK(ret);

            ret = clearFimcBuf(m_cap_fd);
            CHECK(ret);

            return -1;
        }

        ret = fimc_v4l2_dqbuf_userptr(m_cap_fd);
        CHECK(ret);

        ret = fimc_v4l2_streamoff_userptr(m_cap_fd);
        CHECK(ret);

        ret = clearFimcBuf(m_cap_fd);
        CHECK(ret);
    } else {
        /* S/W scaler - NEON */
        unsigned char  *src_y_addr;
        unsigned char  *src_cbcr_addr;
        unsigned char  *dst_y_addr;
        unsigned char  *dst_cbcr_addr;

        int pictureSize = m_snapshot_width * m_snapshot_height;

        /* crop input size
         * '*3/4' : 4x zoom
         * '/31' : 31 steps
         */
        int step = 31;
        float zoom[step];
        float inc = 0.1;
        zoom[0] = 1.0;
        for (int n = 0; n < (step - 1); n++) {
            zoom[n+1] = zoom[n] + inc;
        }

        int src_width    = (int)((float)m_snapshot_width / zoom[m_zoom_level]);
        src_width = ALIGN(src_width, 4);

        int src_height   = (int)((float)m_snapshot_height / zoom[m_zoom_level]);
        src_height = ALIGN(src_height, 4);

        int dst_width    = m_snapshot_width;
        int dst_height   = m_snapshot_height;

        int src_left    = (dst_width - src_width) / 2;
        int src_top     = (dst_height - src_height) / 2;

        int offset = (dst_width * src_top) + src_left;

        src_y_addr = (unsigned char *)(m_buffers_share[index].virt.p + offset);
        src_cbcr_addr = (unsigned char *)(src_y_addr + ALIGN(pictureSize, SIZE_4K));

        dst_y_addr = (unsigned char *)m_cap_mapped_addr;
        dst_cbcr_addr = (unsigned char *)(dst_y_addr + pictureSize);

        SW_Scale_up_crop(src_width, src_height,
                         dst_width, dst_height,
                         src_y_addr, src_cbcr_addr,
                         dst_y_addr, dst_cbcr_addr);
    }

    return 0;
}

int SecCamera::setFimcSrc(int fd, int width, int height, camera_frame_metadata_t *facedata)
{
    ALOGV("%s", __func__);
    struct v4l2_format  fmt;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;
    int croppedFaceInfo[CAMERA_MAX_FACES];
    int num_croppedFace = 0;

    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = m_snapshot_v4lformat;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_S_FMT failed : errno=%d (%s)"
                " : fd=%d\n", __func__, errno, strerror(errno), fd);
        return -1;
    }

    /* crop input size
     * '*3/4' : 4x zoom
     * '/31' : 31 steps
     */
    int step = 31;
    float zoom[step];
    float inc = 0.1;
    zoom[0] = 1.0;
    for (int n = 0; n < (step - 1); n++) {
        zoom[n+1] = zoom[n] + inc;
    }

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    crop.c.width    = (int)((float)width / zoom[m_zoom_level]);
    crop.c.height   = (int)((float)height / zoom[m_zoom_level]);

    if (crop.c.width % 2)
        crop.c.width -= 1;

    if (crop.c.height % 2)
        crop.c.height -= 1;

    crop.c.left     = (width - crop.c.width) / 2;
    crop.c.top      = (height - crop.c.height) / 2;

    if (crop.c.left % 2)
        crop.c.left -= 1;

    if (crop.c.top % 2)
        crop.c.top -= 1;

    int facerect[4];

    /* calculate face detection rectangle */
    /* rotate original face : left->top, top->right, right->bottom, bottom->left */
    if (facedata != NULL && facedata->number_of_faces != 0) {
        int temp[4];

        float w_ratio = ((float)2000 / (float)width);

        temp[0] = (int)((float)crop.c.left) * w_ratio;  //left
        temp[1] = (int)((float)crop.c.top) * w_ratio;   //top
        temp[2] = (int)(2000 - temp[0]);                //right
        temp[3] = (int)(2000 - temp[1]);                //bottom

        facerect[0] = temp[1];
        facerect[1] = temp[2];
        facerect[2] = temp[3];
        facerect[3] = temp[0];

        /* check the face in croped rect */
        for(int i = 0; i < facedata->number_of_faces; i++) {
            ALOGV("orignal facedata %d %d %d %d", facedata->faces[i].rect[0],
                                facedata->faces[i].rect[1],
                                facedata->faces[i].rect[2],
                                facedata->faces[i].rect[3]);

            ALOGV("rotated facedata %d %d %d %d", facedata->faces[i].rect[0] + 1000,
                                (0 - (facedata->faces[i].rect[1] - 1000)),
                                facedata->faces[i].rect[2] + 1000,
                                (0 - (facedata->faces[i].rect[3] - 1000)));

            if (((facedata->faces[i].rect[0] + 1000) < facerect[0])
                    || ((0 - (facedata->faces[i].rect[1] - 1000)) > facerect[1])
                    || ((facedata->faces[i].rect[2] + 1000) > facerect[2])
                    || ((0 - (facedata->faces[i].rect[3] - 1000)) < facerect[3])) {

                croppedFaceInfo[i] = 0;
            } else {
                croppedFaceInfo[i] = 1;
                num_croppedFace++;
            }
        }

        /* update facedata */
        int croppedIndex = 0;
        facedata->number_of_faces = num_croppedFace;
        if (facedata->number_of_faces == 0) {
            for (int i = 0; i < CAMERA_MAX_FACES; i++) {
                facedata->faces[i].rect[0]      = 0;
                facedata->faces[i].rect[1]      = 0;
                facedata->faces[i].rect[2]      = 0;
                facedata->faces[i].rect[3]      = 0;
                facedata->faces[i].score        = 0;
                facedata->faces[i].id           = 0;
                facedata->faces[i].left_eye[0]  = 0;
                facedata->faces[i].left_eye[1]  = 0;
                facedata->faces[i].right_eye[0] = 0;
                facedata->faces[i].right_eye[1] = 0;
                facedata->faces[i].mouth[0]     = 0;
                facedata->faces[i].mouth[1]     = 0;
            }
        }
        for(int i = 0; i < facedata->number_of_faces; i++) {
            for (int j = 0; j < CAMERA_MAX_FACES; j++) {
                if (croppedFaceInfo[j] == 1) {
                    croppedIndex = j;
                    croppedFaceInfo[j] = 0;
                    break;
                }
            }
            facedata->faces[i].rect[0]      = (int)((float)facedata->faces[croppedIndex].rect[0] * zoom[m_zoom_level]);
            facedata->faces[i].rect[1]      = (int)((float)facedata->faces[croppedIndex].rect[1] * zoom[m_zoom_level]);
            facedata->faces[i].rect[2]      = (int)((float)facedata->faces[croppedIndex].rect[2] * zoom[m_zoom_level]);
            facedata->faces[i].rect[3]      = (int)((float)facedata->faces[croppedIndex].rect[3] * zoom[m_zoom_level]);
            facedata->faces[i].score        = facedata->faces[croppedIndex].score;
            facedata->faces[i].id           = 0;
            facedata->faces[i].left_eye[0]  = facedata->faces[croppedIndex].left_eye[0];
            facedata->faces[i].left_eye[1]  = facedata->faces[croppedIndex].left_eye[1];
            facedata->faces[i].right_eye[0] = facedata->faces[croppedIndex].right_eye[0];
            facedata->faces[i].right_eye[1] = facedata->faces[croppedIndex].right_eye[1];
            facedata->faces[i].mouth[0]     = facedata->faces[croppedIndex].mouth[0];
            facedata->faces[i].mouth[1]     = facedata->faces[croppedIndex].mouth[1];
        }
    }

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        ALOGE("%s::Error in video VIDIOC_S_CROP :"
                "crop.c.left : (%d), crop.c.top : (%d), crop.c.width : (%d), crop.c.height : (%d)",
                __func__, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        return -1;
    }

    /* input buffer type */
    req.count       = 1;
    req.memory      = V4L2_MEMORY_USERPTR;
    req.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("%s::Error in VIDIOC_REQBUFS", __func__);
        return -1;
    }

    return 0;
}

int SecCamera::setFimcDst(int fd, int width, int height, int pix_fmt, unsigned int addr)
{
    struct v4l2_format      sFormat;
    struct v4l2_control     vc;
    struct v4l2_framebuffer fbuf;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;
    int ret;

    ALOGV("%s", __func__);

    /* set size, format & address for destination image (DMA-OUTPUT) */
    ret = ioctl(fd, VIDIOC_G_FBUF, &fbuf);
    if (ret < 0) {
        ALOGE("%s::Error in video VIDIOC_G_FBUF (%d)", __func__, ret);
        return -1;
    }

    fbuf.base            = (void *)addr;
    fbuf.fmt.width       = width;
    fbuf.fmt.height      = height;
    fbuf.fmt.pixelformat = pix_fmt;

    ret = ioctl(fd, VIDIOC_S_FBUF, &fbuf);
    if (ret < 0) {
        ALOGE("%s::Error in video VIDIOC_S_FBUF (%d)", __func__, ret);
        return -1;
    }

    /* set destination window */
    sFormat.type             = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    sFormat.fmt.win.w.left   = 0;
    sFormat.fmt.win.w.top    = 0;
    sFormat.fmt.win.w.width  = width;
    sFormat.fmt.win.w.height = height;

    ret = ioctl(fd, VIDIOC_S_FMT, &sFormat);
    if (ret < 0) {
        ALOGE("%s::Error in video VIDIOC_S_FMT (%d)", __func__, ret);
        return -1;
    }

    return 0;
}

int SecCamera::clearFimcBuf(int fd)
{
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.memory  = V4L2_MEMORY_USERPTR;
    req.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        ALOGE("Error in VIDIOC_REQBUFS");
    }

    return 0;
}

int SecCamera::stopPreview(void)
{
    int ret;

    ALOGV("%s :", __func__);

    if (m_flag_camera_start == 0) {
        ALOGW("%s: doing nothing because m_flag_camera_start is zero", __func__);
        return 0;
    }

    if (m_params->flash_mode == FLASH_MODE_TORCH)
        setFlashMode(FLASH_MODE_OFF);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }
#ifdef USE_FACE_DETECTION
    if (m_camera_use_ISP) {
        ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CMD_FD, IS_FD_COMMAND_STOP);
        CHECK(ret);
    }
#endif
    /* TODO : This code is temporary implementation. */
    /*        Because streamoff is failed when ae lock or awb lock state */
    if (m_camera_use_ISP && m_params->aeawb_mode) {
        if (m_params->aeawb_mode & 0x1) {
            if (setAutoExposureLock(0) < 0) {
                ALOGE("ERR(%s): Fail on setAutoExposureLock()");
                return -1;
            }
        }
        if (m_params->aeawb_mode & (0x1 << 1)) {
            if (setAutoWhiteBalanceLock(0) < 0) {
                ALOGE("ERR(%s): Fail on setAutoWhiteBalnaceLock()");
                return -1;
            }
        }
        m_params->aeawb_mode = 0;
    }

    ret = fimc_v4l2_streamoff(m_cam_fd);
    CHECK(ret);

    close_buffers(m_buffers_share, MAX_BUFFERS);

    fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE, 0);

    m_flag_camera_start = 0;

    return ret;
}

int SecCamera::startSnapshot(SecBuffer *yuv_buf)
{
    int ret;
    ALOGV("%s :", __func__);

    // already started
    if (m_snapshot_state) {
        ALOGI("%s: Doing nothing because snapshot is already started!", __func__);
        return 0;
    }

    if (m_cap_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    m_snapshot_state = 1;

    memset(&m_events_c2, 0, sizeof(m_events_c2));
    m_events_c2.fd = m_cap_fd;
    m_events_c2.events = POLLIN | POLLERR;

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUV420)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_YUV420");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV12)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_NV12");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV12T)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_NV12T");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV21)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_NV21");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUV422P)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_YUV422P");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUYV)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_YUYV");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_UYVY)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_UYVY");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV16)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_NV16");
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565)
        ALOGV("SnapshotFormat:V4L2_PIX_FMT_RGB565");
    else
        ALOGV("SnapshotFormat:UnknownFormat");
#endif

    if (m_camera_use_ISP) {
        ret = setFimcSrc(m_cap_fd, m_sensor_width, m_sensor_height, NULL);
        CHECK(ret);

        ret = setFimcDst(m_cap_fd, m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat, (unsigned int)(yuv_buf->virt.extP[0]));
        CHECK(ret);
    } else {
        int ret = fimc_v4l2_enum_fmt(m_cap_fd, m_snapshot_v4lformat);
        CHECK(ret);

        if (!m_recording_en)
            ret = fimc_v4l2_s_fmt_cap(m_cap_fd, m_snapshot_width, m_snapshot_height, m_snapshot_v4lformat);
        else
            ret = fimc_v4l2_s_fmt_cap(m_cap_fd, m_sensor_width, m_sensor_height, m_snapshot_v4lformat);
        CHECK(ret);

        ret = fimc_v4l2_s_ctrl(m_cap_fd, V4L2_CID_CACHEABLE, 1);
        CHECK(ret);

        ret = fimc_v4l2_reqbufs(m_cap_fd, V4L2_BUF_TYPE, m_num_capbuf);
        CHECK(ret);

        ret = fimc_v4l2_querybuf(m_cap_fd, m_capture_buf, V4L2_BUF_TYPE, m_num_capbuf, 1);
        CHECK(ret);

        /* start with all buffers in queue */
        for (int i = 0; i <  m_num_capbuf; i++) {
            ret = fimc_v4l2_qbuf(m_cap_fd, m_snapshot_width, m_snapshot_height, m_capture_buf, i, 1, CAPTURE_MODE);
            CHECK(ret);
        }

        ret = fimc_v4l2_streamon(m_cap_fd);
        CHECK(ret);
    }

    return 0;
}

int SecCamera::stopSnapshot(void)
{
    int ret;

    ALOGV("%s :", __func__);

    if (!m_snapshot_state) {
        ALOGI("%s: Doing nothing because snapshot is not started!", __func__);
        return 0;
    }

    if (m_cap_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    ret = fimc_v4l2_streamoff(m_cap_fd);
    CHECK(ret);

    endSnapshot();

    m_snapshot_state = 0;

    return ret;
}

//Recording
int SecCamera::startRecord(bool recordHint)
{
    int ret, i;

    ALOGV("%s :", __func__);

    // aleady started
    if (m_flag_record_start > 0) {
        ALOGE("ERR(%s):Preview was already started", __func__);
        return 0;
    }

    if (m_rec_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    /* enum_fmt, s_fmt sample */
    ret = fimc_v4l2_enum_fmt(m_rec_fd, RECORD_PIX_FMT);
    CHECK(ret);

    ALOGI("%s: m_recording_width = %d, m_recording_height = %d",
         __func__, m_recording_width, m_recording_height);

    ALOGV("m_camera_use_ISP(%d), %s", m_camera_use_ISP, (const char*)getCameraSensorName());

    if (m_camera_use_ISP) {
        fimc_v4l2_s_fmt_is(m_rec_fd, m_sensor_width, m_sensor_height,
                m_preview_v4lformat, (enum v4l2_field) IS_MODE_CAPTURE_VIDEO);

        ret = fimc_v4l2_s_fmt(m_rec_fd, m_recording_width,
                              m_recording_height, RECORD_PIX_FMT, V4L2_FIELD_ANY, RECORD_NUM_PLANE);
        CHECK(ret);
    } else {
        ret = fimc_v4l2_s_fmt(m_rec_fd, m_preview_width,
                              m_preview_height, RECORD_PIX_FMT, V4L2_FIELD_ANY, RECORD_NUM_PLANE);
        CHECK(ret);
        fimc_v4l2_s_fmt_is(m_rec_fd, m_preview_width, m_preview_height,
                m_preview_v4lformat, (enum v4l2_field) IS_MODE_CAPTURE_VIDEO);
    }

    if (!m_camera_use_ISP) {
        ret = fimc_v4l2_s_ctrl(m_rec_fd, V4L2_CID_CAMERA_BUSFREQ_LOCK, 267160);
        CHECK(ret);
    }

    ret = fimc_v4l2_reqbufs(m_rec_fd, V4L2_BUF_TYPE, MAX_BUFFERS);
    CHECK(ret);

    ret = fimc_v4l2_querybuf(m_rec_fd, m_buffers_record, V4L2_BUF_TYPE, MAX_BUFFERS, RECORD_NUM_PLANE);
    CHECK(ret);

    /* start with all buffers in queue */
    for (i = 0; i < MAX_BUFFERS; i++) {
        /* TODO : The name of m_preview_width and m_preview_height is un-suitable in recording phase
                  This is temporary code for V4L2_ION project.
                  We need to modify this messy codes carefully.
           ret = fimc_v4l2_qbuf(m_rec_fd, m_recording_width, m_recording_height, m_buffers_record, i, RECORD_NUM_PLANE, RECORD_MODE);
         */
        ret = fimc_v4l2_qbuf(m_rec_fd, m_preview_width, m_preview_height, m_buffers_record, i, RECORD_NUM_PLANE, RECORD_MODE);
        CHECK(ret);
    }

    // Get and throw away the first frame since it is often garbled.
    memset(&m_events_c3, 0, sizeof(m_events_c3));
    m_events_c3.fd = m_rec_fd;
    m_events_c3.events = POLLIN | POLLERR;

    m_record_hint = recordHint;

    ret = fimc_v4l2_streamon(m_rec_fd);
    CHECK(ret);

    m_flag_record_start = 1;

    return 0;
}

int SecCamera::stopRecord(void)
{
    int ret;

    ALOGV("%s :", __func__);

    if (m_flag_record_start == 0) {
        ALOGW("%s: doing nothing because m_flag_record_start is zero", __func__);
        return 0;
    }

    if (m_rec_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    m_flag_record_start = 0;

    if (!m_camera_use_ISP) {
        ret = fimc_v4l2_s_ctrl(m_rec_fd, V4L2_CID_CAMERA_BUSFREQ_UNLOCK, 0);
        CHECK(ret);

        ret = fimc_v4l2_streamoff(m_rec_fd);
        CHECK(ret);

        close_buffers(m_buffers_record, MAX_BUFFERS);

        fimc_v4l2_reqbufs(m_rec_fd, V4L2_BUF_TYPE, 0);
    }

    return 0;
}

int SecCamera::getRecordAddr(int index, SecBuffer *buffer)
{
    buffer->phys.extP[0] = fimc_v4l2_s_ctrl(m_rec_fd, V4L2_CID_PADDR_Y, index);
    CHECK((int)buffer->phys.extP[0]);
    buffer->phys.extP[1] = fimc_v4l2_s_ctrl(m_rec_fd, V4L2_CID_PADDR_CBCR, index);
    CHECK((int)buffer->phys.extP[1]);
    return 0;
}

int SecCamera::getRecordPhysAddr(int index, SecBuffer *buffer)
{
    ALOGV("%s", __func__);

    buffer->phys.extP[0] = m_buffers_record[index].phys.extP[0];
    CHECK((int)buffer->phys.extP[0]);

    buffer->phys.extP[1] = m_buffers_record[index].phys.extP[1];
    CHECK((int)buffer->phys.extP[1]);

    return 0;
}

int SecCamera::getPreviewAddr(int index, SecBuffer *buffer)
{
    buffer->phys.extP[0] = fimc_v4l2_s_ctrl(m_prev_fd, V4L2_CID_PADDR_Y, index);
    CHECK((int)buffer->phys.extP[0]);
    buffer->phys.extP[1] = fimc_v4l2_s_ctrl(m_prev_fd, V4L2_CID_PADDR_CBCR, index);
    CHECK((int)buffer->phys.extP[1]);
    return 0;
}

int SecCamera::getShareBufferAddr(int index, struct fimc_buf *src_buf)
{
    src_buf->base[0] = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_PADDR_Y, index);
    src_buf->base[1] = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_PADDR_CBCR, index);

    ALOGV("%s: Y %p, CbCr %p,index %d",
        __func__, src_buf->base[0], src_buf->base[1], index);

    return 0;
}

int SecCamera::getSnapshotAddr(int index, SecBuffer *buffer)
{
    buffer->virt.extP[0] = (char *)m_cap_mapped_addr;
    CHECK((int)buffer->virt.extP[0]);

    buffer->virt.extP[1] = (char *)m_cap_mapped_addr + (m_snapshot_width * m_snapshot_height);
    CHECK((int)buffer->virt.extP[1]);

    if (buffer->virt.extP[0] == 0) {
        ALOGE("ERR(%s): Snapshot mapped addr is NULL");
        return -1;
    }

    return 0;
}

int SecCamera::getCaptureAddr(int index, SecBuffer *buffer)
{
    buffer->virt.extP[0] = m_capture_buf[index].virt.extP[0];
    CHECK((int)buffer->virt.extP[0]);
    return 0;
}

#ifdef IS_FW_DEBUG
int SecCamera::getDebugAddr(unsigned int *vaddr)
{
    m_debug_paddr = (off_t)fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_IS_FW_DEBUG_REGION_ADDR);
    CHECK((int)m_debug_paddr);

    int m_mem_fd = open(PFX_NODE_MEM, O_RDWR);
    if (m_mem_fd < 0) {
        ALOGE("ERR(%s):Cannot open %s (error : %s)", __func__, PFX_NODE_MEM, strerror(errno));
    }

    *vaddr = (unsigned int)mmap(0, 128 * SIZE_4K, PROT_READ | PROT_WRITE, MAP_SHARED, m_mem_fd, m_debug_paddr);
    return 0;
}
#endif

int SecCamera::getPreview(camera_frame_metadata_t *facedata)
{
    int index;
    int ret;

    if (m_flag_camera_start == 0 || fimc_poll(&m_events_c) == 0) {
        ALOGE("ERR(%s):Start Camera Device Reset", __func__);
        /*
         * When there is no data for more than 1 second from the camera we inform
         * the FIMC driver by calling fimc_v4l2_s_input() with a special value = 1000
         * FIMC driver identify that there is something wrong with the camera
         * and it restarts the sensor.
         */
        stopPreview();
        /* Reset Only Camera Device */
        ret = fimc_v4l2_querycap(m_cam_fd);
        CHECK(ret);
        if (fimc_v4l2_enuminput(m_cam_fd, m_camera_id))
            return -1;
        ret = fimc_v4l2_s_input(m_cam_fd, 1000);
        CHECK(ret);
        ret = startPreview();
        if (ret < 0) {
            ALOGE("ERR(%s): startPreview() return %d", __func__, ret);
            return 0;
        }
    }

    index = fimc_v4l2_dqbuf(m_cam_fd, PREVIEW_NUM_PLANE);
    if (!(0 <= index && index < MAX_BUFFERS)) {
        ALOGE("ERR(%s):wrong index = %d", __func__, index);
        return -1;
    }

#ifdef USE_FACE_DETECTION
    if (m_camera_use_ISP) {
        fimc_v4l2_s_ext_ctrl_face_detection(m_cam_fd, 0, facedata);
    }
#endif

    return index;
}

int SecCamera::setPreviewFrame(int index)
{
    ALOGV("%s", __func__);
    int ret;
    ret = fimc_v4l2_qbuf(m_cam_fd, m_preview_width, m_preview_height, m_buffers_preview, index, PREVIEW_NUM_PLANE, PREVIEW_MODE);
    CHECK(ret);

    return ret;
}

int SecCamera::getSnapshot()
{
    int index;
    int ret;

    if (m_snapshot_state) {
        fimc_poll(&m_events_c2);

        index = fimc_v4l2_dqbuf(m_cap_fd, 1);
        if (!(0 <= index && index < m_num_capbuf)) {
            ALOGE("ERR(%s):wrong index = %d", __func__, index);
            return -1;
        }
        return index;
    }

    return -1;
}

int SecCamera::setSnapshotFrame(int index)
{
    ALOGV("%s :", __func__);
    int ret;
    ret = fimc_v4l2_qbuf(m_cap_fd, m_snapshot_width, m_snapshot_height, m_capture_buf, index, PREVIEW_NUM_PLANE, CAPTURE_MODE);
    CHECK(ret);

    return ret;
}

int SecCamera::getRecordFrame()
{
    if (m_flag_record_start == 0) {
        ALOGE("%s: m_flag_record_start is 0", __func__);
        return -1;
    }

    fimc_poll(&m_events_c3);
    int index = fimc_v4l2_dqbuf(m_rec_fd, RECORD_NUM_PLANE);
    if (!(0 <= index && index < MAX_BUFFERS)) {
        ALOGE("ERR(%s):wrong index = %d", __func__, index);
        return -1;
    }

    return index;
}

int SecCamera::releaseRecordFrame(int index)
{
    ALOGV("%s :", __func__);
    if (!m_flag_record_start) {
        /* this can happen when recording frames are returned after
         * the recording is stopped at the driver level.  we don't
         * need to return the buffers in this case and we've seen
         * cases where fimc could crash if we called qbuf and it
         * wasn't expecting it.
         */
//        ALOGI("%s: recording not in progress, ignoring", __func__);
        return 0;
    }

    /* TODO : For V4L2_ION. need to modify this.
     return fimc_v4l2_qbuf(m_rec_fd, m_recording_width, m_recording_height, m_buffers_record, index, RECORD_NUM_PLANE, RECORD_MODE);
     */
    return fimc_v4l2_qbuf(m_rec_fd, m_preview_width, m_preview_height, m_buffers_record, index, RECORD_NUM_PLANE, RECORD_MODE);
}

int SecCamera::setPreviewSize(int width, int height, int pixel_format)
{
    ALOGV("%s(width(%d), height(%d), format(%d))", __func__, width, height, pixel_format);

    int v4lpixelformat = pixel_format;

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if (v4lpixelformat == V4L2_PIX_FMT_YUV420)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_YUV420");
    else if (v4lpixelformat == V4L2_PIX_FMT_YVU420)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_YVU420");
    else if (v4lpixelformat == V4L2_PIX_FMT_YVU420M)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_YVU420M");
    else if (v4lpixelformat == V4L2_PIX_FMT_NV12)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_NV12");
    else if (v4lpixelformat == V4L2_PIX_FMT_NV12T)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_NV12T");
    else if (v4lpixelformat == V4L2_PIX_FMT_NV21)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_NV21");
    else if (v4lpixelformat == V4L2_PIX_FMT_YUV422P)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_YUV422P");
    else if (v4lpixelformat == V4L2_PIX_FMT_YUYV)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_YUYV");
    else if (v4lpixelformat == V4L2_PIX_FMT_RGB565)
        ALOGV("PreviewFormat:V4L2_PIX_FMT_RGB565");
    else
        ALOGV("PreviewFormat:UnknownFormat");
#endif
    m_preview_width  = width;
    m_preview_height = height;
    m_preview_v4lformat = v4lpixelformat;

    return 0;
}

int SecCamera::getPreviewSize(int *width, int *height, int *frame_size)
{
    *width  = m_preview_width;
    *height = m_preview_height;
    *frame_size = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_preview_v4lformat), *width, *height);
    return 0;
}

int SecCamera::getPreviewSrcSize(int *width, int *height, int *frame_size)
{
    *width  = m_sensor_width;
    *height = m_sensor_height;
    *frame_size = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_preview_v4lformat), *width, *height);
    return 0;
}

int SecCamera::getPreviewMaxSize(int *width, int *height)
{
    *width  = m_preview_max_width;
    *height = m_preview_max_height;

    return 0;
}

int SecCamera::getPreviewPixelFormat(void)
{
    return m_preview_v4lformat;
}

/*
 * Devide getJpeg() as two funcs, setSnapshotCmd() & getJpeg() because of the shutter sound timing.
 * Here, just send the capture cmd to camera ISP to start JPEG capture.
 */
int SecCamera::setSnapshotCmd(void)
{
    ALOGV("%s :", __func__);

    int ret = 0;

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return 0;
    }

    if (m_flag_camera_start > 0) {
        ALOGW("WARN(%s):Camera was in preview, should have been stopped", __func__);
        stopPreview();
    }

    memset(&m_events_c, 0, sizeof(m_events_c));
    m_events_c.fd = m_cam_fd;
    m_events_c.events = POLLIN | POLLERR;

    int nframe = 1;

    ret = fimc_v4l2_enum_fmt(m_cam_fd,m_snapshot_v4lformat);
    CHECK(ret);
    ret = fimc_v4l2_s_fmt_cap(m_cam_fd, m_snapshot_width, m_snapshot_height, V4L2_PIX_FMT_JPEG);
    CHECK(ret);

    if (!m_camera_use_ISP)
        if (!m_recording_en)
            fimc_v4l2_s_fmt_is(m_cap_fd, m_snapshot_width, m_snapshot_height,
                    V4L2_PIX_FMT_JPEG, (enum v4l2_field) IS_MODE_PREVIEW_STILL);
        else
            fimc_v4l2_s_fmt_is(m_cap_fd, m_sensor_width, m_sensor_height,
                    V4L2_PIX_FMT_JPEG, (enum v4l2_field) IS_MODE_PREVIEW_VIDEO);

    ret = fimc_v4l2_reqbufs(m_cam_fd, V4L2_BUF_TYPE, nframe);
    CHECK(ret);

    ret = fimc_v4l2_querybuf(m_cam_fd, m_capture_buf, V4L2_BUF_TYPE, 1, 1);
    CHECK(ret);

    ret = fimc_v4l2_qbuf(m_cam_fd, m_snapshot_width, m_snapshot_height, m_capture_buf, 0, 1, CAPTURE_MODE);
    CHECK(ret);

    ret = fimc_v4l2_streamon(m_cam_fd);
    CHECK(ret);

    return 0;
}

int SecCamera::endSnapshot(void)
{
    close_buffers(m_capture_buf, m_num_capbuf);

    fimc_v4l2_reqbufs(m_cap_fd, V4L2_BUF_TYPE, 0);

    return 0;
}

/*
 * Set Jpeg quality & exif info and get JPEG data from camera ISP
 */
unsigned char* SecCamera::getJpeg(int *jpeg_size,
                                  int *thumb_size,
                                  unsigned int *thumb_addr,
                                  unsigned int *phyaddr)
{
    int index, ret = 0;
    unsigned char *addr;
    SecBuffer jpegAddr;

    // capture
    ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_CAPTURE, 0);
    CHECK_PTR(ret);
    ret = fimc_poll(&m_events_c);
    CHECK_PTR(ret);
    index = fimc_v4l2_dqbuf(m_cam_fd, 1);

    if (index != 0) {
        ALOGE("ERR(%s):wrong index = %d", __func__, index);
        return NULL;
    }

    *jpeg_size = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_MAIN_SIZE);
    CHECK_PTR(*jpeg_size);

    int main_offset = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_MAIN_OFFSET);
    CHECK_PTR(main_offset);

    *thumb_size = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_THUMB_SIZE);
    CHECK_PTR(*thumb_size);

    int thumb_offset = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_THUMB_OFFSET);
    CHECK_PTR(thumb_offset);

    m_postview_offset = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET);
    CHECK_PTR(m_postview_offset);

    ret = fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_STREAM_PAUSE, 0);
    CHECK_PTR(ret);

    ALOGV("\nsnapshot dqueued buffer = %d snapshot_width = %d snapshot_height = %d, size = %d",
            index, m_snapshot_width, m_snapshot_height, *jpeg_size);

    addr = (unsigned char*)(m_capture_buf[0].virt.extP[0]) + main_offset;

    *thumb_addr = (unsigned int)(addr + thumb_offset);

    getPreviewAddr(index, &jpegAddr);
    *phyaddr = jpegAddr.phys.extP[0] + m_postview_offset;

    ret = fimc_v4l2_streamoff(m_cam_fd);
    CHECK_PTR(ret);

    return addr;
}

int SecCamera::getExif(unsigned char *pExifDst, unsigned char *pThumbSrc, int thumbSize)
{
#ifdef SAMSUNG_EXYNOS4210
    /* JPEG encode for smdkv310 */
    if (m_jpeg_fd > 0) {
        if (api_jpeg_encode_deinit(m_jpeg_fd) != JPEG_OK)
            ALOGE("ERR(%s):Fail on api_jpeg_encode_deinit", __func__);
        m_jpeg_fd = 0;
    }

    m_jpeg_fd = api_jpeg_encode_init();
    ALOGV("(%s):JPEG device open ID = %d", __func__, m_jpeg_fd);

    if (m_jpeg_fd <= 0) {
        if (m_jpeg_fd < 0) {
            m_jpeg_fd = 0;
            ALOGE("ERR(%s):Cannot open a jpeg device file", __func__);
            return -1;
        }
        ALOGE("ERR(%s):JPEG device was closed", __func__);
        return -1;
    }

    if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) {
        ALOGE("ERR(%s):It doesn't support V4L2_PIX_FMT_RGB565", __func__);
        return -1;
    }

    struct jpeg_enc_param    enc_param;
    enum jpeg_frame_format inFormat = YUV_422;
    enum jpeg_stream_format outFormat = JPEG_422;

    switch (m_snapshot_v4lformat) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        inFormat = YUV_420;
        outFormat = JPEG_420;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YUV422P:
    default:
        inFormat = YUV_422;
        outFormat = JPEG_422;
        break;
    }

    // set encode parameters //
    enc_param.width = m_jpeg_thumbnail_width;
    enc_param.height = m_jpeg_thumbnail_width;
    enc_param.in_fmt = inFormat; // YCBCR Only
    enc_param.out_fmt = outFormat;

    if (m_jpeg_thumbnail_quality >= 90)
        enc_param.quality = QUALITY_LEVEL_1;
    else if (m_jpeg_thumbnail_quality >= 80)
        enc_param.quality = QUALITY_LEVEL_2;
    else if (m_jpeg_thumbnail_quality >= 70)
        enc_param.quality = QUALITY_LEVEL_3;
    else
        enc_param.quality = QUALITY_LEVEL_4;

    api_jpeg_set_encode_param(&enc_param);

    unsigned int thumbnail_size = m_jpeg_thumbnail_width * m_jpeg_thumbnail_height * 2;
    unsigned char *pInBuf = (unsigned char *)api_jpeg_get_encode_in_buf(m_jpeg_fd, thumbnail_size);
    if (pInBuf == NULL) {
        ALOGE("ERR(%s):JPEG input buffer is NULL!!", __func__);
        return -1;
    }

    unsigned char *pOutBuf = (unsigned char *)api_jpeg_get_encode_out_buf(m_jpeg_fd);
    if (pOutBuf == NULL) {
        ALOGE("ERR(%s):JPEG output buffer is NULL!!", __func__);
        return -1;
    }

    memcpy(pInBuf, pThumbSrc, thumbnail_size);

    enum jpeg_ret_type result = api_jpeg_encode_exe(m_jpeg_fd, &enc_param);
    if (result != JPEG_ENCODE_OK) {
        ALOGE("ERR(%s):encode failed", __func__);
        return -1;
    }

    unsigned int outbuf_size = enc_param.size;
    unsigned int exifSize;

    setExifChangedAttribute();

    ALOGV("%s: calling jpgEnc.makeExif, mExifInfo.width set to %d, height to %d",
         __func__, mExifInfo.width, mExifInfo.height);

    ALOGV("%s : enableThumb set to true", __func__);
    mExifInfo.enableThumb = true;

    makeExif(pExifDst, pOutBuf, outbuf_size, &mExifInfo, &exifSize, true);
#endif

#ifdef SAMSUNG_EXYNOS4x12
    /* JPEG encode for smdk4x12 */
    unsigned int exifSize;

    if (m_camera_use_ISP) {
        ALOGV("%s : m_jpeg_thumbnail_width = %d, height = %d",
             __func__, m_jpeg_thumbnail_width, m_jpeg_thumbnail_height);
        m_jpeg_fd = jpeghal_enc_init();
        ALOGV("(%s):JPEG device open ID = %d", __func__, m_jpeg_fd);

        if (m_jpeg_fd <= 0) {
            if (m_jpeg_fd < 0) {
                m_jpeg_fd = 0;
                ALOGE("ERR(%s):Cannot open a jpeg device file", __func__);
                return -1;
            }
            ALOGE("ERR(%s):JPEG device was closed", __func__);
            return -1;
        }

        if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) {
            ALOGE("ERR(%s):It doesn't support V4L2_PIX_FMT_RGB565", __func__);
            return -1;
        }

        struct jpeg_config    enc_config;
        int inFormat, outFormat;

        inFormat = m_snapshot_v4lformat;

        switch (inFormat) {
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_YUV420:
            outFormat = V4L2_PIX_FMT_JPEG_420;
            break;
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_YUV422P:
        default:
            outFormat = V4L2_PIX_FMT_JPEG_422;
            break;
        }

        // set encode parameters //
        enc_config.mode = JPEG_ENCODE;
        enc_config.enc_qual = QUALITY_LEVEL_4;
        enc_config.width = m_jpeg_thumbnail_width;
        enc_config.height = m_jpeg_thumbnail_height;
        enc_config.pix.enc_fmt.in_fmt = inFormat;
        enc_config.pix.enc_fmt.out_fmt = outFormat;

        jpeghal_enc_setconfig(m_jpeg_fd, &enc_config);

        jpeghal_s_ctrl(m_jpeg_fd, V4L2_CID_CACHEABLE, 1);

        struct jpeg_buf    m_jpeg_inbuf;
        m_jpeg_inbuf.memory = V4L2_MEMORY_MMAP;

        m_jpeg_inbuf.num_planes = 2;

        if (jpeghal_set_inbuf(m_jpeg_fd, &m_jpeg_inbuf) < 0) {
            ALOGE("ERR(%s):Fail to JPEG input buffer!!", __func__);
            return -1;
        }

        struct jpeg_buf    m_jpeg_outbuf;
        m_jpeg_outbuf.memory = V4L2_MEMORY_MMAP;
        m_jpeg_outbuf.num_planes = 1;

        if (jpeghal_set_outbuf(m_jpeg_fd, &m_jpeg_outbuf) < 0) {
            ALOGE("ERR(%s):Fail to JPEG output buffer!!", __func__);
            return -1;
        }

        int y_size = m_jpeg_thumbnail_width * m_jpeg_thumbnail_height;
        memcpy(m_jpeg_inbuf.start[0], pThumbSrc, m_jpeg_inbuf.length[0]);
        memcpy(m_jpeg_inbuf.start[1], pThumbSrc + y_size, m_jpeg_inbuf.length[1]);

        if (jpeghal_enc_exe(m_jpeg_fd, &m_jpeg_inbuf, &m_jpeg_outbuf) < 0) {
            ALOGE("ERR(%s):encode failed", __func__);
            return -1;
        }

        int outbuf_size = jpeghal_g_ctrl(m_jpeg_fd, V4L2_CID_CAM_JPEG_ENCODEDSIZE);
        if (outbuf_size < 0) {
            ALOGE("ERR(%s): jpeghal_g_ctrl fail on V4L2_CID_CAM_JPEG_ENCODEDSIZE", __func__);
            return -1;
        }

        setExifChangedAttribute();

        ALOGV("%s: calling jpgEnc.makeExif, mExifInfo.width set to %d, height to %d",
             __func__, mExifInfo.width, mExifInfo.height);

        ALOGV("%s : enableThumb set to true", __func__);
        mExifInfo.enableThumb = true;

        makeExif(pExifDst, (unsigned char *)m_jpeg_outbuf.start[0], (unsigned int)outbuf_size, &mExifInfo, &exifSize, true);

        if (m_jpeg_fd > 0) {
            if (jpeghal_deinit(m_jpeg_fd, &m_jpeg_inbuf, &m_jpeg_outbuf) < 0)
                ALOGE("ERR(%s):Fail on api_jpeg_encode_deinit", __func__);
            m_jpeg_fd = 0;
        }
    } else {
        setExifChangedAttribute();
        mExifInfo.enableThumb = true;
        makeExif(pExifDst, pThumbSrc, (unsigned int)thumbSize, &mExifInfo, &exifSize, true);
    }
#endif

    return exifSize;
}

void SecCamera::getPostViewConfig(int *width, int *height, int *size)
{
    *width = m_snapshot_width;
    *height = m_snapshot_height;
    *size = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_snapshot_v4lformat), *width, *height);
    ALOGV("[5B] m_preview_width : %d, mPostViewWidth = %d mPostViewHeight = %d mPostViewSize = %d",
            m_preview_width, *width, *height, *size);
}

void SecCamera::getThumbnailConfig(int *width, int *height, int *size)
{
    *width = m_jpeg_thumbnail_width;
    *height = m_jpeg_thumbnail_height;

    *size = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_snapshot_v4lformat), *width, *height);
}

int SecCamera::getPostViewOffset(void)
{
    return m_postview_offset;
}

int SecCamera::getSnapshotAndJpeg(SecBuffer *yuv_buf, int index, unsigned char *jpeg_buf,
                                            int *output_size)
{
    ALOGV("%s :", __func__);

    int ret = 0;
    int i;

#ifdef ZERO_SHUTTER_LAG
    if (!m_camera_use_ISP){
        startSnapshot(yuv_buf);

        index = getSnapshot();
        if (index < 0) {
            ALOGE("ERR(%s): Invalid index!", __func__);
            return -1;
        }

        ret = fimc_v4l2_s_ctrl(m_cap_fd, V4L2_CID_STREAM_PAUSE, 0);
        CHECK_PTR(ret);
        ALOGV("snapshot dequeued buffer = %d snapshot_width = %d snapshot_height = %d",
                index, m_snapshot_width, m_snapshot_height);

        getCaptureAddr(index, yuv_buf);

       if (yuv_buf->virt.extP[0] == NULL) {
           ALOGE("ERR(%s):Fail on SecCamera getCaptureAddr = %0x ",
                __func__, yuv_buf->virt.extP[0]);
           return UNKNOWN_ERROR;
       }
    }
#else
    startSnapshot(yuv_buf);

    index = getSnapshot();
    if (index < 0) {
        ALOGE("ERR(%s): Invalid index!", __func__);
        return -1;
    }

    ret = fimc_v4l2_s_ctrl(m_cap_fd, V4L2_CID_STREAM_PAUSE, 0);
    CHECK_PTR(ret);
    ALOGV("snapshot dequeued buffer = %d snapshot_width = %d snapshot_height = %d",
            index, m_snapshot_width, m_snapshot_height);

    getCaptureAddr(index, yuv_buf);

    if (yuv_buf->virt.extP[0] == NULL) {
        ALOGE("ERR(%s):Fail on SecCamera getCaptureAddr = %0x ",
             __func__, yuv_buf->virt.extP[0]);
        return UNKNOWN_ERROR;
    }
#endif

#ifdef SAMSUNG_EXYNOS4210
    /* JPEG encode for smdkv310 */
    if (m_jpeg_fd > 0) {
        if (api_jpeg_encode_deinit(m_jpeg_fd) != JPEG_OK)
            ALOGE("ERR(%s):Fail on api_jpeg_encode_deinit", __func__);
        m_jpeg_fd = 0;
    }

    m_jpeg_fd = api_jpeg_encode_init();
    ALOGV("(%s):JPEG device open ID = %d", __func__, m_jpeg_fd);

    if (m_jpeg_fd <= 0) {
        if (m_jpeg_fd < 0) {
            m_jpeg_fd = 0;
            ALOGE("ERR(%s):Cannot open a jpeg device file", __func__);
            return -1;
        }
        ALOGE("ERR(%s):JPEG device was closed", __func__);
        return -1;
    }

    if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) {
        ALOGE("ERR(%s):It doesn't support V4L2_PIX_FMT_RGB565", __func__);
        return -1;
    }

    struct jpeg_enc_param    enc_param;
    enum jpeg_frame_format inFormat = YUV_422;
    enum jpeg_stream_format outFormat = JPEG_422;

    switch (m_snapshot_v4lformat) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        inFormat = YUV_420;
        outFormat = JPEG_420;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YUV422P:
    default:
        inFormat = YUV_422;
        outFormat = JPEG_422;
        break;
    }

    // set encode parameters //
    enc_param.width = m_snapshot_width;
    enc_param.height = m_snapshot_height;
    enc_param.in_fmt = inFormat; // YCBCR Only
    enc_param.out_fmt = outFormat;

    if (m_jpeg_quality >= 90)
        enc_param.quality = QUALITY_LEVEL_1;
    else if (m_jpeg_quality >= 80)
        enc_param.quality = QUALITY_LEVEL_2;
    else if (m_jpeg_quality >= 70)
        enc_param.quality = QUALITY_LEVEL_3;
    else
        enc_param.quality = QUALITY_LEVEL_4;

    api_jpeg_set_encode_param(&enc_param);

    unsigned int snapshot_size = m_snapshot_width * m_snapshot_height * 2;
    unsigned char *pInBuf = (unsigned char *)api_jpeg_get_encode_in_buf(m_jpeg_fd, snapshot_size);
    if (pInBuf == NULL) {
        ALOGE("ERR(%s):JPEG input buffer is NULL!!", __func__);
        return -1;
    }

    unsigned char *pOutBuf = (unsigned char *)api_jpeg_get_encode_out_buf(m_jpeg_fd);
    if (pOutBuf == NULL) {
        ALOGE("ERR(%s):JPEG output buffer is NULL!!", __func__);
        return -1;
    }

    memcpy(pInBuf, yuv_buf->virt.extP[0], snapshot_size);

    enum jpeg_ret_type result = api_jpeg_encode_exe(m_jpeg_fd, &enc_param);
    if (result != JPEG_ENCODE_OK) {
        ALOGE("ERR(%s):encode failed", __func__);
        return -1;
    }

    *output_size = enc_param.size;
    memcpy(jpeg_buf, pOutBuf, *output_size);
#endif

#ifdef SAMSUNG_EXYNOS4x12
    /* JPEG encode for smdk4x12 */
    m_jpeg_fd = jpeghal_enc_init();
    ALOGV("(%s):JPEG device open ID = %d", __func__, m_jpeg_fd);

    if (m_jpeg_fd <= 0) {
        if (m_jpeg_fd < 0) {
            m_jpeg_fd = 0;
            ALOGE("ERR(%s):Cannot open a jpeg device file", __func__);
            return -1;
        }
        ALOGE("ERR(%s):JPEG device was closed", __func__);
        return -1;
    }

    if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565) {
        ALOGE("ERR(%s):It doesn't support V4L2_PIX_FMT_RGB565", __func__);
        return -1;
    }

    struct jpeg_config    enc_config;
    int inFormat, outFormat;

    inFormat = m_snapshot_v4lformat;

    switch (inFormat) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        outFormat = V4L2_PIX_FMT_JPEG_420;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YUV422P:
    default:
        outFormat = V4L2_PIX_FMT_JPEG_422;
        break;
    }

    // set encode parameters //
    enc_config.mode = JPEG_ENCODE;

    if (m_jpeg_quality >= 90)
        enc_config.enc_qual = QUALITY_LEVEL_1;
    else if (m_jpeg_quality >= 80)
        enc_config.enc_qual = QUALITY_LEVEL_2;
    else if (m_jpeg_quality >= 70)
        enc_config.enc_qual = QUALITY_LEVEL_3;
    else
        enc_config.enc_qual = QUALITY_LEVEL_4;

    enc_config.width = m_snapshot_width;
    enc_config.height = m_snapshot_height;

    enc_config.pix.enc_fmt.in_fmt = inFormat;
    enc_config.pix.enc_fmt.out_fmt = outFormat;

    jpeghal_enc_setconfig(m_jpeg_fd, &enc_config);

    if (m_flag_record_start == 0)
        /* output buf cacheable */
        ret = jpeghal_s_ctrl(m_jpeg_fd, V4L2_CID_CACHEABLE, 3);
    else
        /* input & output buf cacheable */
        ret = jpeghal_s_ctrl(m_jpeg_fd, V4L2_CID_CACHEABLE, 1);

    CHECK(ret);

    struct jpeg_buf    m_jpeg_inbuf;

    m_jpeg_inbuf.start[0] = (void *)m_snapshot_phys_addr;
    m_jpeg_inbuf.start[1] = (void *)m_snapshot_phys_addr + (m_snapshot_width * m_snapshot_height);

    m_jpeg_inbuf.length[0] = m_snapshot_width * m_snapshot_height;
    m_jpeg_inbuf.length[1] = m_snapshot_width * m_snapshot_height;

    m_jpeg_inbuf.num_planes = 2;

    m_jpeg_inbuf.memory = V4L2_MEMORY_USERPTR;

    if (jpeghal_set_inbuf(m_jpeg_fd, &m_jpeg_inbuf) < 0) {
        ALOGE("ERR(%s):Fail to JPEG input buffer!!", __func__);
        return -1;
    }

    for (i = 0; i < m_jpeg_inbuf.num_planes; i++) {
        if ((unsigned int)m_jpeg_inbuf.start[i] & (32 - 1)) {
            ALOGE("ERR(%s): JPEG start address should be aligned to 32 bytes", __func__);
            return -1;
        } else if ((unsigned int)enc_config.width & (16 - 1)) {
            ALOGE("ERR(%s): Image width should be multiple of 16", __func__);
            return -1;
        }
    }

    struct jpeg_buf    m_jpeg_outbuf;
    m_jpeg_outbuf.memory = V4L2_MEMORY_MMAP;
    m_jpeg_outbuf.num_planes = 1;

    if (jpeghal_set_outbuf(m_jpeg_fd, &m_jpeg_outbuf) < 0) {
        ALOGE("ERR(%s):Fail to JPEG output buffer!!", __func__);
        return -1;
    }

    if (jpeghal_enc_exe(m_jpeg_fd, &m_jpeg_inbuf, &m_jpeg_outbuf) < 0) {
        ALOGE("ERR(%s):encode failed", __func__);
        return -1;
    }

    ret = jpeghal_g_ctrl(m_jpeg_fd, V4L2_CID_CAM_JPEG_ENCODEDSIZE);
    if (ret < 0) {
        ALOGE("ERR(%s): jpeghal_g_ctrl fail on V4L2_CID_CAM_JPEG_ENCODEDSIZE", __func__);
        return -1;
    } else {
        *output_size = (unsigned int)ret;
    }

    memcpy(jpeg_buf, m_jpeg_outbuf.start[0], *output_size);

    if (m_jpeg_fd > 0) {
        if (jpeghal_deinit(m_jpeg_fd, &m_jpeg_inbuf, &m_jpeg_outbuf) < 0)
            ALOGE("ERR(%s):Fail on api_jpeg_encode_deinit", __func__);
        m_jpeg_fd = 0;
    }
#endif

    return 0;
}

int SecCamera::setSensorSize(int width, int height)
{
    ALOGV("%s(width(%d), height(%d))", __func__, width, height);

    m_sensor_width  = width;
    m_sensor_height = height;

    return 0;
}

int SecCamera::getSensorSize(int *width, int *height, int *frame_size)
{
    *width  = m_sensor_width;
    *height = m_sensor_height;

    int frame = 0;

    frame = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_snapshot_v4lformat), *width, *height);

    // set it big.
    if (frame == 0)
        frame = m_sensor_width * m_sensor_height * BPP;

    *frame_size = frame;

    return 0;
}

int SecCamera::setSnapshotSize(int width, int height)
{
    ALOGV("%s(width(%d), height(%d))", __func__, width, height);

    m_snapshot_width  = width;
    m_snapshot_height = height;

    return 0;
}

int SecCamera::getSnapshotSize(int *width, int *height, int *frame_size)
{
    *width  = m_snapshot_width;
    *height = m_snapshot_height;

    int frame = 0;

    frame = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(m_snapshot_v4lformat), *width, *height);

    // set it big.
    if (frame == 0)
        frame = m_snapshot_width * m_snapshot_height * BPP;

    *frame_size = frame;

    return 0;
}

int SecCamera::getSnapshotMaxSize(int *width, int *height)
{
    *width  = m_snapshot_max_width;
    *height = m_snapshot_max_height;

    return 0;
}

int SecCamera::setSnapshotPixelFormat(int pixel_format)
{
    int v4lpixelformat = pixel_format;

    if (m_snapshot_v4lformat != v4lpixelformat) {
        m_snapshot_v4lformat = v4lpixelformat;
    }

#if defined(LOG_NDEBUG) && LOG_NDEBUG == 0
    if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUV420)
        ALOGE("%s : SnapshotFormat:V4L2_PIX_FMT_YUV420", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV12)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_NV12", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV12T)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_NV12T", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV21)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_NV21", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUV422P)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_YUV422P", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_YUYV)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_YUYV", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_UYVY)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_UYVY", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_NV16)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_NV16", __func__);
    else if (m_snapshot_v4lformat == V4L2_PIX_FMT_RGB565)
        ALOGD("%s : SnapshotFormat:V4L2_PIX_FMT_RGB565", __func__);
    else
        ALOGD("SnapshotFormat:UnknownFormat");
#endif
    return 0;
}

int SecCamera::getSnapshotPixelFormat(void)
{
    return m_snapshot_v4lformat;
}

int SecCamera::getCameraId(void)
{
    return m_camera_id;
}

int SecCamera::initSetParams(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ISO, ISO_AUTO) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ISO", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING, IS_METERING_AVERAGE) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_METERING", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SATURATION, SATURATION_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SATURATION", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SCENE_MODE, SCENE_MODE_NONE) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SCENE_MODE", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SHARPNESS, SHARPNESS_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SHARPNESS", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_WHITE_BALANCE, WHITE_BALANCE_AUTO) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_WHITE_BALANCE", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ANTI_BANDING, ANTI_BANDING_OFF) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ANTI_BANDING", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_CONTRAST, IS_CONTRAST_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_CONTRAST", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_EFFECT, IMAGE_EFFECT_NONE) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_EFFECT", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_BRIGHTNESS, IS_BRIGHTNESS_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_BRIGHTNESS", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_EXPOSURE, IS_EXPOSURE_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_EXPOSURE", __func__);
        return -1;
    }
    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_HUE, IS_HUE_DEFAULT) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_HUE", __func__);
        return -1;
    }

    initParameters(m_camera_use_ISP);

    return 0;
}

int SecCamera::setAutofocus(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_AUTO_FOCUS, AUTO_FOCUS_ON) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_AUTO_FOCUS", __func__);
        return -1;
    }

    m_auto_focus_state = 1;

    return 0;
}

int SecCamera::setTouchAF(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FOCUS_MODE, FOCUS_MODE_TOUCH) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FOCUS_MODE", __func__);
        return -1;
    }

    return 0;
}

int SecCamera::getAutoFocusResult(void)
{
    int af_result;

    af_result = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_AUTO_FOCUS_RESULT);

    ALOGV("%s : returning %d", __func__, af_result);

    return af_result;
}

int SecCamera::cancelAutofocus(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (m_flag_camera_start && m_auto_focus_state) {
        if (m_params->focus_mode == FOCUS_MODE_AUTO || m_params->focus_mode == FOCUS_MODE_MACRO) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_AUTO_FOCUS, AUTO_FOCUS_OFF) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_AUTO_FOCUS", __func__);
                return -1;
            }
        }
        m_auto_focus_state = 0;
    }

    /* TODO: Currently we only able to set same area both touchAF and touchMetering */
    if (m_isTouchMetering) {
        m_isTouchMetering = false;
        if (setMetering(m_params->metering) < 0) {
            ALOGE("%s(%d): FAIL set metering mode", __func__, __LINE__);
        }
    }

    return 0;
}

int SecCamera::SetRotate(int angle)
{
    ALOGE("%s(angle(%d))", __func__, angle);

    if (m_angle != angle) {
        switch (angle) {
        case -360:
        case    0:
        case  360:
            m_angle = 0;
            break;

        case -270:
        case   90:
            m_angle = 90;
            break;

        case -180:
        case  180:
            m_angle = 180;
            break;

        case  -90:
        case  270:
            m_angle = 270;
            break;

        default:
            ALOGE("ERR(%s):Invalid angle(%d)", __func__, angle);
            return -1;
        }

        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_ROTATION, angle) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_ROTATION", __func__);
                return -1;
            }
            m_angle = angle;
        }
    }

    return 0;
}

int SecCamera::getRotate(void)
{
    ALOGV("%s : angle(%d)", __func__, m_angle);
    return m_angle;
}

int SecCamera::setFrameRate(int frame_rate)
{
    ALOGV("%s(FrameRate(%d))", __func__, frame_rate);

    if (frame_rate < FRAME_RATE_AUTO || FRAME_RATE_MAX < frame_rate ) {
        ALOGE("ERR(%s):Invalid frame_rate(%d)", __func__, frame_rate);
        return -1;
    }

    if (m_params->capture.timeperframe.denominator != frame_rate) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FRAME_RATE, frame_rate) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FRAME_RATE", __func__);
                return -1;
            }
            m_params->capture.timeperframe.denominator = frame_rate;
        }
    }

    return 0;
}

int SecCamera::setVerticalMirror(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_VFLIP, 0) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_VFLIP", __func__);
        return -1;
    }

    return 0;
}

int SecCamera::setHorizontalMirror(void)
{
    ALOGV("%s :", __func__);

    if (m_cam_fd <= 0) {
        ALOGE("ERR(%s):Camera was closed", __func__);
        return -1;
    }

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_HFLIP, 0) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_HFLIP", __func__);
        return -1;
    }

    return 0;
}

int SecCamera::setWhiteBalance(int white_balance)
{
    ALOGV("%s(white_balance(%d))", __func__, white_balance);

    if (white_balance <= WHITE_BALANCE_BASE || WHITE_BALANCE_MAX <= white_balance) {
        ALOGE("ERR(%s):Invalid white_balance(%d)", __func__, white_balance);
        return -1;
    }

    if (m_params->white_balance != white_balance) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_WHITE_BALANCE, white_balance) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_WHITE_BALANCE", __func__);
                return -1;
            }
            m_params->white_balance = white_balance;
        }
    }

    return 0;
}

int SecCamera::getWhiteBalance(void)
{
    ALOGV("%s : white_balance(%d)", __func__, m_params->white_balance);
    return m_params->white_balance;
}

int SecCamera::setBrightness(int brightness)
{
    ALOGV("%s(brightness(%d))", __func__, brightness);

    if (m_camera_use_ISP) {
        brightness += IS_BRIGHTNESS_DEFAULT;
        if (brightness < IS_BRIGHTNESS_MINUS_2 || IS_BRIGHTNESS_PLUS_2 < brightness) {
            ALOGE("ERR(%s):Invalid brightness(%d)", __func__, brightness);
            return -1;
        }
    } else {
        ALOGW("WARN(%s):Not supported brightness setting", __func__);
        return 0;
    }

    if (m_params->brightness != brightness) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_BRIGHTNESS, brightness) < EV_MINUS_4) {
                ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_BRIGHTNESS", __func__);
                return -1;
            }
            m_params->brightness = brightness;
        }
    }

    return 0;
}

int SecCamera::getBrightness(void)
{
    ALOGV("%s : brightness(%d)", __func__, m_params->brightness);
    return m_params->brightness;
}

int SecCamera::setExposure(int exposure)
{
    ALOGV("%s(exposure(%d))", __func__, exposure);

    if (m_camera_use_ISP) {
        exposure += IS_EXPOSURE_DEFAULT;
        if (exposure < IS_EXPOSURE_MINUS_4 || IS_EXPOSURE_PLUS_4 < exposure) {
            ALOGE("ERR(%s):Invalid exposure(%d)", __func__, exposure);
            return -1;
        }
    } else {
        exposure += EV_DEFAULT;
        if (exposure < EV_MINUS_4 || EV_PLUS_4 < exposure) {
            ALOGE("ERR(%s):Invalid exposure(%d)", __func__, exposure);
            return -1;
        }
    }

    if (m_params->exposure != exposure) {
        if (m_flagCreate) {
            if (m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_EXPOSURE, exposure) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_EXPOSURE", __func__);
                    return -1;
                }
            } else {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_BRIGHTNESS, exposure) < EV_MINUS_4) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_BRIGHTNESS", __func__);
                    return -1;
                }
            }
            m_params->exposure = exposure;
        }
    }

    return 0;
}

int SecCamera::getExposure(void)
{
    ALOGV("%s : exposure(%d)", __func__, m_params->exposure);
    return m_params->exposure;
}

int SecCamera::setImageEffect(int image_effect)
{
    ALOGV("%s(image_effect(%d))", __func__, image_effect);

    if (image_effect <= IMAGE_EFFECT_BASE || IMAGE_EFFECT_MAX <= image_effect) {
        ALOGE("ERR(%s):Invalid image_effect(%d)", __func__, image_effect);
        return -1;
    }

    if (m_params->effects != image_effect) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_EFFECT, image_effect) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_EFFECT", __func__);
                return -1;
            }
            m_params->effects = image_effect;
        }
    }

    return 0;
}

int SecCamera::getImageEffect(void)
{
    ALOGV("%s : image_effect(%d)", __func__, m_params->effects);
    return m_params->effects;
}

int SecCamera::setAntiBanding(int anti_banding)
{
    ALOGV("%s(anti_banding(%d))", __func__, anti_banding);

    if (anti_banding < ANTI_BANDING_AUTO || ANTI_BANDING_OFF < anti_banding) {
        ALOGE("ERR(%s):Invalid anti_banding (%d)", __func__, anti_banding);
        return -1;
    }

    if (m_params->anti_banding != anti_banding) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ANTI_BANDING, anti_banding) < 0) {
                 ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ANTI_BANDING", __func__);
                 return -1;
            }
            m_params->anti_banding = anti_banding;
        }
    }

    return 0;
}

int SecCamera::setSceneMode(int scene_mode)
{
    ALOGV("%s(scene_mode(%d))", __func__, scene_mode);

    if (scene_mode <= SCENE_MODE_BASE || SCENE_MODE_MAX <= scene_mode) {
        ALOGE("ERR(%s):Invalid scene_mode (%d)", __func__, scene_mode);
        return -1;
    }

    if (m_params->scene_mode != scene_mode) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SCENE_MODE, scene_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SCENE_MODE", __func__);
                return -1;
            }
            m_params->scene_mode = scene_mode;
        }
    }

    return 0;
}

int SecCamera::getSceneMode(void)
{
    return m_params->scene_mode;
}

int SecCamera::setFlashMode(int flash_mode)
{
    ALOGV("%s(flash_mode(%d))", __func__, flash_mode);

    if (flash_mode <= FLASH_MODE_BASE || FLASH_MODE_MAX <= flash_mode) {
        ALOGE("ERR(%s):Invalid flash_mode (%d)", __func__, flash_mode);
        return -1;
    }

    if (m_params->flash_mode != flash_mode) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FLASH_MODE, flash_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FLASH_MODE", __func__);
                return -1;
            }
            m_params->flash_mode = flash_mode;
        }
    }

    return 0;
}

int SecCamera::getFlashMode(void)
{
    return m_params->flash_mode;
}

int SecCamera::setAutoExposureLock(int toggle)
{
    ALOGV("%s(toggle value(%d))", __func__, toggle);

    int aeawb_mode = m_params->aeawb_mode;

    if (m_flagCreate) {
        if (toggle ^ aeawb_mode) {
            aeawb_mode = aeawb_mode ^ 0x1;
            m_params->aeawb_mode = aeawb_mode;
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK, aeawb_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK", __func__);
                return -1;
            }
        }
    }
    return 0;
}

int SecCamera::setAutoWhiteBalanceLock(int toggle)
{
    ALOGV("%s(toggle value(%d))", __func__, toggle);

    int aeawb_mode = m_params->aeawb_mode;

    if (m_flagCreate) {
        if (toggle ^ (aeawb_mode >> 1)) {
            aeawb_mode = aeawb_mode ^ (0x1 << 1);
            m_params->aeawb_mode = aeawb_mode;
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK, aeawb_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK", __func__);
                return -1;
            }
        }
    }
    return 0;
}

int SecCamera::setISO(int iso_value)
{
    ALOGV("%s(iso_value(%d))", __func__, iso_value);

    if (iso_value < ISO_AUTO || ISO_MAX <= iso_value) {
        ALOGE("ERR(%s):Invalid iso_value (%d)", __func__, iso_value);
        return -1;
    }

    if (m_params->iso != iso_value) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ISO, iso_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ISO", __func__);
                return -1;
            }
            m_params->iso = iso_value;
        }
    }

    return 0;
}

int SecCamera::getISO(void)
{
    return m_params->iso;
}

int SecCamera::setContrast(int contrast_value)
{
    ALOGV("%s(contrast_value(%d))", __func__, contrast_value);

    if (m_camera_use_ISP) {
        if (contrast_value < IS_CONTRAST_AUTO || IS_CONTRAST_MAX <= contrast_value) {
            ALOGE("ERR(%s):Invalid contrast_value (%d)", __func__, contrast_value);
            return -1;
        }
    } else {
        if (contrast_value < CONTRAST_MINUS_2 || CONTRAST_MAX <= contrast_value) {
            ALOGE("ERR(%s):Invalid contrast_value (%d)", __func__, contrast_value);
            return -1;
        }
    }

    if (m_params->contrast != contrast_value) {
        if (m_flagCreate) {
            if (m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_CONTRAST, contrast_value) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_CONTRAST", __func__);
                    return -1;
                }
            } else {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_CONTRAST, contrast_value) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_CONTRAST", __func__);
                    return -1;
                }
            }
            m_params->contrast = contrast_value;
        }
    }

    return 0;
}

int SecCamera::getContrast(void)
{
    return m_params->contrast;
}

int SecCamera::setSaturation(int saturation_value)
{
    ALOGV("%s(saturation_value(%d))", __func__, saturation_value);

    saturation_value += SATURATION_DEFAULT;
    if (saturation_value < SATURATION_MINUS_2 || SATURATION_MAX <= saturation_value) {
        ALOGE("ERR(%s):Invalid saturation_value (%d)", __func__, saturation_value);
        return -1;
    }

    if (m_params->saturation != saturation_value) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SATURATION, saturation_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SATURATION", __func__);
                return -1;
            }
            m_params->saturation = saturation_value;
        }
    }

    return 0;
}

int SecCamera::getSaturation(void)
{
    return m_params->saturation;
}

int SecCamera::setSharpness(int sharpness_value)
{
    ALOGV("%s(sharpness_value(%d))", __func__, sharpness_value);

    sharpness_value += SHARPNESS_DEFAULT;
    if (sharpness_value < SHARPNESS_MINUS_2 || SHARPNESS_MAX <= sharpness_value) {
        ALOGE("ERR(%s):Invalid sharpness_value (%d)", __func__, sharpness_value);
        return -1;
    }

    if (m_params->sharpness != sharpness_value) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SHARPNESS, sharpness_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SHARPNESS", __func__);
                return -1;
            }
            m_params->sharpness = sharpness_value;
        }
    }

    return 0;
}

int SecCamera::getSharpness(void)
{
    return m_params->sharpness;
}

int SecCamera::setHue(int hue_value)
{
    ALOGV("%s(hue_value(%d))", __func__, hue_value);

    if (m_camera_use_ISP) {
        hue_value += IS_HUE_DEFAULT;
        if (hue_value < IS_HUE_MINUS_2 || IS_HUE_MAX <= hue_value) {
            ALOGE("ERR(%s):Invalid hue_value (%d)", __func__, hue_value);
            return -1;
        }
    } else {
            ALOGW("WARN(%s):Not supported hue setting", __func__);
            return 0;
    }

    if (m_params->hue != hue_value) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_HUE, hue_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_HUE", __func__);
                return -1;
            }
            m_params->hue = hue_value;
        }
    }

    return 0;
}

int SecCamera::getHue(void)
{
    return m_params->hue;
}

int SecCamera::setWDR(int wdr_value)
{
    ALOGV("%s(wdr_value(%d))", __func__, wdr_value);

    if (m_camera_use_ISP) {
        if (wdr_value < IS_DRC_BYPASS_DISABLE || IS_DRC_BYPASS_MAX <= wdr_value) {
            ALOGE("ERR(%s):Invalid drc_value (%d)", __func__, wdr_value);
            return -1;
        }
    } else {
        if (wdr_value < WDR_OFF || WDR_MAX <= wdr_value) {
            ALOGE("ERR(%s):Invalid wdr_value (%d)", __func__, wdr_value);
            return -1;
        }
    }

    if (m_wdr != wdr_value) {
        if (m_flagCreate) {
            if (m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_SET_DRC, wdr_value) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_IS_SET_DRC", __func__);
                    return -1;
                }
            } else {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_WDR, wdr_value) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_WDR", __func__);
                    return -1;
                }
            }
            m_wdr = wdr_value;
        }
    }

    return 0;
}

int SecCamera::getWDR(void)
{
    return m_wdr;
}

int SecCamera::setAntiShake(int anti_shake)
{
    ALOGV("%s(anti_shake(%d))", __func__, anti_shake);

    if (anti_shake < ANTI_SHAKE_OFF || ANTI_SHAKE_MAX <= anti_shake) {
        ALOGE("ERR(%s):Invalid anti_shake (%d)", __func__, anti_shake);
        return -1;
    }

    if (m_anti_shake != anti_shake) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ANTI_SHAKE, anti_shake) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ANTI_SHAKE", __func__);
                return -1;
            }
            m_anti_shake = anti_shake;
        }
    }

    return 0;
}

int SecCamera::getAntiShake(void)
{
    return m_anti_shake;
}

int SecCamera::setMetering(int metering_value)
{
    ALOGV("%s(metering (%d))", __func__, metering_value);

    if (m_camera_use_ISP) {
        if (metering_value < IS_METERING_AVERAGE || IS_METERING_MAX <= metering_value) {
            ALOGE("ERR(%s):Invalid metering_value (%d)", __func__, metering_value);
            return -1;
        }
    } else {
        if (metering_value <= METERING_BASE || METERING_MAX <= metering_value) {
            ALOGE("ERR(%s):Invalid metering_value (%d)", __func__, metering_value);
            return -1;
        }
    }

    if (m_flagCreate) {
        if (m_camera_use_ISP) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING, metering_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_IS_CAMERA_METERING", __func__);
                return -1;
            }
        } else {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_METERING, metering_value) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_METERING", __func__);
                return -1;
            }
        }
        if (!m_isTouchMetering)
            m_params->metering = metering_value;
    }

    return 0;
}

int SecCamera::getMetering(void)
{
    return m_params->metering;
}

bool SecCamera::setMeteringAreas(int num, ExynosRect2 *rect2s, int *weights)
{
    int maxNumMeteringAreas = getMaxNumMeteringAreas();

    if (maxNumMeteringAreas == 0) {
        ALOGV("DEBUG(%s):maxNumMeteringAreas is 0. so, ignored", __func__);
        return true;
    }

    if (   rect2s[0].w == 0
        && rect2s[0].h == 0
        && weights[0] == 0) {
        ALOGV("%s(%d): Set intial value", __func__, __LINE__);
        return true;
    }

    if (maxNumMeteringAreas < num)
        num = maxNumMeteringAreas;

    for (int i = 0; i < num; i++) {
        if (   rect2s[i].x < 0
            || rect2s[i].y < 0
            || rect2s[i].w < 0
            || rect2s[i].h < 0) {
            ALOGW("%s(%d): Invalid Metering area", __func__, __LINE__);
            return true;
        }
    }

    if (m_flagCreate == true) {
        for (int i = 0; i < num; i++) {
            if (   fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING_POSITION_X, rect2s[i].x) < 0
                || fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING_POSITION_Y, rect2s[i].y) < 0
                || fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING_WINDOW_X,   rect2s[i].w) < 0
                || fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_METERING_WINDOW_Y,   rect2s[i].h) < 0) {
                ALOGE("ERR(%s):fimc_v4l2_s_ctrl() fail", __func__);
                return false;
            }
        }
    }

    m_isTouchMetering = true;

    if (setMetering(IS_METERING_SPOT) < 0) {
        ALOGE("%s(%d): FAIL set metering mode", __func__, __LINE__);
        m_isTouchMetering = false;
    }

    return true;
}

int SecCamera::getMaxNumMeteringAreas(void)
{
    if (m_camera_use_ISP)
        return MAX_METERING_AREA;
    else
        return 0;
}

int SecCamera::setJpegQuality(int jpeg_quality)
{
    ALOGV("%s(jpeg_quality (%d))", __func__, jpeg_quality);

    if (jpeg_quality < JPEG_QUALITY_ECONOMY || JPEG_QUALITY_MAX <= jpeg_quality) {
        ALOGE("ERR(%s):Invalid jpeg_quality (%d)", __func__, jpeg_quality);
        return -1;
    }

    if (m_jpeg_quality != jpeg_quality) {
        m_jpeg_quality = jpeg_quality;
        if (m_flagCreate && !m_camera_use_ISP) {
            jpeg_quality -= 5;
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAM_JPEG_QUALITY, jpeg_quality) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAM_JPEG_QUALITY", __func__);
                return -1;
            }
        }
    }

    return 0;
}

int SecCamera::getJpegQuality(void)
{
    return m_jpeg_quality;
}

int SecCamera::setZoom(int zoom_level)
{
    ALOGV("%s(zoom_level (%d))", __func__, zoom_level);

    if (zoom_level < ZOOM_LEVEL_0 || ZOOM_LEVEL_MAX <= zoom_level) {
        ALOGE("ERR(%s):Invalid zoom_level (%d)", __func__, zoom_level);
        return -1;
    }

    if (m_zoom_level != zoom_level) {
        if (m_flagCreate) {
            if (!m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_ZOOM, zoom_level) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_ZOOM", __func__);
                    return -1;
                }
            }
            m_zoom_level = zoom_level;
        }
    }

    return 0;
}

int SecCamera::getZoom(void)
{
    return m_zoom_level;
}

int SecCamera::setObjectTracking(int object_tracking)
{
    ALOGV("%s(object_tracking (%d))", __func__, object_tracking);

    if (object_tracking < OBJECT_TRACKING_OFF || OBJECT_TRACKING_MAX <= object_tracking) {
        ALOGE("ERR(%s):Invalid object_tracking (%d)", __func__, object_tracking);
        return -1;
    }

    if (m_object_tracking != object_tracking)
        m_object_tracking = object_tracking;

    return 0;
}

int SecCamera::getObjectTracking(void)
{
    return m_object_tracking;
}

int SecCamera::getObjectTrackingStatus(void)
{
    int obj_status = 0;
    obj_status = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_OBJ_TRACKING_STATUS);
    return obj_status;
}

int SecCamera::setObjectTrackingStartStop(int start_stop)
{
    ALOGV("%s(object_tracking_start_stop (%d))", __func__, start_stop);

    if (m_object_tracking_start_stop != start_stop) {
        m_object_tracking_start_stop = start_stop;
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP, start_stop) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP", __func__);
            return -1;
        }
    }

    return 0;
}

int SecCamera::setTouchAFStartStop(int start_stop)
{
    ALOGV("%s(touch_af_start_stop (%d))", __func__, start_stop);

    if (m_touch_af_start_stop != start_stop) {
        m_touch_af_start_stop = start_stop;
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_TOUCH_AF_START_STOP, start_stop) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_TOUCH_AF_START_STOP", __func__);
            return -1;
        }
    }

    return 0;
}

int SecCamera::setSmartAuto(int smart_auto)
{
    ALOGV("%s(smart_auto (%d))", __func__, smart_auto);

    if (smart_auto < SMART_AUTO_OFF || SMART_AUTO_MAX <= smart_auto) {
        ALOGE("ERR(%s):Invalid smart_auto (%d)", __func__, smart_auto);
        return -1;
    }

    if (m_smart_auto != smart_auto) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SMART_AUTO, smart_auto) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SMART_AUTO", __func__);
                return -1;
            }
            m_smart_auto = smart_auto;
        }
    }

    return 0;
}

int SecCamera::getSmartAuto(void)
{
    return m_smart_auto;
}

int SecCamera::getAutosceneStatus(void)
{
    int autoscene_status = -1;

    if (getSmartAuto() == SMART_AUTO_ON) {
        autoscene_status = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_SMART_AUTO_STATUS);

        if ((autoscene_status < SMART_AUTO_STATUS_AUTO) || (autoscene_status > SMART_AUTO_STATUS_MAX)) {
            ALOGE("ERR(%s):Invalid getAutosceneStatus (%d)", __func__, autoscene_status);
            return -1;
        }
    }
    return autoscene_status;
}

int SecCamera::setBeautyShot(int beauty_shot)
{
    ALOGV("%s(beauty_shot (%d))", __func__, beauty_shot);

    if (beauty_shot < BEAUTY_SHOT_OFF || BEAUTY_SHOT_MAX <= beauty_shot) {
        ALOGE("ERR(%s):Invalid beauty_shot (%d)", __func__, beauty_shot);
        return -1;
    }

    if (m_beauty_shot != beauty_shot) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_BEAUTY_SHOT, beauty_shot) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_BEAUTY_SHOT", __func__);
                return -1;
            }
            m_beauty_shot = beauty_shot;
        }

        setFaceDetect(FACE_DETECTION_ON_BEAUTY);
    }

    return 0;
}

int SecCamera::getBeautyShot(void)
{
    return m_beauty_shot;
}

int SecCamera::setVintageMode(int vintage_mode)
{
    ALOGV("%s(vintage_mode(%d))", __func__, vintage_mode);

    if (vintage_mode <= VINTAGE_MODE_BASE || VINTAGE_MODE_MAX <= vintage_mode) {
        ALOGE("ERR(%s):Invalid vintage_mode (%d)", __func__, vintage_mode);
        return -1;
    }

    if (m_vintage_mode != vintage_mode) {
        if (m_flagCreate) {
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_VINTAGE_MODE, vintage_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_VINTAGE_MODE", __func__);
                return -1;
            }
            m_vintage_mode = vintage_mode;
        }
    }

    return 0;
}

int SecCamera::getVintageMode(void)
{
    return m_vintage_mode;
}

int SecCamera::setFocusMode(int focus_mode)
{
    ALOGV("%s(focus_mode(%d))", __func__, focus_mode);

        if (FOCUS_MODE_MAX <= focus_mode) {
            ALOGE("ERR(%s):Invalid focus_mode (%d)", __func__, focus_mode);
            return -1;
        }

    if (m_params->focus_mode != focus_mode) {
        if (m_flagCreate) {
            if (m_params->focus_mode == FOCUS_MODE_AUTO || m_params->focus_mode == FOCUS_MODE_MACRO) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_AUTO_FOCUS, AUTO_FOCUS_OFF) < 0) {
                        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_AUTO_FOCUS", __func__);
                        return -1;
                }
            }
            if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FOCUS_MODE, focus_mode) < 0) {
                ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FOCUS_MODE", __func__);
                return -1;
            }
            if (!m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_AUTO_FOCUS, AUTO_FOCUS_ON) < 0) {
                        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_AUTO_FOCUS", __func__);
                        return -1;
                }
            }
            m_params->focus_mode = focus_mode;
        }
    }

    return 0;
}

int SecCamera::getFocusMode(void)
{
    return m_params->focus_mode;
}

int SecCamera::setFaceDetect(int face_detect)
{
    ALOGV("%s(face_detect(%d))", __func__, face_detect);
    if (m_camera_use_ISP) {
        if (face_detect < IS_FD_COMMAND_STOP || IS_FD_COMMAND_MAX <= face_detect) {
            ALOGE("ERR(%s):Invalid face_detect value (%d)", __func__, face_detect);
            return -1;
        }
    } else {
        if (face_detect < FACE_DETECTION_OFF || FACE_DETECTION_MAX <= face_detect) {
            ALOGE("ERR(%s):Invalid face_detect value (%d)", __func__, face_detect);
            return -1;
        }
    }

    if (m_face_detect != face_detect) {
        if (m_flagCreate) {
            if (m_camera_use_ISP) {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_IS_CMD_FD, face_detect) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_IS_CMD_FD", __func__);
                    return -1;
                }
            } else {
                if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FACE_DETECTION, face_detect) < 0) {
                    ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FACE_DETECTION", __func__);
                    return -1;
                }
            }
            m_face_detect = face_detect;
        }
    }

    return 0;
}

int SecCamera::getFaceDetect(void)
{
    return m_face_detect;
}

int SecCamera::setGPSLatitude(const char *gps_latitude)
{
    double conveted_latitude = 0;
    ALOGV("%s(gps_latitude(%s))", __func__, gps_latitude);
    if (gps_latitude == NULL)
        m_gps_latitude = 0;
    else {
        conveted_latitude = atof(gps_latitude);
        m_gps_latitude = (long)(conveted_latitude * 10000 / 1);
    }

    ALOGV("%s(m_gps_latitude(%ld))", __func__, m_gps_latitude);
    return 0;
}

int SecCamera::setGPSLongitude(const char *gps_longitude)
{
    double conveted_longitude = 0;
    ALOGV("%s(gps_longitude(%s))", __func__, gps_longitude);
    if (gps_longitude == NULL)
        m_gps_longitude = 0;
    else {
        conveted_longitude = atof(gps_longitude);
        m_gps_longitude = (long)(conveted_longitude * 10000 / 1);
    }

    ALOGV("%s(m_gps_longitude(%ld))", __func__, m_gps_longitude);
    return 0;
}

int SecCamera::setGPSAltitude(const char *gps_altitude)
{
    double conveted_altitude = 0;
    ALOGV("%s(gps_altitude(%s))", __func__, gps_altitude);
    if (gps_altitude == NULL)
        m_gps_altitude = 0;
    else {
        conveted_altitude = atof(gps_altitude);
        m_gps_altitude = (long)(conveted_altitude * 100 / 1);
    }

    ALOGV("%s(m_gps_altitude(%ld))", __func__, m_gps_altitude);
    return 0;
}

int SecCamera::setGPSTimeStamp(const char *gps_timestamp)
{
    ALOGV("%s(gps_timestamp(%s))", __func__, gps_timestamp);
    if (gps_timestamp == NULL)
        m_gps_timestamp = 0;
    else
        m_gps_timestamp = atol(gps_timestamp);

    ALOGV("%s(m_gps_timestamp(%ld))", __func__, m_gps_timestamp);
    return 0;
}

int SecCamera::setGPSProcessingMethod(const char *gps_processing_method)
{
    ALOGV("%s(gps_processing_method(%s))", __func__, gps_processing_method);
    memset(mExifInfo.gps_processing_method, 0, sizeof(mExifInfo.gps_processing_method));
    if (gps_processing_method != NULL) {
        size_t len = strlen(gps_processing_method);
        if (len > sizeof(mExifInfo.gps_processing_method)) {
            len = sizeof(mExifInfo.gps_processing_method);
        }
        memcpy(mExifInfo.gps_processing_method, gps_processing_method, len);
    }
    return 0;
}

int SecCamera::setFaceDetectLockUnlock(int facedetect_lockunlock)
{
    ALOGV("%s(facedetect_lockunlock(%d))", __func__, facedetect_lockunlock);

    if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK, facedetect_lockunlock) < 0) {
        ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK", __func__);
        return -1;
    }

    return 0;
}

int SecCamera::setObjectPosition(int x, int y)
{
    ALOGV("%s(setObjectPosition(x=%d, y=%d))", __func__, x, y);

    /* crop input size
     * '*3/4' : 4x zoom
     * '/31' : 31 steps
     */
    int step = 31;
    float zoom[step];
    float inc = 0.1;
    int new_x, new_y;
    zoom[0] = 1.0;
    for (int n = 0; n < (step - 1); n++) {
        zoom[n+1] = zoom[n] + inc;
    }

    /* Converting axis and Calcurating x,y position.
     * Because driver need (x, y) point.
     */
    new_x = (int)(((x / zoom[m_zoom_level]) + 1000) * 1023 / 2000);
    new_y = (int)(((y / zoom[m_zoom_level]) + 1000) * 1023 / 2000);

    if (m_flag_camera_start) {
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_OBJECT_POSITION_X, new_x) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_OBJECT_POSITION_X", __func__);
            return -1;
        }
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_OBJECT_POSITION_Y, new_y) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_OBJECT_POSITION_Y", __func__);
            return -1;
        }
    }

    return 0;
}

int SecCamera::setGamma(int gamma)
{
     ALOGV("%s(gamma(%d))", __func__, gamma);

     if (gamma < GAMMA_OFF || GAMMA_MAX <= gamma) {
         ALOGE("ERR(%s):Invalid gamma (%d)", __func__, gamma);
         return -1;
     }

     if (m_video_gamma != gamma) {
         if (m_flagCreate) {
             if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_GAMMA, gamma) < 0) {
                 ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_GAMMA", __func__);
                 return -1;
             }
            m_video_gamma = gamma;
         }
     }

     return 0;
}

int SecCamera::setSlowAE(int slow_ae)
{
     ALOGV("%s(slow_ae(%d))", __func__, slow_ae);

     if (slow_ae < GAMMA_OFF || GAMMA_MAX <= slow_ae) {
         ALOGE("ERR(%s):Invalid slow_ae (%d)", __func__, slow_ae);
         return -1;
     }

     if (m_slow_ae!= slow_ae) {
         if (m_flagCreate) {
             if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_SET_SLOW_AE, slow_ae) < 0) {
                 ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_SET_SLOW_AE", __func__);
                 return -1;
             }
            m_slow_ae = slow_ae;
         }
     }

     return 0;
}

int SecCamera::setRecordingSize(int width, int height)
{
     ALOGV("%s(width(%d), height(%d))", __func__, width, height);

     m_recording_width  = width;
     m_recording_height = height;

     return 0;
}

int SecCamera::getRecordingSize(int *width, int *height)
{
    *width  = m_recording_width;
    *height = m_recording_height;

    return 0;
}

int SecCamera::setExifOrientationInfo(int orientationInfo)
{
     ALOGV("%s(orientationInfo(%d))", __func__, orientationInfo);

     if (orientationInfo < 0) {
         ALOGE("ERR(%s):Invalid orientationInfo (%d)", __func__, orientationInfo);
         return -1;
     }
     m_exif_orientation = orientationInfo;

     return 0;
}

int SecCamera::setBatchReflection()
{
    if (m_flagCreate) {
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_BATCH_REFLECTION, 1) < 0) {
             ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_BATCH_REFLECTION", __func__);
             return -1;
        }
    }

    return 0;
}

/* Camcorder fix fps */
int SecCamera::setSensorMode(int sensor_mode)
{
    ALOGV("%s(sensor_mode (%d))", __func__, sensor_mode);

    if (sensor_mode < SENSOR_MODE_CAMERA || SENSOR_MODE_MOVIE < sensor_mode) {
        ALOGE("ERR(%s):Invalid sensor mode (%d)", __func__, sensor_mode);
        return -1;
    }

    if (m_sensor_mode != sensor_mode)
        m_sensor_mode = sensor_mode;

    return 0;
}

/*  Shot mode   */
/*  SINGLE = 0
*   CONTINUOUS = 1
*   PANORAMA = 2
*   SMILE = 3
*   SELF = 6
*/
int SecCamera::setShotMode(int shot_mode)
{
    ALOGV("%s(shot_mode (%d))", __func__, shot_mode);
    if (shot_mode < SHOT_MODE_SINGLE || SHOT_MODE_SELF < shot_mode) {
        ALOGE("ERR(%s):Invalid shot_mode (%d)", __func__, shot_mode);
        return -1;
    }
    m_shot_mode = shot_mode;

    return 0;
}

int SecCamera::setDataLineCheck(int chk_dataline)
{
    ALOGV("%s(chk_dataline (%d))", __func__, chk_dataline);

    if (chk_dataline < CHK_DATALINE_OFF || CHK_DATALINE_MAX <= chk_dataline) {
        ALOGE("ERR(%s):Invalid chk_dataline (%d)", __func__, chk_dataline);
        return -1;
    }

    m_chk_dataline = chk_dataline;

    return 0;
}

int SecCamera::getDataLineCheck(void)
{
    return m_chk_dataline;
}

int SecCamera::setDataLineCheckStop(void)
{
    ALOGV("%s", __func__);

    if (m_flagCreate) {
        if (fimc_v4l2_s_ctrl(m_cam_fd, V4L2_CID_CAMERA_CHECK_DATALINE_STOP, 1) < 0) {
            ALOGE("ERR(%s):Fail on V4L2_CID_CAMERA_CHECK_DATALINE_STOP", __func__);
            return -1;
        }
    }
    return 0;
}

const __u8* SecCamera::getCameraSensorName(void)
{
    ALOGV("%s", __func__);

    return fimc_v4l2_enuminput(m_cam_fd, getCameraId());
}

bool SecCamera::getUseInternalISP(void)
{
    ALOGV("%s", __func__);
    int ret = 0;

/*TODO*/
    if (!strncmp((const char*)getCameraSensorName(), "ISP Camera", 10))
        return true;
    else if(!strncmp((const char*)getCameraSensorName(), "S5K3H2", 10))
        return true;
    else if(!strncmp((const char*)getCameraSensorName(), "S5K3H7", 10))
        return true;
    else if(!strncmp((const char*)getCameraSensorName(), "S5K4E5", 10))
        return true;
    else if(!strncmp((const char*)getCameraSensorName(), "S5K6A3", 10))
        return true;
    else
        return false;
}

bool SecCamera::setMaxSize(void)
{
    ALOGV("%s", __func__);

    if (!strncmp((const char*)getCameraSensorName(), "ISP Camera", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 1392;
        m_snapshot_max_height = 1392;
        return true;
    } else if(!strncmp((const char*)getCameraSensorName(), "S5K3H2", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 3248;
        m_snapshot_max_height = 2436;
        return true;
    } else if(!strncmp((const char*)getCameraSensorName(), "S5K3H7", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 3248;
        m_snapshot_max_height = 2436;
        return true;
    } else if(!strncmp((const char*)getCameraSensorName(), "S5K4E5", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 2560;
        m_snapshot_max_height = 1920;
        return true;
    } else if(!strncmp((const char*)getCameraSensorName(), "S5K6A3", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 1392;
        m_snapshot_max_height = 1392;
        return true;
    } else if(!strncmp((const char*)getCameraSensorName(), "M5MO", 10)) {
        m_preview_max_width   = 640;
        m_preview_max_height  = 480;
        m_snapshot_max_width  = 3264;
        m_snapshot_max_height = 2448;
        return true;
    } else
        return false;
}

#ifdef ENABLE_ESD_PREVIEW_CHECK
int SecCamera::getCameraSensorESDStatus(void)
{
    ALOGV("%s", __func__);

    // 0 : normal operation, 1 : abnormal operation
    int status = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_ESD_INT);

    return status;
}
#endif // ENABLE_ESD_PREVIEW_CHECK

int SecCamera::setJpegThumbnailSize(int width, int height)
{
    ALOGV("%s(width(%d), height(%d))", __func__, width, height);

    m_jpeg_thumbnail_width  = width;
    m_jpeg_thumbnail_height = height;

    return 0;
}

int SecCamera::getJpegThumbnailSize(int *width, int  *height)
{
    if (width)
        *width   = m_jpeg_thumbnail_width;
    if (height)
        *height  = m_jpeg_thumbnail_height;

    return 0;
}

int SecCamera::setJpegThumbnailQuality(int jpeg_thumbnail_quality)
{
    ALOGV("%s(jpeg_thumbnail_quality (%d))", __func__, jpeg_thumbnail_quality);

    if (jpeg_thumbnail_quality < JPEG_QUALITY_ECONOMY || JPEG_QUALITY_MAX <= jpeg_thumbnail_quality) {
        ALOGE("ERR(%s):Invalid jpeg_thumbnail_quality (%d)", __func__, jpeg_thumbnail_quality);
        return -1;
    }

    if (m_jpeg_thumbnail_quality != jpeg_thumbnail_quality) {
        m_jpeg_thumbnail_quality = jpeg_thumbnail_quality;
    }

    return 0;
}

int SecCamera::getJpegThumbnailQuality(void)
{
    return m_jpeg_thumbnail_quality;
}

void SecCamera::setExifFixedAttribute()
{
    char property[PROPERTY_VALUE_MAX];

    //2 0th IFD TIFF Tags
    //3 Maker
    property_get("ro.product.brand", property, EXIF_DEF_MAKER);
    strncpy((char *)mExifInfo.maker, property,
                sizeof(mExifInfo.maker) - 1);
    mExifInfo.maker[sizeof(mExifInfo.maker) - 1] = '\0';
    //3 Model
    property_get("ro.product.model", property, EXIF_DEF_MODEL);
    strncpy((char *)mExifInfo.model, property,
                sizeof(mExifInfo.model) - 1);
    mExifInfo.model[sizeof(mExifInfo.model) - 1] = '\0';
    //3 Software
    property_get("ro.build.id", property, EXIF_DEF_SOFTWARE);
    strncpy((char *)mExifInfo.software, property,
                sizeof(mExifInfo.software) - 1);
    mExifInfo.software[sizeof(mExifInfo.software) - 1] = '\0';

    //3 YCbCr Positioning
    mExifInfo.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    //2 0th IFD Exif Private Tags
    //3 F Number
    mExifInfo.fnumber.num = EXIF_DEF_FNUMBER_NUM;
    mExifInfo.fnumber.den = EXIF_DEF_FNUMBER_DEN;
    //3 Exposure Program
    mExifInfo.exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    //3 Exif Version
    memcpy(mExifInfo.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(mExifInfo.exif_version));
    //3 Aperture
    uint32_t av = APEX_FNUM_TO_APERTURE((double)mExifInfo.fnumber.num/mExifInfo.fnumber.den);
    mExifInfo.aperture.num = av*EXIF_DEF_APEX_DEN;
    mExifInfo.aperture.den = EXIF_DEF_APEX_DEN;
    //3 Maximum lens aperture
    mExifInfo.max_aperture.num = mExifInfo.aperture.num;
    mExifInfo.max_aperture.den = mExifInfo.aperture.den;
    //3 Lens Focal Length
    if (m_camera_id == CAMERA_ID_BACK)
        mExifInfo.focal_length.num = BACK_CAMERA_FOCAL_LENGTH;
    else
        mExifInfo.focal_length.num = FRONT_CAMERA_FOCAL_LENGTH;

    mExifInfo.focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;
    //3 User Comments
    strcpy((char *)mExifInfo.user_comment, EXIF_DEF_USERCOMMENTS);
    //3 Color Space information
    mExifInfo.color_space = EXIF_DEF_COLOR_SPACE;
    //3 Exposure Mode
    mExifInfo.exposure_mode = EXIF_DEF_EXPOSURE_MODE;

    //2 0th IFD GPS Info Tags
    unsigned char gps_version[4] = { 0x02, 0x02, 0x00, 0x00 };
    memcpy(mExifInfo.gps_version_id, gps_version, sizeof(gps_version));

    //2 1th IFD TIFF Tags
    mExifInfo.compression_scheme = EXIF_DEF_COMPRESSION;
    mExifInfo.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    mExifInfo.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    mExifInfo.y_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    mExifInfo.y_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    mExifInfo.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
}

void SecCamera::setExifChangedAttribute()
{
    //2 0th IFD TIFF Tags
    //3 Width
    mExifInfo.width = m_snapshot_width;
    //3 Height
    mExifInfo.height = m_snapshot_height;

    //3 Orientation
    switch (m_exif_orientation) {
    case 90:
        mExifInfo.orientation = EXIF_ORIENTATION_90;
        break;
    case 180:
        mExifInfo.orientation = EXIF_ORIENTATION_180;
        break;
    case 270:
        mExifInfo.orientation = EXIF_ORIENTATION_270;
        break;
    case 0:
    default:
        mExifInfo.orientation = EXIF_ORIENTATION_UP;
        break;
    }
    //3 Date time
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime((char *)mExifInfo.date_time, 20, "%Y:%m:%d %H:%M:%S", timeinfo);

    //2 0th IFD Exif Private Tags
    //3 Exposure Time
    int shutterSpeed = 100;
    if (m_camera_use_ISP) {
        shutterSpeed = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_EXIF_SHUTTERSPEED);
        if (shutterSpeed <= 0) {
            ALOGE("%s: error %d getting shutterSpeed, camera_id = %d, using 100",
                 __func__, shutterSpeed, m_camera_id);
            shutterSpeed = 100;
        }
    } else {
        shutterSpeed = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_EXIF_TV);
        if (shutterSpeed <= 0) {
            ALOGE("%s: error %d getting shutterSpeed, camera_id = %d, using 100",
                 __func__, shutterSpeed, m_camera_id);
            shutterSpeed = 100;
        }
    }

    /* TODO : external isp is not shuppoting exptime now. */
    int exptime = 100;
    if (m_camera_use_ISP) {
        exptime = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_EXIF_EXPTIME);
        if (exptime <= 0) {
            ALOGE("%s: error %d getting exposure time, camera_id = %d, using 100",
                 __func__, exptime, m_camera_id);
            exptime = 100;
        }
    }
    mExifInfo.exposure_time.num = 1;
    mExifInfo.exposure_time.den = (uint32_t)exptime;

    /* TODO : Normaly exposure time and shutter speed is same. But we need to  */
    /*        calculate exactly value. */
    shutterSpeed = exptime;

    //3 ISO Speed Rating
    int iso;
    if (m_camera_use_ISP)
        iso = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_IS_CAMERA_EXIF_ISO);
    else
        iso = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_EXIF_ISO);
    if (iso < 0) {
        ALOGE("%s: error %d getting iso, camera_id = %d, using 100",
             __func__, iso, m_camera_id);
        iso = 0;
    }
    mExifInfo.iso_speed_rating = iso;

    uint32_t av, tv, bv, sv, ev;
    if (m_camera_use_ISP) {
        av = APEX_FNUM_TO_APERTURE((double)mExifInfo.fnumber.num / mExifInfo.fnumber.den);
        tv = APEX_EXPOSURE_TO_SHUTTER((double)mExifInfo.exposure_time.num / mExifInfo.exposure_time.den);
        sv = APEX_ISO_TO_FILMSENSITIVITY(mExifInfo.iso_speed_rating);
        bv = av + tv - sv;
        ev = m_params->exposure - IS_EXPOSURE_DEFAULT;
    } else {
        av = APEX_FNUM_TO_APERTURE((double)mExifInfo.fnumber.num / mExifInfo.fnumber.den);
        tv = shutterSpeed;
        sv = APEX_ISO_TO_FILMSENSITIVITY(mExifInfo.iso_speed_rating);
        bv = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_EXIF_BV);
        ev = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_EXIF_EBV);
    }
    ALOGD("Shutter speed=1/%d s, iso=%d", shutterSpeed, mExifInfo.iso_speed_rating);
    ALOGD("AV=%d, TV=%d, SV=%d, BV=%d, EV=%d", av, tv, sv, bv, ev);

    //3 Shutter Speed
    mExifInfo.shutter_speed.num = 1;
    mExifInfo.shutter_speed.den = shutterSpeed;
    //3 Brightness
    mExifInfo.brightness.num = bv*EXIF_DEF_APEX_DEN;
    mExifInfo.brightness.den = EXIF_DEF_APEX_DEN;
    //3 Exposure Bias
    if (m_params->scene_mode == SCENE_MODE_BEACH_SNOW) {
        mExifInfo.exposure_bias.num = EXIF_DEF_APEX_DEN;
        mExifInfo.exposure_bias.den = EXIF_DEF_APEX_DEN;
    } else {
        mExifInfo.exposure_bias.num = ev*EXIF_DEF_APEX_DEN;
        mExifInfo.exposure_bias.den = EXIF_DEF_APEX_DEN;
    }
    //3 Metering Mode
    switch (m_params->metering) {
    case METERING_SPOT:
        mExifInfo.metering_mode = EXIF_METERING_SPOT;
        break;
    case METERING_MATRIX:
        mExifInfo.metering_mode = EXIF_METERING_AVERAGE;
        break;
    case METERING_CENTER:
        mExifInfo.metering_mode = EXIF_METERING_CENTER;
        break;
    default :
        mExifInfo.metering_mode = EXIF_METERING_AVERAGE;
        break;
    }

    //3 Flash
    int flash = m_params->flash_mode;
    //int flash = fimc_v4l2_g_ctrl(m_cam_fd, V4L2_CID_CAMERA_GET_FLASH_ONOFF);
    if (flash < 0)
        mExifInfo.flash = EXIF_DEF_FLASH;
    else
        mExifInfo.flash = flash;

    //3 White Balance
    if (m_params->white_balance == WHITE_BALANCE_AUTO || m_params->white_balance == IS_AWB_AUTO)
        mExifInfo.white_balance = EXIF_WB_AUTO;
    else
        mExifInfo.white_balance = EXIF_WB_MANUAL;
    //3 Scene Capture Type
    switch (m_params->scene_mode) {
    case SCENE_MODE_PORTRAIT:
        mExifInfo.scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    case SCENE_MODE_LANDSCAPE:
        mExifInfo.scene_capture_type = EXIF_SCENE_LANDSCAPE;
        break;
    case SCENE_MODE_NIGHTSHOT:
        mExifInfo.scene_capture_type = EXIF_SCENE_NIGHT;
        break;
    default:
        mExifInfo.scene_capture_type = EXIF_SCENE_STANDARD;
        break;
    }

    //2 0th IFD GPS Info Tags
    if (m_gps_latitude != 0 && m_gps_longitude != 0) {
        if (m_gps_latitude > 0)
            strcpy((char *)mExifInfo.gps_latitude_ref, "N");
        else
            strcpy((char *)mExifInfo.gps_latitude_ref, "S");

        if (m_gps_longitude > 0)
            strcpy((char *)mExifInfo.gps_longitude_ref, "E");
        else
            strcpy((char *)mExifInfo.gps_longitude_ref, "W");

        if (m_gps_altitude > 0)
            mExifInfo.gps_altitude_ref = 0;
        else
            mExifInfo.gps_altitude_ref = 1;

        double latitude = fabs(m_gps_latitude / 10000.0);
        double longitude = fabs(m_gps_longitude / 10000.0);
        double altitude = fabs(m_gps_altitude / 100.0);

        mExifInfo.gps_latitude[0].num = (uint32_t)latitude;
        mExifInfo.gps_latitude[0].den = 1;
        mExifInfo.gps_latitude[1].num = (uint32_t)((latitude - mExifInfo.gps_latitude[0].num) * 60);
        mExifInfo.gps_latitude[1].den = 1;
        mExifInfo.gps_latitude[2].num = (uint32_t)((((latitude - mExifInfo.gps_latitude[0].num) * 60)
                                        - mExifInfo.gps_latitude[1].num) * 60);
        mExifInfo.gps_latitude[2].den = 1;

        mExifInfo.gps_longitude[0].num = (uint32_t)longitude;
        mExifInfo.gps_longitude[0].den = 1;
        mExifInfo.gps_longitude[1].num = (uint32_t)((longitude - mExifInfo.gps_longitude[0].num) * 60);
        mExifInfo.gps_longitude[1].den = 1;
        mExifInfo.gps_longitude[2].num = (uint32_t)((((longitude - mExifInfo.gps_longitude[0].num) * 60)
                                        - mExifInfo.gps_longitude[1].num) * 60);
        mExifInfo.gps_longitude[2].den = 1;

        mExifInfo.gps_altitude.num = (uint32_t)altitude;
        mExifInfo.gps_altitude.den = 1;

        struct tm tm_data;
        gmtime_r(&m_gps_timestamp, &tm_data);
        mExifInfo.gps_timestamp[0].num = tm_data.tm_hour;
        mExifInfo.gps_timestamp[0].den = 1;
        mExifInfo.gps_timestamp[1].num = tm_data.tm_min;
        mExifInfo.gps_timestamp[1].den = 1;
        mExifInfo.gps_timestamp[2].num = tm_data.tm_sec;
        mExifInfo.gps_timestamp[2].den = 1;
        snprintf((char*)mExifInfo.gps_datestamp, sizeof(mExifInfo.gps_datestamp),
                "%04d:%02d:%02d", tm_data.tm_year + 1900, tm_data.tm_mon + 1, tm_data.tm_mday);

        mExifInfo.enableGps = true;
    } else {
        mExifInfo.enableGps = false;
    }

    //2 1th IFD TIFF Tags
    mExifInfo.widthThumb = m_jpeg_thumbnail_width;
    mExifInfo.heightThumb = m_jpeg_thumbnail_height;
}

int SecCamera::makeExif (unsigned char *exifOut,
                                        unsigned char *thumb_buf,
                                        unsigned int thumb_size,
                                        exif_attribute_t *exifInfo,
                                        unsigned int *size,
                                        bool useMainbufForThumb)
{
    unsigned char *pCur, *pApp1Start, *pIfdStart, *pGpsIfdPtr, *pNextIfdOffset;
    unsigned int tmp, LongerTagOffest = 0;
    pApp1Start = pCur = exifOut;

    //2 Exif Identifier Code & TIFF Header
    pCur += 4;  // Skip 4 Byte for APP1 marker and length
    unsigned char ExifIdentifierCode[6] = { 0x45, 0x78, 0x69, 0x66, 0x00, 0x00 };
    memcpy(pCur, ExifIdentifierCode, 6);
    pCur += 6;

    /* Byte Order - little endian, Offset of IFD - 0x00000008.H */
    unsigned char TiffHeader[8] = { 0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00 };
    memcpy(pCur, TiffHeader, 8);
    pIfdStart = pCur;
    pCur += 8;

    //2 0th IFD TIFF Tags
    if (exifInfo->enableGps)
        tmp = NUM_0TH_IFD_TIFF;
    else
        tmp = NUM_0TH_IFD_TIFF - 1;

    memcpy(pCur, &tmp, NUM_SIZE);
    pCur += NUM_SIZE;

    LongerTagOffest += 8 + NUM_SIZE + tmp*IFD_SIZE + OFFSET_SIZE;

    writeExifIfd(&pCur, EXIF_TAG_IMAGE_WIDTH, EXIF_TYPE_LONG,
                 1, exifInfo->width);
    writeExifIfd(&pCur, EXIF_TAG_IMAGE_HEIGHT, EXIF_TYPE_LONG,
                 1, exifInfo->height);
    writeExifIfd(&pCur, EXIF_TAG_MAKE, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->maker) + 1, exifInfo->maker, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_MODEL, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->model) + 1, exifInfo->model, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_ORIENTATION, EXIF_TYPE_SHORT,
                 1, exifInfo->orientation);
    writeExifIfd(&pCur, EXIF_TAG_SOFTWARE, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->software) + 1, exifInfo->software, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_YCBCR_POSITIONING, EXIF_TYPE_SHORT,
                 1, exifInfo->ycbcr_positioning);
    writeExifIfd(&pCur, EXIF_TAG_EXIF_IFD_POINTER, EXIF_TYPE_LONG,
                 1, LongerTagOffest);
    if (exifInfo->enableGps) {
        pGpsIfdPtr = pCur;
        pCur += IFD_SIZE;   // Skip a ifd size for gps IFD pointer
    }

    pNextIfdOffset = pCur;  // Skip a offset size for next IFD offset
    pCur += OFFSET_SIZE;

    //2 0th IFD Exif Private Tags
    pCur = pIfdStart + LongerTagOffest;

    tmp = NUM_0TH_IFD_EXIF;
    memcpy(pCur, &tmp , NUM_SIZE);
    pCur += NUM_SIZE;

    LongerTagOffest += NUM_SIZE + NUM_0TH_IFD_EXIF*IFD_SIZE + OFFSET_SIZE;

    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_TIME, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->exposure_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_FNUMBER, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->fnumber, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_PROGRAM, EXIF_TYPE_SHORT,
                 1, exifInfo->exposure_program);
    writeExifIfd(&pCur, EXIF_TAG_ISO_SPEED_RATING, EXIF_TYPE_SHORT,
                 1, exifInfo->iso_speed_rating);
    writeExifIfd(&pCur, EXIF_TAG_EXIF_VERSION, EXIF_TYPE_UNDEFINED,
                 4, exifInfo->exif_version);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME_ORG, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME_DIGITIZE, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_SHUTTER_SPEED, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->shutter_speed, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_APERTURE, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->aperture, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_BRIGHTNESS, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->brightness, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_BIAS, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->exposure_bias, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_MAX_APERTURE, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->max_aperture, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_METERING_MODE, EXIF_TYPE_SHORT,
                 1, exifInfo->metering_mode);
    writeExifIfd(&pCur, EXIF_TAG_FLASH, EXIF_TYPE_SHORT,
                 1, exifInfo->flash);
    writeExifIfd(&pCur, EXIF_TAG_FOCAL_LENGTH, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->focal_length, &LongerTagOffest, pIfdStart);
    char code[8] = { 0x00, 0x00, 0x00, 0x49, 0x49, 0x43, 0x53, 0x41 };
    int commentsLen = strlen((char *)exifInfo->user_comment) + 1;
    memmove(exifInfo->user_comment + sizeof(code), exifInfo->user_comment, commentsLen);
    memcpy(exifInfo->user_comment, code, sizeof(code));
    writeExifIfd(&pCur, EXIF_TAG_USER_COMMENT, EXIF_TYPE_UNDEFINED,
                 commentsLen + sizeof(code), exifInfo->user_comment, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_COLOR_SPACE, EXIF_TYPE_SHORT,
                 1, exifInfo->color_space);
    writeExifIfd(&pCur, EXIF_TAG_PIXEL_X_DIMENSION, EXIF_TYPE_LONG,
                 1, exifInfo->width);
    writeExifIfd(&pCur, EXIF_TAG_PIXEL_Y_DIMENSION, EXIF_TYPE_LONG,
                 1, exifInfo->height);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_MODE, EXIF_TYPE_LONG,
                 1, exifInfo->exposure_mode);
    writeExifIfd(&pCur, EXIF_TAG_WHITE_BALANCE, EXIF_TYPE_LONG,
                 1, exifInfo->white_balance);
    writeExifIfd(&pCur, EXIF_TAG_SCENCE_CAPTURE_TYPE, EXIF_TYPE_LONG,
                 1, exifInfo->scene_capture_type);
    tmp = 0;
    memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
    pCur += OFFSET_SIZE;

    //2 0th IFD GPS Info Tags
    if (exifInfo->enableGps) {
        writeExifIfd(&pGpsIfdPtr, EXIF_TAG_GPS_IFD_POINTER, EXIF_TYPE_LONG,
                     1, LongerTagOffest); // GPS IFD pointer skipped on 0th IFD

        pCur = pIfdStart + LongerTagOffest;

        if (exifInfo->gps_processing_method[0] == 0) {
            // don't create GPS_PROCESSING_METHOD tag if there isn't any
            tmp = NUM_0TH_IFD_GPS - 1;
        } else {
            tmp = NUM_0TH_IFD_GPS;
        }
        memcpy(pCur, &tmp, NUM_SIZE);
        pCur += NUM_SIZE;

        LongerTagOffest += NUM_SIZE + tmp*IFD_SIZE + OFFSET_SIZE;

        writeExifIfd(&pCur, EXIF_TAG_GPS_VERSION_ID, EXIF_TYPE_BYTE,
                     4, exifInfo->gps_version_id);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LATITUDE_REF, EXIF_TYPE_ASCII,
                     2, exifInfo->gps_latitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LATITUDE, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_latitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LONGITUDE_REF, EXIF_TYPE_ASCII,
                     2, exifInfo->gps_longitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LONGITUDE, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_longitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_ALTITUDE_REF, EXIF_TYPE_BYTE,
                     1, exifInfo->gps_altitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_ALTITUDE, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->gps_altitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_TIMESTAMP, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_timestamp, &LongerTagOffest, pIfdStart);
        tmp = strlen((char*)exifInfo->gps_processing_method);
        if (tmp > 0) {
            if (tmp > 100) {
                tmp = 100;
            }
            static const char ExifAsciiPrefix[] = { 0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0 };
            unsigned char tmp_buf[100+sizeof(ExifAsciiPrefix)];
            memcpy(tmp_buf, ExifAsciiPrefix, sizeof(ExifAsciiPrefix));
            memcpy(&tmp_buf[sizeof(ExifAsciiPrefix)], exifInfo->gps_processing_method, tmp);
            writeExifIfd(&pCur, EXIF_TAG_GPS_PROCESSING_METHOD, EXIF_TYPE_UNDEFINED,
                         tmp+sizeof(ExifAsciiPrefix), tmp_buf, &LongerTagOffest, pIfdStart);
        }
        writeExifIfd(&pCur, EXIF_TAG_GPS_DATESTAMP, EXIF_TYPE_ASCII,
                     11, exifInfo->gps_datestamp, &LongerTagOffest, pIfdStart);
        tmp = 0;
        memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
        pCur += OFFSET_SIZE;
    }

    //2 1th IFD TIFF Tags

    unsigned char *thumbBuf = thumb_buf;
    unsigned int thumbSize = thumb_size;

    if (thumbSize > m_jpeg_thumbnail_width * m_jpeg_thumbnail_height) {
        ALOGE("ERR(%s): Thumbnail image size is too big. thumb size is %d (max = %d)", __func__, thumbSize, m_jpeg_thumbnail_width * m_jpeg_thumbnail_height);
        thumbBuf = NULL;
    }

    if (exifInfo->enableThumb && (thumbBuf != NULL) && (thumbSize > 0)) {
        tmp = LongerTagOffest;
        memcpy(pNextIfdOffset, &tmp, OFFSET_SIZE);  // NEXT IFD offset skipped on 0th IFD

        pCur = pIfdStart + LongerTagOffest;

        tmp = NUM_1TH_IFD_TIFF;
        memcpy(pCur, &tmp, NUM_SIZE);
        pCur += NUM_SIZE;

        LongerTagOffest += NUM_SIZE + NUM_1TH_IFD_TIFF*IFD_SIZE + OFFSET_SIZE;

        writeExifIfd(&pCur, EXIF_TAG_IMAGE_WIDTH, EXIF_TYPE_LONG,
                     1, exifInfo->widthThumb);
        writeExifIfd(&pCur, EXIF_TAG_IMAGE_HEIGHT, EXIF_TYPE_LONG,
                     1, exifInfo->heightThumb);
        writeExifIfd(&pCur, EXIF_TAG_COMPRESSION_SCHEME, EXIF_TYPE_SHORT,
                     1, exifInfo->compression_scheme);
        writeExifIfd(&pCur, EXIF_TAG_ORIENTATION, EXIF_TYPE_SHORT,
                     1, exifInfo->orientation);
        writeExifIfd(&pCur, EXIF_TAG_X_RESOLUTION, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->x_resolution, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_Y_RESOLUTION, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->y_resolution, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_RESOLUTION_UNIT, EXIF_TYPE_SHORT,
                     1, exifInfo->resolution_unit);
        writeExifIfd(&pCur, EXIF_TAG_JPEG_INTERCHANGE_FORMAT, EXIF_TYPE_LONG,
                     1, LongerTagOffest);
        writeExifIfd(&pCur, EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LEN, EXIF_TYPE_LONG,
                     1, thumbSize);

        tmp = 0;
        memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
        pCur += OFFSET_SIZE;

        memcpy(pIfdStart + LongerTagOffest, thumbBuf, thumbSize);
        LongerTagOffest += thumbSize;
    } else {
        tmp = 0;
        memcpy(pNextIfdOffset, &tmp, OFFSET_SIZE);  // NEXT IFD offset skipped on 0th IFD
    }

    unsigned char App1Marker[2] = { 0xff, 0xe1 };
    memcpy(pApp1Start, App1Marker, 2);
    pApp1Start += 2;

    *size = 10 + LongerTagOffest;
    tmp = *size - 2;    // APP1 Maker isn't counted
    unsigned char size_mm[2] = {(tmp >> 8) & 0xFF, tmp & 0xFF};
    memcpy(pApp1Start, size_mm, 2);

    ALOGD("makeExif X");

    return 0;
}

inline void SecCamera::writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         uint32_t value)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, &value, 4);
    *pCur += 4;
}

inline void SecCamera::writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         unsigned char *pValue)
{
    char buf[4] = { 0,};

    memcpy(buf, pValue, count);
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, buf, 4);
    *pCur += 4;
}

inline void SecCamera::writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         unsigned char *pValue,
                                         unsigned int *offset,
                                         unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, count);
    *offset += count;
}

inline void SecCamera::writeExifIfd(unsigned char **pCur,
                                         unsigned short tag,
                                         unsigned short type,
                                         unsigned int count,
                                         rational_t *pValue,
                                         unsigned int *offset,
                                         unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, 8 * count);
    *offset += 8 * count;
}

status_t SecCamera::dump(int fd)
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    snprintf(buffer, 255, "dump(%d)\n", fd);
    result.append(buffer);
    ::write(fd, result.string(), result.size());
    return NO_ERROR;
}

double SecCamera::jpeg_ratio = 0.7;
int SecCamera::interleaveDataSize = 5242880;
int SecCamera::jpegLineLength = 636;

}; // namespace android
