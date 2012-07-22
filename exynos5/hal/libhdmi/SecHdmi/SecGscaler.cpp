/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright@ Samsung Electronics Co. LTD
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

//#define DEBUG_LIB_FIMC
#define LOG_TAG "libgscaler"
//#define USE_GSC_USERPTR
#include <cutils/log.h>
#include "../libhdmi/SecHdmi/SecGscaler.h"

#ifdef USE_GSC_USERPTR
#define V4L2_MEMORY_TYPE V4L2_MEMORY_USERPTR
#else
#define V4L2_MEMORY_TYPE V4L2_MEMORY_MMAP
#endif

struct yuv_fmt_list yuv_list[] = {
    { "V4L2_PIX_FMT_NV12",      "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12,      12, 1 },
    { "V4L2_PIX_FMT_NV12M",     "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12M,     12, 2 },
    { "V4L2_PIX_FMT_NV12MT",    "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12MT,    12, 2 },
    { "V4L2_PIX_FMT_NV21",      "YUV420/2P/LSB_CRCB",   V4L2_PIX_FMT_NV21,      12, 1 },
    { "V4L2_PIX_FMT_NV21X",     "YUV420/2P/MSB_CBCR",   V4L2_PIX_FMT_NV21X,     12, 2 },
    { "V4L2_PIX_FMT_NV12X",     "YUV420/2P/MSB_CRCB",   V4L2_PIX_FMT_NV12X,     12, 2 },
    { "V4L2_PIX_FMT_YUV420",    "YUV420/3P",            V4L2_PIX_FMT_YUV420,    12, 3 },
    { "V4L2_PIX_FMT_YUV420M",   "YUV420/3P",            V4L2_PIX_FMT_YUV420M,   12, 3 },
    { "V4L2_PIX_FMT_YUYV",      "YUV422/1P/YCBYCR",     V4L2_PIX_FMT_YUYV,      16, 1 },
    { "V4L2_PIX_FMT_YVYU",      "YUV422/1P/YCRYCB",     V4L2_PIX_FMT_YVYU,      16, 1 },
    { "V4L2_PIX_FMT_UYVY",      "YUV422/1P/CBYCRY",     V4L2_PIX_FMT_UYVY,      16, 1 },
    { "V4L2_PIX_FMT_VYUY",      "YUV422/1P/CRYCBY",     V4L2_PIX_FMT_VYUY,      16, 1 },
    { "V4L2_PIX_FMT_UV12",      "YUV422/2P/LSB_CBCR",   V4L2_PIX_FMT_NV16,      16, 2 },
    { "V4L2_PIX_FMT_UV21",      "YUV422/2P/LSB_CRCB",   V4L2_PIX_FMT_NV61,      16, 2 },
    { "V4L2_PIX_FMT_UV12X",     "YUV422/2P/MSB_CBCR",   V4L2_PIX_FMT_NV16X,     16, 2 },
    { "V4L2_PIX_FMT_UV21X",     "YUV422/2P/MSB_CRCB",   V4L2_PIX_FMT_NV61X,     16, 2 },
    { "V4L2_PIX_FMT_YUV422P",   "YUV422/3P",            V4L2_PIX_FMT_YUV422P,   16, 3 },
};

void dump_pixfmt_mp(struct v4l2_pix_format_mplane *pix_mp)
{
    ALOGI("w: %d", pix_mp->width);
    ALOGI("h: %d", pix_mp->height);
    ALOGI("color: %x", pix_mp->colorspace);

    switch (pix_mp->pixelformat) {
    case V4L2_PIX_FMT_YUYV:
        ALOGI ("YUYV");
        break;
    case V4L2_PIX_FMT_UYVY:
        ALOGI ("UYVY");
        break;
    case V4L2_PIX_FMT_RGB565:
        ALOGI ("RGB565");
        break;
    case V4L2_PIX_FMT_RGB565X:
        ALOGI ("RGB565X");
        break;
    default:
        ALOGI("not supported");
    }
}

void dump_crop(struct v4l2_rect *rect)
{
    ALOGI("crop l: %d", rect->left);
    ALOGI("crop t: %d", rect->top);
    ALOGI("crop w: %d", rect->width);
    ALOGI("crop h: %d", rect->height);
}

void gsc_v4l2_dump_state(int fd)
{
    struct v4l2_format format;
    struct v4l2_crop   crop;
    struct v4l2_subdev_crop sCrop;

    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) < 0)
        return;

    ALOGI("dumping driver state:");
    dump_pixfmt_mp(&format.fmt.pix_mp);

    crop.type = format.type;
    if (ioctl(fd, VIDIOC_G_CROP, &crop) < 0)
        return;

    ALOGI("input image crop:");
    dump_crop(&(crop.c));

    sCrop.pad   = GSC_SUBDEV_PAD_SOURCE;
    sCrop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (ioctl(fd, VIDIOC_SUBDEV_G_CROP, &sCrop) < 0)
        return;

    ALOGI("output image crop:");
    dump_crop(&(sCrop.rect));

}

int gsc_v4l2_querycap(int fd, char *node)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_capability v4l2cap;

    if (ioctl(fd, VIDIOC_QUERYCAP, &v4l2cap) < 0) {
        ALOGE("%s::VIDIOC_QUERYCAP failed", __func__);
        return -1;
    }

    if (!(v4l2cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s::%s is not support streaming", __func__, node);
        return -1;
    }

    if (!(v4l2cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)) {
        ALOGE("%s::%s is not support video output mplane", __func__, node);
        return -1;
    }

    return 0;
}

int gsc_v4l2_query_buf(int fd, SecBuffer *secBuf, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int buf_index, int num_plane)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_GSCALER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_GSCALER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    if (MAX_BUFFERS_GSCALER <= buf_index || MAX_PLANES_GSCALER < num_plane) {
        ALOGE("%s::exceed MAX! : buf_index=%d, num_plane=%d", __func__, buf_index, num_plane);
        return -1;
    }

    buf.type     = type;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.index    = buf_index;
    buf.length   = num_plane;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_QUERYBUF failed, plane_cnt=%d", __func__, buf.length);
        return -1;
    }

    for (int i = 0; i < num_plane; i++) {
        if ((secBuf->virt.extP[i] = (char *)mmap(0, buf.m.planes[i].length,
                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[i].m.mem_offset)) < 0) {
            ALOGE("%s::mmap failed", __func__);
            ALOGE("%s::Offset = 0x%x", __func__, buf.m.planes[i].m.mem_offset);
            ALOGE("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
            ALOGE("%s::vaddr[%d][%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
            return -1;
        }
        secBuf->size.extS[i] = buf.m.planes[i].length;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::vaddr[bufindex=%d][planeindex=%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
    ALOGD("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
#endif
    }

    return 0;
}

int gsc_v4l2_req_buf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int num_bufs)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_requestbuffers reqbuf;

    reqbuf.type   = type;
    reqbuf.memory = memory;
    reqbuf.count  = num_bufs;

#ifdef DEBUG_LIB_FIMC
    ALOGI("%d buffers needed", reqbuf.count);
#endif

    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        ALOGE("%s::VIDIOC_REQBUFS failed, reqbuf.count=%d", __func__, reqbuf.count);
        return -1;
    }

#ifdef DEBUG_LIB_FIMC
    ALOGI("%d buffers allocated", reqbuf.count);
#endif

    if (reqbuf.count < num_bufs) {
        ALOGE("%s::VIDIOC_REQBUFS failed ((reqbuf.count(%d) < num_bufs(%d))",
            __func__, reqbuf.count, num_bufs);
        return -1;
    }

    return 0;
}

int gsc_v4l2_s_ctrl(int fd, int id, int value)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_control vc;

    vc.id    = id;
    vc.value = value;

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("%s::VIDIOC_S_CTRL (id=%d,value=%d) failed", __func__, id, value);
        return -1;
    }

    return 0;
}

int gsc_v4l2_set_fmt(int fd, enum v4l2_buf_type type, enum v4l2_field field, s5p_fimc_img_info *img_info)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_format  fmt;
    struct v4l2_crop    crop;

    fmt.type = type;
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::fmt.type=%d", __func__, fmt.type);
#endif
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_G_FMT failed", __func__);
        return -1;
    }

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        fmt.fmt.pix.width       = img_info->full_width;
        fmt.fmt.pix.height      = img_info->full_height;
        fmt.fmt.pix.pixelformat = img_info->color_space;
        fmt.fmt.pix.field       = field;
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        fmt.fmt.pix_mp.width       = img_info->full_width;
        fmt.fmt.pix_mp.height      = img_info->full_height;
        fmt.fmt.pix_mp.pixelformat = img_info->color_space;
        fmt.fmt.pix_mp.field       = field;
        fmt.fmt.pix_mp.num_planes  = img_info->planes;
        break;
    default:
        ALOGE("%s::invalid buffer type", __func__);
        return -1;
        break;
    }

#ifdef DEBUG_LIB_FIMC
for (int i = 0; i < 3; i++) {
    ALOGD("%s::fmt.fmt.pix_mp. w=%d, h=%d, pixelformat=0x%08x, filed=%d, num_planes=%d",
               __func__,
               fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
               fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.field,
               fmt.fmt.pix_mp.num_planes);
}
#endif

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_S_FMT failed", __func__);
        return -1;
    }

#ifdef DEBUG_LIB_FIMC
for (int i = 0; i < 3; i++) {
    ALOGD("%s::pix_mp.pix_mp.plane_fmt[%d].sizeimage   =0x%08x", __func__, i, fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
    ALOGD("%s::pix_mp.pix_mp.plane_fmt[%d].bytesperline=0x%08x", __func__, i, fmt.fmt.pix_mp.plane_fmt[i].bytesperline);
}
#endif

    crop.type     = type;
    crop.c.left   = img_info->start_x;
    crop.c.top    = img_info->start_y;
    crop.c.width  = img_info->width;
    crop.c.height = img_info->height;

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_S_CROP (x=%d, y=%d, w=%d, h=%d) failed",
            __func__,
            img_info->start_x,
            img_info->start_y,
            img_info->width,
            img_info->height);
        return -1;
    }

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::pix_mp pixelformat=0x%08x", __func__, fmt.fmt.pix_mp.pixelformat);
    ALOGD("%s::pix_mp w=%d, h=%d, planes=%d", __func__, fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, fmt.fmt.pix_mp.num_planes);
    ALOGD("%s::crop   x=%d, y=%d, w=%d, h=%d", __func__, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
#endif

    return 0;
}

int gsc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMON failed", __func__);
        return -1;
    }

    return 0;
}

int gsc_v4l2_queue(int fd, SecBuffer *secBuf, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int index, int num_plane)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
    ALOGD("%s::num_plane=%d", __func__, num_plane);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_GSCALER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_GSCALER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_plane;
    buf.index    = index;
    buf.m.planes = planes;

    for (unsigned int i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)secBuf->virt.extP[i];
        buf.m.planes[i].length    = secBuf->size.extS[i];
        buf.m.planes[i].bytesused = buf.m.planes[i].length;
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::buf.index=%d", __func__, buf.index);
    ALOGD("%s::buf.m.planes[%d].m.userptr=0x%08x", __func__, i, (unsigned int)buf.m.planes[i].m.userptr);
    ALOGD("%s::buf.m.planes[%d].length   =0x%08x", __func__, i, buf.m.planes[i].length);
    ALOGD("%s::buf.m.planes[%d].bytesused=0x%08x", __func__, i, buf.m.planes[i].bytesused);
#endif
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_QBUF failed", __func__);
        return -1;
    }

    return 0;
}

int gsc_v4l2_dequeue(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int *index, int num_plane)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_GSCALER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_GSCALER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));


    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_plane;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_DQBUF failed", __func__);
        return -1;
    }

    *index = buf.index;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::buf.index=%d", __func__, buf.index);
#endif

    return 0;
}

int gsc_v4l2_stream_off(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMOFF failed", __func__);
        return -1;
    }

    return 0;
}

int gsc_v4l2_clr_buf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.type    = type;
    req.memory  = memory;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("%s::VIDIOC_REQBUFS failed", __func__);
        return -1;
    }

    return 0;
}

int gsc_subdev_set_fmt(int fd, unsigned int pad, enum v4l2_mbus_pixelcode code, s5p_fimc_img_info *img_info)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    struct v4l2_subdev_format fmt;
    struct v4l2_subdev_crop   crop;

    fmt.pad   = pad;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.width  = img_info->full_width;
    fmt.format.height = img_info->full_height;
    fmt.format.code   = code;

    if (ioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_FMT failed", __func__);
        return -1;
    }

    crop.pad   = pad;
    crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    crop.rect.left   = img_info->start_x;
    crop.rect.top    = img_info->start_y;
    crop.rect.width  = img_info->width;
    crop.rect.height = img_info->height;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::pix_mp w=%d, h=%d", __func__, fmt.format.width, fmt.format.height);
    ALOGD("%s::crop   x=%d, y=%d, w=%d, h=%d", __func__, crop.rect.left, crop.rect.top, crop.rect.width, crop.rect.height);
#endif

    if (ioctl(fd, VIDIOC_SUBDEV_S_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_CROP failed", __func__);
        return -1;
    }

    return 0;
}

static inline int multipleOfN(int number, int N)
{
    int result = number;
    switch (N) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
    case 256:
        result = (number - (number & (N-1)));
        break;
    default:
        result = number - (number % N);
        break;
    }
    return result;
}

SecGscaler::SecGscaler()
:   mFlagCreate(false)
{
}

SecGscaler::~SecGscaler()
{
    if (mFlagCreate == true) {
        ALOGE("%s::this is not Destroyed fail", __func__);

        if (destroy() == false)
            ALOGE("%s::destroy failed", __func__);
    }
}

bool SecGscaler::create(enum DEV dev, enum MODE mode, unsigned int numOfBuf)
{
    create(dev, numOfBuf);
    return true;
}

bool SecGscaler::create(enum DEV dev, unsigned int numOfBuf)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if (mFlagCreate == true) {
        ALOGE("%s::Already Created", __func__);
        return true;
    }

    char node[20];
    char subdevname[32];
    char videodevname[32];

    struct v4l2_capability   v4l2cap;
    struct media_entity_desc entity_desc;

    mDev = dev;
    mVideoNodeNum = dev;
    mNumOfBuf = numOfBuf;

    mVideodevFd = 0;
    mMediadevFd = 0;
    mSubdevFd = 0;

    mSrcIndex = 0;
    mRotVal = 0;
    mFlagGlobalAlpha = false;
    mGlobalAlpha = 0x0;
    mFlagLocalAlpha = false;
    mFlagColorKey = false;
    mColorKey = 0x0;
    mFlagSetSrcParam = false;
    mFlagSetDstParam = false;
    mFlagStreamOn = false;

    memset(&mS5pFimc, 0, sizeof(s5p_fimc_t));

    switch(mDev) {
    case DEV_0:
        mVideoNodeNum = 24;
        mSubdevNodeNum = 4;
        break;
    case DEV_1:
        mVideoNodeNum = 27;
        mSubdevNodeNum = 5;
        break;
    case DEV_2:
        mVideoNodeNum = 30;
        mSubdevNodeNum = 6;
        break;
    case DEV_3:
        mVideoNodeNum = 33;
        mSubdevNodeNum = 3; // need to modify //carrotsm
        break;
    default:
        ALOGE("%s::invalid mDev(%d)", __func__, mDev);
        goto err;
        break;
    }

    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    mMediadevFd = open(node, O_RDONLY);

    if (mMediadevFd < 0) {
        ALOGE("%s::open(%s) failed : O_RDONLY", __func__, node);
        goto err;
    }

    sprintf(subdevname,   PFX_ENTITY_SUBDEV_GSC, mDev);
    sprintf(videodevname, PFX_ENTITY_OUTPUTDEV_GSC, mDev);

    for (__u32 id = 0; ; id = entity_desc.id) {
        entity_desc.id = id | MEDIA_ENT_ID_FLAG_NEXT;

        if (ioctl(mMediadevFd, MEDIA_IOC_ENUM_ENTITIES, &entity_desc) < 0) {
            if (errno == EINVAL) {
                ALOGD("%s::MEDIA_IOC_ENUM_ENTITIES ended", __func__);
                break;
            }
            ALOGE("%s::MEDIA_IOC_ENUM_ENTITIES failed", __func__);
            goto err;
        }

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s::entity_desc.id=%d, .minor=%d .name=%s", __func__, entity_desc.id, entity_desc.v4l.minor, entity_desc.name);
#endif

        if (strncmp(entity_desc.name, subdevname, strlen(subdevname)) == 0)
            mSubdevEntity = entity_desc.id;

        if (strncmp(entity_desc.name, videodevname, strlen(videodevname)) == 0)
            mVideodevEntity = entity_desc.id;
    }

    if (0 < mMediadevFd)
        close(mMediadevFd);
    mMediadevFd = -1;

    sprintf(node, "%s%d", PFX_NODE_SUBDEV, mSubdevNodeNum);
    mSubdevFd = open(node, O_RDWR);
    if (mSubdevFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto err;
    }

    sprintf(node, "%s%d", PFX_NODE_VIDEODEV, mVideoNodeNum);
    mVideodevFd = open(node, O_RDWR);
    if (mVideodevFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        goto err;
    }

    /* check capability */
    if (gsc_v4l2_querycap(mVideodevFd, node) < 0 ) {
        ALOGE("%s::tvout_std_v4l2_querycap failed", __func__);
        goto err;
    }

    mFlagCreate = true;

    return true;

err :
    if (0 < mVideodevFd)
        close(mVideodevFd);

    if (0 < mMediadevFd)
        close(mMediadevFd);

    if (0 < mSubdevFd)
        close(mSubdevFd);

    mVideodevFd = -1;
    mMediadevFd = -1;
    mSubdevFd = -1;

    return false;
}

bool SecGscaler::destroy()
{
    s5p_fimc_params_t *params = &(mS5pFimc.params);

    if (mFlagCreate == false) {
        ALOGE("%s::Already Destroyed", __func__);
        return true;
    }

    if (mFlagStreamOn == true) {
        if (streamOff() == false) {
            ALOGE("%s::streamOff() failed", __func__);
            return false;
        }

        if (closeVideodevFd() == false) {
            ALOGE("%s::closeVideodevFd() failed", __func__);
            return false;
        }

        mFlagStreamOn = false;
    }

    if (0 < mMediadevFd)
        close(mMediadevFd);

    if (0 < mSubdevFd)
        close(mSubdevFd);

    mVideodevFd = -1;
    mMediadevFd = -1;
    mSubdevFd = -1;

    mFlagCreate = false;

    return true;
}

bool SecGscaler::flagCreate(void)
{
    return mFlagCreate;
}

int SecGscaler::getFd(void)
{
    return getVideodevFd();
}

int SecGscaler::getVideodevFd(void)
{
    return mVideodevFd;
}

bool SecGscaler::openVideodevFd(void)
{
    char node[32];

    sprintf(node, "%s%d", PFX_NODE_VIDEODEV, mVideoNodeNum);
    mVideodevFd = open(node, O_RDWR);
    if (mVideodevFd < 0) {
        ALOGE("%s::open(%s) failed", __func__, node);
        return false;
    }

    return true;
}

bool SecGscaler::closeVideodevFd(void)
{
    if (mFlagSetSrcParam == true) {
        if (gsc_v4l2_clr_buf(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE) < 0) {
            ALOGE("%s::gsc_v4l2_clr_buf() failed", __func__);
            return false;
        }
    }

    if (0 < mVideodevFd) {
        if (close(mVideodevFd) < 0) {
            ALOGE("%s::close Videodev failed", __func__);
            return false;
        }
    }
    mVideodevFd = -1;

    return true;
}

int SecGscaler::getSubdevFd(void)
{
    return mSubdevFd;
}

__u32 SecGscaler::getSubdevEntity(void)
{
    return mSubdevEntity;
}

__u32 SecGscaler::getVideodevEntity(void)
{
    return mVideodevEntity;
}

bool  SecGscaler::getFlagSteamOn(void)
{
    return mFlagStreamOn;
}

SecBuffer * SecGscaler::getSrcBufferAddr(int index)
{
    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    return &mSrcBuffer[index];
}

bool SecGscaler::setSrcParams(unsigned int width, unsigned int height,
                              unsigned int cropX, unsigned int cropY,
                              unsigned int *cropWidth, unsigned int *cropHeight,
                              int colorFormat,
                              bool forceChange)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(colorFormat);
    if (v4l2ColorFormat < 0) {
        ALOGE("%s::not supported color format", __func__);
        return false;
    }

    s5p_fimc_params_t *params = &(mS5pFimc.params);

    unsigned int fimcWidth  = *cropWidth;
    unsigned int fimcHeight = *cropHeight;
    int src_planes = m_getYuvPlanes(v4l2ColorFormat);
    int src_bpp    = m_getYuvBpp(v4l2ColorFormat);
    unsigned int frame_size  = width * height;
    unsigned int frame_ratio = 8;

    if (m_checkSrcSize(width, height, cropX, cropY,
                   &fimcWidth, &fimcHeight, v4l2ColorFormat, false) == false) {
        ALOGE("%s::::size align error!", __func__);
        return false;
    }

    if (fimcWidth != *cropWidth || fimcHeight != *cropHeight) {
        if (forceChange == true) {
#ifdef DEBUG_LIB_FIMC
            ALOGD("size is changed from [w=%d, h=%d] to [w=%d, h=%d]",
                    *cropWidth, *cropHeight, fimcWidth, fimcHeight);
#endif
        } else {
            ALOGE("%s::invalid source params", __func__);
            return false;
        }
    }

    if (   (params->src.full_width == width)
        && (params->src.full_height == height)
        && (params->src.start_x == cropX)
        && (params->src.start_y == cropY)
        && (params->src.width == fimcWidth)
        && (params->src.height == fimcHeight)
        && (params->src.color_space == (unsigned int)v4l2ColorFormat))
        return true;

    params->src.full_width  = width;
    params->src.full_height = height;
    params->src.start_x     = cropX;
    params->src.start_y     = cropY;
    params->src.width       = fimcWidth;
    params->src.height      = fimcHeight;
    params->src.color_space = v4l2ColorFormat;
    src_planes = (src_planes == -1) ? 1 : src_planes;
    params->src.planes = src_planes;

    mSrcBuffer[mSrcIndex].size.extS[0] = 0;
    mSrcBuffer[mSrcIndex].size.extS[1] = 0;
    mSrcBuffer[mSrcIndex].size.extS[2] = 0;

    frame_ratio = frame_ratio * (src_planes -1) / (src_bpp - 8);
#ifdef USE_GSC_USERPTR
    for (int buf_index = 0; buf_index < MAX_BUFFERS_GSCALER; buf_index++) {
        switch (src_planes) {
        case 1:
            switch (v4l2ColorFormat) {
            case V4L2_PIX_FMT_BGR32:
                params->src.color_space = V4L2_PIX_FMT_RGB32;
            case V4L2_PIX_FMT_RGB32:
                mSrcBuffer[buf_index].size.extS[0] = frame_size << 2;
                break;
            case V4L2_PIX_FMT_RGB565X:
            case V4L2_PIX_FMT_NV16:
            case V4L2_PIX_FMT_NV61:
            case V4L2_PIX_FMT_YUYV:
            case V4L2_PIX_FMT_UYVY:
            case V4L2_PIX_FMT_VYUY:
            case V4L2_PIX_FMT_YVYU:
                mSrcBuffer[buf_index].size.extS[0] = frame_size << 1;
                break;
            case V4L2_PIX_FMT_YUV420:
            case V4L2_PIX_FMT_NV12:
            case V4L2_PIX_FMT_NV21:
                mSrcBuffer[buf_index].size.extS[0] = (frame_size * 3) >> 1;
                break;
            default:
                ALOGE("%s::invalid color type", __func__);
                return false;
                break;
            }
            mSrcBuffer[buf_index].size.extS[1] = 0;
            mSrcBuffer[buf_index].size.extS[2] = 0;
            break;
        case 2:
        case 3:
            mSrcBuffer[buf_index].size.extS[0] = frame_size;
            mSrcBuffer[buf_index].size.extS[1] = frame_size / frame_ratio;
            mSrcBuffer[buf_index].size.extS[2] = frame_size / frame_ratio;
            break;
        default:
            ALOGE("%s::invalid color foarmt", __func__);
            return false;
            break;
        }
    }
#else
    if (v4l2ColorFormat == V4L2_PIX_FMT_BGR32)
        params->src.color_space = V4L2_PIX_FMT_RGB32;
#endif

    if (mFlagSetSrcParam == true) {
        if (gsc_v4l2_clr_buf(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE) < 0) {
            ALOGE("%s::gsc_v4l2_clr_buf() failed", __func__);
            return false;
        }
    }

    if (gsc_v4l2_set_fmt(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_NONE, &(params->src)) < 0) {
        ALOGE("%s::fimc_v4l2_set_fmt()[src] failed", __func__);
        return false;
    }

    if (gsc_v4l2_req_buf(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE, mNumOfBuf) < 0) {
        ALOGE("%s::fimc_v4l2_req_buf()[src] failed", __func__);
        return false;
    }
#ifdef USE_GSC_USERPTR
#else
    for (unsigned int buf_index = 0; buf_index < MAX_BUFFERS_GSCALER; buf_index++) {
        if (gsc_v4l2_query_buf(mVideodevFd, &mSrcBuffer[buf_index], V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP, buf_index, params->src.planes) < 0) {
            ALOGE("%s::gsc_v4l2_query_buf() failed", __func__);
            return false;
        }
    }
#endif

    *cropWidth  = fimcWidth;
    *cropHeight = fimcHeight;

    mFlagSetSrcParam = true;

    return true;
}

bool SecGscaler::getSrcParams(unsigned int *width, unsigned int *height,
                              unsigned int *cropX, unsigned int *cropY,
                              unsigned int *cropWidth, unsigned int *cropHeight,
                              int *v4l2colorFormat)
{
    struct v4l2_format fmt;
    struct v4l2_crop   crop;

    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    if (ioctl(mVideodevFd, VIDIOC_G_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_G_FMT(fmt.type : %d) failed", __func__, fmt.type);
        return false;
    }

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
    case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        *width       = fmt.fmt.pix.width;
        *height      = fmt.fmt.pix.height;
        *v4l2colorFormat = fmt.fmt.pix.pixelformat;
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        *width       = fmt.fmt.pix_mp.width;
        *height      = fmt.fmt.pix_mp.height;
        *v4l2colorFormat = fmt.fmt.pix_mp.pixelformat;
        break;
    default:
        ALOGE("%s::Invalid buffer type", __func__);
        return false;
        break;
    }

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(mVideodevFd, VIDIOC_G_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_G_CROP failed", __func__);
        return false;
    }

    *cropX      = crop.c.left;
    *cropY      = crop.c.top;
    *cropWidth  = crop.c.width;
    *cropHeight = crop.c.height;

    return true;
}

bool SecGscaler::setSrcAddr(unsigned int YAddr,
                            unsigned int CbAddr,
                            unsigned int CrAddr,
                            int colorFormat)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagSetSrcParam == false) {
        ALOGE("%s::mFlagSetSrcParam == false", __func__);
        return false;
    }

#ifdef USE_GSC_USERPTR
    mSrcBuffer[mSrcIndex].virt.extP[0] = (char *)YAddr;
    mSrcBuffer[mSrcIndex].virt.extP[1] = (char *)CbAddr;
    mSrcBuffer[mSrcIndex].virt.extP[2] = (char *)CrAddr;
#else
    unsigned int srcAddr[MAX_PLANES_GSCALER];

    srcAddr[0] = YAddr;
    srcAddr[1] = CbAddr;
    srcAddr[2] = CrAddr;

    for (int plane_index = 0; plane_index < mS5pFimc.params.src.planes; plane_index++) {
        memcpy((void *)mSrcBuffer[mSrcIndex].virt.extP[plane_index], (void *)srcAddr[plane_index], mSrcBuffer[mSrcIndex].size.extS[plane_index]);
    }
#endif

    return true;
}

bool SecGscaler::setDstParams(unsigned int width, unsigned int height,
                              unsigned int cropX, unsigned int cropY,
                              unsigned int *cropWidth, unsigned int *cropHeight,
                              int colorFormat,
                              bool forceChange)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    int mbusFormat = V4L2_MBUS_FMT_YUV8_1X24;

    s5p_fimc_params_t *params = &(mS5pFimc.params);

    unsigned int fimcWidth  = *cropWidth;
    unsigned int fimcHeight = *cropHeight;

    if( m_checkDstSize(width, height, cropX, cropY,
                   &fimcWidth, &fimcHeight, V4L2_PIX_FMT_YUV444,
                   mRotVal, true) == false) {
        ALOGE("%s::size align error!", __func__);
        return false;
    }


    if (fimcWidth != *cropWidth || fimcHeight != *cropHeight) {
        if (forceChange == true) {
#ifdef DEBUG_LIB_FIMC
            ALOGD("size is changed from [w = %d, h= %d] to [w = %d, h = %d]",
                    *cropWidth, *cropHeight, fimcWidth, fimcHeight);
#endif
        } else {
            ALOGE("%s::Invalid destination params", __func__);
            return false;
        }
    }

    if (90 == mRotVal || 270 == mRotVal) {
        params->dst.full_width  = height;
        params->dst.full_height = width;

        if (90 == mRotVal) {
            params->dst.start_x     = cropY;
            params->dst.start_y     = width - (cropX + fimcWidth);
        } else {
            params->dst.start_x = height - (cropY + fimcHeight);
            params->dst.start_y = cropX;
        }

        params->dst.width       = fimcHeight;
        params->dst.height      = fimcWidth;

        params->dst.start_y     += (fimcWidth - params->dst.height);

    } else {
        params->dst.full_width  = width;
        params->dst.full_height = height;

        if (180 == mRotVal) {
            params->dst.start_x = width - (cropX + fimcWidth);
            params->dst.start_y = height - (cropY + fimcHeight);
        } else {
            params->dst.start_x     = cropX;
            params->dst.start_y     = cropY;
        }

        params->dst.width       = fimcWidth;
        params->dst.height      = fimcHeight;
    }

    if (gsc_subdev_set_fmt(mSubdevFd, GSC_SUBDEV_PAD_SOURCE, V4L2_MBUS_FMT_YUV8_1X24, &(params->dst)) < 0) {
        ALOGE("%s::gsc_subdev_set_fmt()[dst] failed", __func__);
        return false;
    }

    mFlagSetDstParam = true;

    return true;
}

bool SecGscaler::getDstParams(unsigned int *width, unsigned int *height,
                              unsigned int *cropX, unsigned int *cropY,
                              unsigned int *cropWidth, unsigned int *cropHeight,
                              int *mbusColorFormat)
{
    struct v4l2_subdev_format fmt;
    struct v4l2_subdev_crop   crop;

    fmt.pad   = GSC_SUBDEV_PAD_SOURCE;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

    if (ioctl(mSubdevFd, VIDIOC_SUBDEV_G_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_FMT", __func__);
        return -1;
    }

    *width  = fmt.format.width;
    *height = fmt.format.height;
    *mbusColorFormat = fmt.format.code;

    crop.pad   = GSC_SUBDEV_PAD_SOURCE;
    crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;

    if (ioctl(mSubdevFd, VIDIOC_SUBDEV_G_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_CROP", __func__);
        return -1;
    }

    *cropX      = crop.rect.left;
    *cropY      = crop.rect.top;
    *cropWidth  = crop.rect.width;
    *cropHeight = crop.rect.height;

    return true;
}

bool SecGscaler::setDstAddr(unsigned int YAddr, unsigned int CbAddr, unsigned int CrAddr, int buf_index)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    return true;
}

bool SecGscaler::setRotVal(unsigned int rotVal)
{
    struct v4l2_control vc;

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (gsc_v4l2_s_ctrl(mVideodevFd, V4L2_CID_ROTATE, rotVal) < 0) {
        ALOGE("%s::fimc_v4l2_s_ctrl(V4L2_CID_ROTATE) failed", __func__);
        return false;
    }

    mRotVal = rotVal;
    return true;
}

bool SecGscaler::setGlobalAlpha(bool enable, int alpha)
{
    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagStreamOn == true) {
        ALOGE("%s::mFlagStreamOn == true", __func__);
        return false;
    }

    mFlagGlobalAlpha = enable;
    mGlobalAlpha     = alpha;

    return true;

}

bool SecGscaler::setLocalAlpha(bool enable)
{
    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagStreamOn == true) {
        ALOGE("%s::mFlagStreamOn == true", __func__);
        return false;
    }

    if (mFlagLocalAlpha == enable)
        return true;

    return true;
}

bool SecGscaler::setColorKey(bool enable, int colorKey)
{
    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagStreamOn == true) {
        ALOGE("%s::mFlagStreamOn == true", __func__);
        return false;
    }

    mFlagColorKey = enable;
    mColorKey = colorKey;

    return true;
}

bool SecGscaler::run()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagSetSrcParam == false) {
        ALOGE("%s::faild : Need to set source parameters of Gscaler", __func__);
        return false;
    }

    if (mFlagSetDstParam == false) {
        ALOGE("%s::faild : Need to set destination parameters of Gscaler", __func__);
        return false;
    }

    s5p_fimc_params_t *params = &(mS5pFimc.params);
    int src_planes = m_getYuvPlanes(params->src.color_space);
    unsigned int dqIndex = 0;
    src_planes     = (src_planes == -1) ? 1 : src_planes;

    if (gsc_v4l2_queue(mVideodevFd, &(mSrcBuffer[mSrcIndex]), V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE, mSrcIndex, src_planes) < 0) {
        ALOGE("%s::fimc_v4l2_queue[src](mNumOfBuf : %d,mSrcIndex=%d) failed", __func__, mNumOfBuf, mSrcIndex);
        return false;
    }

    mSrcIndex++;
    if (mSrcIndex >= MAX_BUFFERS_GSCALER) {
        mSrcIndex = 0;
    }

    if (gsc_v4l2_dequeue(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE, &dqIndex, src_planes) < 0) {
        ALOGE("%s::fimc_v4l2_dequeue[src](mNumOfBuf : %d, dqIndex=%d) failed", __func__, mNumOfBuf, dqIndex);
        return false;
    }

    return true;
}

bool SecGscaler::streamOn(void)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagStreamOn == true) {
        ALOGE("%s::already streamon", __func__);
        return true;
    }

    s5p_fimc_params_t *params = &(mS5pFimc.params);
    int src_planes = m_getYuvPlanes(params->src.color_space);
    src_planes     = (src_planes == -1) ? 1 : src_planes;

    if (gsc_v4l2_queue(mVideodevFd, &(mSrcBuffer[mSrcIndex]), V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_TYPE, mSrcIndex, src_planes) < 0) {
        ALOGE("%s::fimc_v4l2_queue[src](mNumOfBuf : %d,mSrcIndex=%d) failed", __func__, mNumOfBuf, mSrcIndex);
        return false;
    }

    mSrcIndex++;
    if (mSrcIndex == MAX_BUFFERS_GSCALER - 1) {
        if (gsc_v4l2_stream_on(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
            ALOGE("%s::fimc_v4l2_stream_on() failed", __func__);
            return false;
        }
        mFlagStreamOn = true;
    }

    if (mSrcIndex >= MAX_BUFFERS_GSCALER) {
        mSrcIndex = 0;
    }

    return true;
}

bool SecGscaler::streamOff(void)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif

    if (mFlagCreate == false) {
        ALOGE("%s::Not yet created", __func__);
        return false;
    }

    if (mFlagStreamOn == false) {
        ALOGE("%s::already streamoff", __func__);
        return true;
    }

    if (gsc_v4l2_stream_off(mVideodevFd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
        ALOGE("%s::fimc_v4l2_stream_off() failed", __func__);
        return false;
    }

    mSrcIndex = 0;
#ifdef USE_GSC_USERPTR
    SecBuffer zeroBuf;
    for (int buf_index = 0; buf_index < MAX_BUFFERS_GSCALER; buf_index++)
        mSrcBuffer[buf_index] = zeroBuf;
#else
    for (int buf_index = 0; buf_index < MAX_BUFFERS_GSCALER; buf_index++) {
        for (int plane_index = 0; plane_index < MAX_PLANES_GSCALER; plane_index++) {
            munmap((void *)mSrcBuffer[buf_index].virt.extP[plane_index], mSrcBuffer[buf_index].size.extS[plane_index]);
        }
    }
#endif

    mFlagStreamOn = false;

    return true;
}

bool SecGscaler::m_checkSrcSize(unsigned int width, unsigned int height,
                             unsigned int cropX, unsigned int cropY,
                             unsigned int *cropWidth, unsigned int *cropHeight,
                             int colorFormat,
                             bool forceChange)
{
    bool ret = true;

    if (8 <= height && *cropHeight < 8) {
        if (forceChange)
            *cropHeight = 8;
        ret = false;
    }

    if (16 <= width && *cropWidth < 16) {
        if (forceChange)
            *cropWidth = 16;
        ret = false;
    }

    if (height < 8)
        return false;

    if (width % 16 != 0)
        return false;

    if (*cropWidth % 16 != 0) {
        if (forceChange)
            *cropWidth = multipleOfN(*cropWidth, 16);
        ret = false;
    }

    return ret;
}

bool SecGscaler::m_checkDstSize(unsigned int width, unsigned int height,
                             unsigned int cropX, unsigned int cropY,
                             unsigned int *cropWidth, unsigned int *cropHeight,
                             int colorFormat, int rotVal,  bool forceChange)
{
    bool ret = true;
    unsigned int rotWidth;
    unsigned int rotHeight;
    unsigned int *rotCropWidth;
    unsigned int *rotCropHeight;

    if (rotVal == 90 || rotVal == 270) {
        rotWidth = height;
        rotHeight = width;
        rotCropWidth = cropHeight;
        rotCropHeight = cropWidth;
    } else {
        rotWidth = width;
        rotHeight = height;
        rotCropWidth = cropWidth;
        rotCropHeight = cropHeight;
    }

    if (rotHeight < 8)
        return false;

    if (rotWidth % 8 != 0)
        return false;

    switch (colorFormat) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YUV420:
        if (*rotCropHeight % 2 != 0) {
            if (forceChange)
                *rotCropHeight = multipleOfN(*rotCropHeight, 2);
            ret = false;
        }
    }
    return ret;
}

int SecGscaler::m_widthOfFimc(int v4l2ColorFormat, int width)
{
    int newWidth = width;

    switch (v4l2ColorFormat) {
    case V4L2_PIX_FMT_RGB565:
        newWidth = multipleOfN(width, 8);
        break;
    case V4L2_PIX_FMT_RGB32:
        newWidth = multipleOfN(width, 4);
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
        newWidth = multipleOfN(width, 4);
        break;
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_NV16:
        newWidth = multipleOfN(width, 8);
        break;
    case V4L2_PIX_FMT_YUV422P:
        newWidth = multipleOfN(width, 16);
        break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12MT:
        newWidth = multipleOfN(width, 8);
        break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
        newWidth = multipleOfN(width, 16);
        break;
    default :
        break;
    }

    return newWidth;
}

int SecGscaler::m_heightOfFimc(int v4l2ColorFormat, int height)
{
    int newHeight = height;

    switch (v4l2ColorFormat) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
        newHeight = multipleOfN(height, 2);
        break;
    default :
        break;
    }
    return newHeight;
}

int SecGscaler::m_getYuvBpp(unsigned int fmt)
{
    int i, sel = -1;

    for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
        if (yuv_list[i].fmt == fmt) {
            sel = i;
            break;
        }
    }

    if (sel == -1)
        return sel;
    else
        return yuv_list[sel].bpp;
}

int SecGscaler::m_getYuvPlanes(unsigned int fmt)
{
    int i, sel = -1;

    for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
        if (yuv_list[i].fmt == fmt) {
            sel = i;
            break;
        }
    }

    if (sel == -1)
        return sel;
    else
        return yuv_list[sel].planes;
}
