/*
 * Copyright (C) 2010 The Android Open Source Project
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
 *
 * @author Rama, Meka(v.meka@samsung.com)
           Sangwoo, Park(sw5771.park@samsung.com)
           Jamie Oh (jung-min.oh@samsung.com)
 * @date   2011-03-11
 *
 */

#include "SecHWCUtils.h"

#define V4L2_BUF_TYPE_OUTPUT V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
#define V4L2_BUF_TYPE_CAPTURE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

//#define CHECK_FPS
#ifdef CHECK_FPS
#include <sys/time.h>
#include <unistd.h>
#define CHK_FRAME_CNT 30

void check_fps()
{
    static struct timeval tick, tick_old;
    static int total = 0;
    static int cnt = 0;
    int FPS;
    cnt++;
    gettimeofday(&tick, NULL);
    if (cnt > 10) {
        if (tick.tv_sec > tick_old.tv_sec)
            total += ((tick.tv_usec/1000) + (tick.tv_sec - tick_old.tv_sec)*1000 - (tick_old.tv_usec/1000));
        else
            total += ((tick.tv_usec - tick_old.tv_usec)/1000);

        memcpy(&tick_old, &tick, sizeof(timeval));
        if (cnt == (10 + CHK_FRAME_CNT)) {
            FPS = 1000*CHK_FRAME_CNT/total;
            ALOGE("[FPS]:%d\n", FPS);
            total = 0;
            cnt = 10;
        }
    } else {
        memcpy(&tick_old, &tick, sizeof(timeval));
        total = 0;
    }
}
#endif

struct yuv_fmt_list yuv_list[] = {
    { "V4L2_PIX_FMT_NV12",      "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12,     12, 2 },
    { "V4L2_PIX_FMT_NV12T",     "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12T,    12, 2 },
    { "V4L2_PIX_FMT_NV21",      "YUV420/2P/LSB_CRCB",   V4L2_PIX_FMT_NV21,     12, 2 },
    { "V4L2_PIX_FMT_NV21X",     "YUV420/2P/MSB_CBCR",   V4L2_PIX_FMT_NV21X,    12, 2 },
    { "V4L2_PIX_FMT_NV12X",     "YUV420/2P/MSB_CRCB",   V4L2_PIX_FMT_NV12X,    12, 2 },
    { "V4L2_PIX_FMT_YUV420",    "YUV420/3P",            V4L2_PIX_FMT_YUV420,   12, 3 },
    { "V4L2_PIX_FMT_YUV420M",   "YUV420/3P",            V4L2_PIX_FMT_YUV420M,  12, 3 },
    { "V4L2_PIX_FMT_YVU420M",   "YVU420/3P",            V4L2_PIX_FMT_YVU420M,  12, 3 },
    { "V4L2_PIX_FMT_NV12M",     "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12M,    12, 2 },
    { "V4L2_PIX_FMT_YUYV",      "YUV422/1P/YCBYCR",     V4L2_PIX_FMT_YUYV,     16, 1 },
    { "V4L2_PIX_FMT_YVYU",      "YUV422/1P/YCRYCB",     V4L2_PIX_FMT_YVYU,     16, 1 },
    { "V4L2_PIX_FMT_UYVY",      "YUV422/1P/CBYCRY",     V4L2_PIX_FMT_UYVY,     16, 1 },
    { "V4L2_PIX_FMT_VYUY",      "YUV422/1P/CRYCBY",     V4L2_PIX_FMT_VYUY,     16, 1 },
    { "V4L2_PIX_FMT_UV12",      "YUV422/2P/LSB_CBCR",   V4L2_PIX_FMT_NV16,     16, 2 },
    { "V4L2_PIX_FMT_UV21",      "YUV422/2P/LSB_CRCB",   V4L2_PIX_FMT_NV61,     16, 2 },
    { "V4L2_PIX_FMT_UV12X",     "YUV422/2P/MSB_CBCR",   V4L2_PIX_FMT_NV16X,    16, 2 },
    { "V4L2_PIX_FMT_UV21X",     "YUV422/2P/MSB_CRCB",   V4L2_PIX_FMT_NV61X,    16, 2 },
    { "V4L2_PIX_FMT_YUV422P",   "YUV422/3P",            V4L2_PIX_FMT_YUV422P,  16, 3 },
};

int window_open(struct hwc_win_info_t *win, int id)
{
    int fd = 0;
    char name[64];
    int vsync = 1;
    int real_id = id;

    char const * const device_template = "/dev/graphics/fb%u";
    // window & FB maping
    // fb0 -> win-id : 2
    // fb1 -> win-id : 1
    // fb2 -> win-id : 0
    // fb3 -> no device node
    // fb4 -> no device node
    // it is pre assumed that ...win0 or win1 is used here..

    switch (id) {
    case 0:
        real_id = 2;
        break;
    case 1:
        real_id = 1;
        break;
    default:
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::id(%d) is weird", __func__, id);
        goto error;
}

    snprintf(name, 64, device_template, real_id);

    win->fd = open(name, O_RDWR);
    if (win->fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Failed to open window device (%s) : %s",
                __func__, strerror(errno), name);
        goto error;
    }

#ifdef ENABLE_FIMD_VSYNC
    vsync = 1;
    if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
        goto error;
    }
#endif

    return 0;

error:
    if (0 < win->fd)
        close(win->fd);
    win->fd = 0;

    return -1;
}

int window_close(struct hwc_win_info_t *win)
{
    int ret = 0;

    if (0 < win->fd) {
        ion_unmap((void *)win->addr[0], ALIGN(win->size * NUM_OF_WIN_BUF, PAGE_SIZE));
        ion_free(win->ion_fd);

#ifdef ENABLE_FIMD_VSYNC
        /* Set using VSYNC Interrupt for FIMD_0   */
        int vsync = 0;
        if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0)
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
#endif
        ret = close(win->fd);
    }
    win->fd = 0;

    return ret;
}

int window_set_pos(struct hwc_win_info_t *win)
{
    struct s3cfb_user_window window;

    //before changing the screen configuration...powerdown the window
    if (window_hide(win) != 0)
        return -1;

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: x(%d), y(%d)",
            __func__, win->rect_info.x, win->rect_info.y);

    win->var_info.xres_virtual = (win->lcd_info.xres + 15) & ~ 15;
    win->var_info.yres_virtual = win->lcd_info.yres * NUM_OF_WIN_BUF;
    win->var_info.xres = win->rect_info.w;
    win->var_info.yres = win->rect_info.h;
    win->var_info.activate &= ~FB_ACTIVATE_MASK;
    win->var_info.activate |= FB_ACTIVATE_FORCE;
    if (ioctl(win->fd, FBIOPUT_VSCREENINFO, &(win->var_info)) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOPUT_VSCREENINFO(%d, %d) fail",
          __func__, win->rect_info.w, win->rect_info.h);
        return -1;
    }

    window.x = win->rect_info.x;
    window.y = win->rect_info.y;
    if (ioctl(win->fd, S3CFB_WIN_POSITION, &window) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_WIN_POSITION(%d, %d) fail",
            __func__, window.x, window.y);
      return -1;
    }

    return 0;
}

int window_get_info(struct hwc_win_info_t *win, int win_num)
{
    int temp_size = 0;

    if (ioctl(win->fd, FBIOGET_FSCREENINFO, &win->fix_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_FSCREENINFO failed : %s",
                strerror(errno));
        goto error;
    }

    win->size = win->fix_info.line_length * win->var_info.yres;

    struct s3c_fb_user_ion_client ion_handle;
    void *ion_start_addr;
    if (ioctl(win->fd, S3CFB_GET_ION_USER_HANDLE, &ion_handle) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Get fb ion client is failed\n");
        return -1;
    }

    win->ion_fd = ion_handle.fd;
    ion_start_addr = ion_map(win->ion_fd, ALIGN(win->size * NUM_OF_WIN_BUF, PAGE_SIZE), 0);

    for (int j = 0; j < NUM_OF_WIN_BUF; j++) {
        temp_size = win->size * j;
        win->addr[j] = (uint32_t)ion_start_addr + temp_size;
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::win-%d add[%d]  %x ",
                __func__, win_num, j,  win->addr[j]);
    }
    return 0;

error:
    win->fix_info.smem_start = 0;

    return -1;
}

int window_pan_display(struct hwc_win_info_t *win)
{
    struct fb_var_screeninfo *lcd_info = &(win->lcd_info);

#ifdef ENABLE_FIMD_VSYNC
    int pan_num = 0;
    if (ioctl(win->fd, FBIO_WAITFORVSYNC, &pan_num) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIO_WAITFORVSYNC fail(%s)",
                __func__, strerror(errno));
#endif

    lcd_info->yoffset = lcd_info->yres * win->buf_index;

    if (ioctl(win->fd, FBIOPAN_DISPLAY, lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOPAN_DISPLAY(%d / %d / %d) fail(%s)",
            __func__,
            lcd_info->yres,
            win->buf_index, lcd_info->yres_virtual,
            strerror(errno));
        return -1;
    }
    return 0;
}

int window_show(struct hwc_win_info_t *win)
{
    if (win->power_state == 0) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
                __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 1;
    }
    return 0;
}

int window_hide(struct hwc_win_info_t *win)
{
    if (win->power_state == 1) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
             __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 0;
    }
    return 0;
}

int window_get_global_lcd_info(int fd, struct fb_var_screeninfo *lcd_info)
{
    if (ioctl(fd, FBIOGET_VSCREENINFO, lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_VSCREENINFO failed : %s",
                strerror(errno));
        return -1;
    }

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: Default LCD x(%d),y(%d)",
            __func__, lcd_info->xres, lcd_info->yres);
    return 0;
}

int fimc_v4l2_set_src(int fd, s5p_fimc_img_info *src)
{
    struct v4l2_format  fmt;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;

    /* You MUST initialize structure for v4l2 */
    memset(&fmt, 0, sizeof(fmt));
    memset(&cropcap, 0, sizeof(cropcap));
    memset(&crop, 0, sizeof(crop));
    memset(&req, 0, sizeof(req));

    /**************  To set size & format for source image (DMA-INPUT) **************/
    fmt.fmt.pix_mp.num_planes  = src->planes;
    fmt.fmt.pix_mp.width       = src->full_width;
    fmt.fmt.pix_mp.height      = src->full_height;
    fmt.fmt.pix_mp.pixelformat = src->color_space;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;
    fmt.type                = V4L2_BUF_TYPE_OUTPUT;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_src-VIDIOC_S_FMT, type(%d), field(%d), plane(%d)"
        "pixelformat(0x%X), width(%d), height(%d)",
        fmt.type,  fmt.fmt.pix.field, fmt.fmt.pix_mp.num_planes,
        fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.width,  fmt.fmt.pix_mp.height);

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::VIDIOC_S_FMT failed : errno=%d (%s)"
                " : fd=%d\n", __func__, errno, strerror(errno), fd);
        return -1;
    }

    /************** crop input size **************/
    crop.type = V4L2_BUF_TYPE_OUTPUT;
    crop.c.width  = src->width;
    crop.c.height = src->height;
    crop.c.left   = src->start_x;
    crop.c.top    = src->start_y;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_src-VIDIOC_S_CROP, type(%d), XY(%d,%d), WH(%d,%d)",
        crop.type,  crop.c.left, crop.c.top, crop.c.width, crop.c.height);

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_CROP :"
                "crop.c.left : (%d), crop.c.top : (%d), crop.c.width : (%d), crop.c.height : (%d)",
                __func__, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        return -1;
    }

    /************** input buffer type **************/
    req.count       = 1;
    req.memory      = V4L2_MEMORY_USERPTR;
    req.type        = V4L2_BUF_TYPE_OUTPUT;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_src-VIDIOC_REQBUFS, count(%d), type(%d), memory(%d)",
        req.count,  req.type, req.memory);

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in VIDIOC_REQBUFS", __func__);
        return -1;
    }

    return 0;
}

int fimc_v4l2_set_dst(int fd, s5p_fimc_img_info *dst,
        int rotation, int hflip, int vflip, unsigned int addr)
{
    struct v4l2_format      sFormat;
    struct v4l2_control     vc;
    struct v4l2_framebuffer fbuf;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;
    int ret;

    /* You MUST initialize structure for v4l2 */
    memset(&sFormat, 0, sizeof(sFormat));
    memset(&vc, 0, sizeof(vc));
    memset(&fbuf, 0, sizeof(fbuf));
    memset(&crop, 0, sizeof(crop));
    memset(&req, 0, sizeof(req));

    /************** set rotation configuration **************/
    vc.id = V4L2_CID_ROTATE;
    vc.value = rotation;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-V4L2_CID_ROTATE, rot(%d)",vc.value);

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - rotation (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    /************** set hflip configuration **************/
    vc.id = V4L2_CID_HFLIP;
    vc.value = hflip;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-V4L2_CID_HFLIP, hflip(%d)",vc.value);

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - hflip (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    /************** set vflip configuration **************/
    vc.id = V4L2_CID_VFLIP;
    vc.value = vflip;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-V4L2_CID_VFLIP, vflip(%d)",vc.value);

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - vflip (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    /************** set destination **************/
    sFormat.type             = V4L2_BUF_TYPE_CAPTURE;
    sFormat.fmt.pix_mp.width         = dst->full_width;
    sFormat.fmt.pix_mp.height        = dst->full_height;
    sFormat.fmt.pix_mp.pixelformat    = dst->color_space;
    sFormat.fmt.pix_mp.num_planes    = dst->planes;
    sFormat.fmt.pix.field            = V4L2_FIELD_ANY;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-VIDIOC_S_FMT, type(%d), field(%d), plane(%d)"
        "pixelformat(0x%X), width(%d), height(%d)",
        sFormat.type,  sFormat.fmt.pix.field, sFormat.fmt.pix_mp.num_planes,
        sFormat.fmt.pix_mp.pixelformat, sFormat.fmt.pix_mp.width,  sFormat.fmt.pix_mp.height);

    ret = ioctl(fd, VIDIOC_S_FMT, &sFormat);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_FMT (%d)", __func__, ret);
        return -1;
    }

    /************** set destination window**************/
    crop.type     = V4L2_BUF_TYPE_CAPTURE;
    crop.c.left   = dst->start_x;
    crop.c.top    = dst->start_y;
    crop.c.width  = dst->width;
    crop.c.height = dst->height;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-VIDIOC_S_CROP, type(%d), XY(%d,%d), WH(%d,%d)",
        crop.type,  crop.c.left, crop.c.top, crop.c.width, crop.c.height);

    ret = ioctl(fd, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_CROP (%d)", __func__, ret);
        return -1;
    }

    /************** input buffer type **************/
    req.count       = 1;
    req.type        = V4L2_BUF_TYPE_CAPTURE;
    req.memory      = V4L2_MEMORY_USERPTR;

    SEC_HWC_Log(HWC_LOG_DEBUG,
        "fimc_v4l2_set_dst-VIDIOC_REQBUFS, count(%d), type(%d), memory(%d)",
        req.count,  req.type, req.memory);

    ret = ioctl (fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in VIDIOC_REQBUFS (%d)", __func__, ret);
        return -1;
    }

    return 0;
}

int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
    SEC_HWC_Log(HWC_LOG_DEBUG,"fimc_v4l2_stream_on-VIDIOC_STREAMON, type(%d)",type);

    if (-1 == ioctl(fd, VIDIOC_STREAMON, &type)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_STREAMON\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf, enum v4l2_buf_type type, int index)
{
    struct v4l2_plane plane[3];
    struct v4l2_buffer buf;
    int i;
    int ret;

    buf.length      = fimc_buf->planes;
    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.index       = index;
    buf.type        = type;

    if (buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
        buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
        for (i = 0; i < buf.length; i++) {
            plane[i].m.userptr = fimc_buf->base[i];
            plane[i].length = fimc_buf->size[i];
        }
    }
    buf.m.planes = plane;

    SEC_HWC_Log(HWC_LOG_DEBUG,"fimc_v4l2_queue-VIDIOC_QBUF, type(%d),"
        "length(%d), memory(%d), index(%d)",
        buf.type, buf.length, buf.memory, buf.index);

    ret = ioctl(fd, VIDIOC_QBUF, &buf);
    if (0 > ret) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_QBUF : (%d)", ret);
        return -1;
    }

    return 0;
}

int fimc_v4l2_dequeue(int fd, struct fimc_buf *fimc_buf, enum v4l2_buf_type type)
{
    struct v4l2_buffer          buf;
    struct v4l2_plane plane[3];

    buf.m.planes    = plane;
    buf.length      = fimc_buf->planes;
    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.type        = type;

    if (-1 == ioctl(fd, VIDIOC_DQBUF, &buf)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_DQBUF\n");
        return -1;
    }

    return buf.index;
}

int fimc_v4l2_stream_off(int fd, enum v4l2_buf_type type)
{
    SEC_HWC_Log(HWC_LOG_DEBUG,"fimc_v4l2_stream_off-VIDIOC_STREAMOFF, type(%d),",type);

    if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_STREAMOFF\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_clr_buf(int fd, enum v4l2_buf_type type)
{
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.memory  = V4L2_MEMORY_USERPTR;
    req.type    = type;

    SEC_HWC_Log(HWC_LOG_DEBUG,"fimc_v4l2_clr_buf-VIDIOC_REQBUFS,"
        "count(%d), memory(%d), type(%d)",
        req.count, req.memory, req.type);

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_REQBUFS");
    }

    return 0;
}

int fimc_v4l2_S_ctrl(int fd)
{
    struct v4l2_control vc;

    vc.id = V4L2_CID_CACHEABLE;
    vc.value = 1;

    SEC_HWC_Log(HWC_LOG_DEBUG,"fimc_v4l2_S_ctrl-VIDIOC_S_CTRL,"
        "id(%d), value(%d)",vc.id , vc.value);

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_S_CTRL");
        return -1;
    }

    return 0;
}

int fimc_handle_oneshot(int fd, struct fimc_buf *fimc_src_buf, struct fimc_buf *fimc_dst_buf)
{
#ifdef CHECK_FPS
    check_fps();
#endif

    if (fimc_v4l2_queue(fd, fimc_src_buf, V4L2_BUF_TYPE_OUTPUT, 0) < 0) {
         SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_queue()");
         return -1;
     }

    if (fimc_v4l2_queue(fd, fimc_dst_buf, V4L2_BUF_TYPE_CAPTURE, 0) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : DST v4l2_queue()");
        return -2;
    }

    if (fimc_v4l2_stream_on(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_stream_on()");
        return -3;
    }

    if (fimc_v4l2_stream_on(fd, V4L2_BUF_TYPE_CAPTURE) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : DST v4l2_stream_on()");
        return -4;
    }

    if (fimc_v4l2_dequeue(fd, fimc_src_buf, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_dequeue()");
        return -6;
    }

    if (fimc_v4l2_dequeue(fd, fimc_dst_buf, V4L2_BUF_TYPE_CAPTURE) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : DST v4l2_dequeue()");
        return -7;
    }

STREAM_OFF:
    if (fimc_v4l2_stream_off(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC  v4l2_stream_off()");
        return -8;
    }

    if (fimc_v4l2_stream_off(fd, V4L2_BUF_TYPE_CAPTURE) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : DST  v4l2_stream_off()");
        return -9;
    }

    if (fimc_v4l2_clr_buf(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_clr_buf()");
        return -10;
    }

    if (fimc_v4l2_clr_buf(fd, V4L2_BUF_TYPE_CAPTURE)< 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : DST v4l2_clr_buf()");
        return -11;
    }

    return 0;
}

static int memcpy_rect(void *dst, void *src, int fullW, int fullH, int realW, int realH, int format)
{
    unsigned char *srcCb, *srcCr;
    unsigned char *dstCb, *dstCr;
    unsigned char *srcY, *dstY;
    int srcCbOffset, srcCrOffset;
    int dstCbOffset, dstFrameOffset, dstCrOffset;
    int cbFullW, cbRealW, cbFullH, cbRealH;
    int ySrcFW, ySrcFH, ySrcRW, ySrcRH;
    int planes;
    int i;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "++memcpy_rect()::"
            "dst(0x%x),src(0x%x),f.w(%d),f.h(%d),r.w(%d),r.h(%d),format(0x%x)",
            (unsigned int)dst, (unsigned int)src, fullW, fullH, realW, realH, format);

// Set dst Y, Cb, Cr address for FIMC
    {
        cbFullW = fullW >> 1;
        cbRealW = realW >> 1;
        cbFullH = fullH >> 1;
        cbRealH = realH >> 1;
        dstFrameOffset = fullW * fullH;
        dstCrOffset = cbFullW * cbFullH;
        dstY = (unsigned char *)dst;
        dstCb = (unsigned char *)dst + dstFrameOffset;
        dstCr = (unsigned char *)dstCb + dstCrOffset;
    }

// Get src Y, Cb, Cr address for source buffer.
// Each address is aligned by 16's multiple for GPU both width and height.
    {
        ySrcFW = fullW;
        ySrcFH = fullH;
        ySrcRW = realW;
        ySrcRH = realH;
        srcCbOffset = EXYNOS4_ALIGN(ySrcRW,16)* EXYNOS4_ALIGN(ySrcRH,16);
        srcCrOffset = EXYNOS4_ALIGN(cbRealW,16)* EXYNOS4_ALIGN(cbRealH,16);
        srcY =  (unsigned char *)src;
        srcCb = (unsigned char *)src + srcCbOffset;
        srcCr = (unsigned char *)srcCb + srcCrOffset;
    }
    SEC_HWC_Log(HWC_LOG_DEBUG,
            "--memcpy_rect()::\n"
            "dstY(0x%x),dstCb(0x%x),dstCr(0x%x) \n"
            "srcY(0x%x),srcCb(0x%x),srcCr(0x%x) \n"
            "cbRealW(%d),cbRealH(%d)",
            (unsigned int)dstY,(unsigned int)dstCb,(unsigned int)dstCr,
            (unsigned int)srcY,(unsigned int)srcCb,(unsigned int)srcCr,
            cbRealW, cbRealH);

    if (format == HAL_PIXEL_FORMAT_YV12) { //YV12(Y,Cr,Cv)
        planes = 3;
//This is code for VE, deleted temporory by SSONG 2011.09.22
// This will be enabled later.
/*
        //as defined in hardware.h, cb & cr full_width should be aligned to 16. ALIGN(y_stride/2, 16).
        ////Alignment is hard coded to 16.
        ////for example...check frameworks/media/libvideoeditor/lvpp/VideoEditorTools.cpp file for UV stride cal
        cbSrcFW = (cbSrcFW + 15) & (~15);
        srcCbOffset = ySrcFW * fullH;
        srcCrOffset = srcCbOffset + ((cbSrcFW * fullH) >> 1);
        srcY =  (unsigned char *)src;
        srcCb = (unsigned char *)src + srcCbOffset;
        srcCr = (unsigned char *)src + srcCrOffset;
*/
    } else if ((format == HAL_PIXEL_FORMAT_YCbCr_420_P) ||
        (format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_LR) ||
        (format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_RL) ||
        (format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_LR) ||
        (format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_RL)) {
        planes = 3;
    } else if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP || format == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
        planes = 2;
    } else {
        SEC_HWC_Log(HWC_LOG_ERROR, "use default memcpy instead of memcpy_rect");
        return -1;
    }
//#define CHECK_PERF
#ifdef CHECK_PERF
    struct timeval start, end;
    gettimeofday(&start, NULL);
#endif
    for (i = 0; i < realH; i++)
        memcpy(dstY + fullW * i, srcY + ySrcFW * i, ySrcRW);
    if (planes == 2) {
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCb + ySrcFW * i, srcCb + ySrcFW * i, ySrcRW);
    } else if (planes == 3) {
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCb + cbFullW * i, srcCb + cbFullW * i, cbRealW);
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCr + cbFullW * i, srcCr + cbFullW * i, cbRealW);
    }
#ifdef CHECK_PERF
    gettimeofday(&end, NULL);
    SEC_HWC_Log(HWC_LOG_ERROR, "[COPY]=%d,",(end.tv_sec - start.tv_sec)*1000+(end.tv_usec - start.tv_usec)/1000);
#endif

    return 0;
}

/*****************************************************************************/
static int get_src_phys_addr(struct hwc_context_t *ctx,
        sec_img *src_img, sec_rect *src_rect)
{
    s5p_fimc_t *fimc = &ctx->fimc;
    struct s3c_mem_alloc *ptr_mem_alloc = &ctx->s3c_mem.mem_alloc[0];
    struct s3c_mem_dma_param s3c_mem_dma;

    unsigned int src_virt_addr  = 0;
    unsigned int src_phys_addr  = 0;
    unsigned int src_frame_size = 0;

    // error check routine
    if (0 == src_img->base) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s invalid src image base\n", __func__);
        return 0;
    }

            fimc->params.src.buf_addr_phy_rgb_y = src_img->base;
            fimc->params.src.buf_addr_phy_cb = src_img->base + src_img->uoffset;
            fimc->params.src.buf_addr_phy_cr = src_img->base + src_img->uoffset + src_img->voffset;
            src_phys_addr = fimc->params.src.buf_addr_phy_rgb_y;

    return src_phys_addr;
}

static int get_dst_phys_addr(struct hwc_context_t *ctx, sec_img *dst_img,
        sec_rect *dst_rect, int *dst_memcpy_flag)
{
    unsigned int dst_phys_addr  = 0;

        dst_phys_addr = dst_img->base;

    return dst_phys_addr;
}

static inline int rotateValueHAL2PP(unsigned char transform)
{
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:  return 90;
    case HAL_TRANSFORM_ROT_180: return 180;
    case HAL_TRANSFORM_ROT_270: return 270;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90: return 90;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90: return 90;
    case HAL_TRANSFORM_FLIP_H: return 0;
    case HAL_TRANSFORM_FLIP_V: return 0;
    }
    return 0;
}

static inline int hflipValueHAL2PP(unsigned char transform)
{
    int flip_flag = transform & 0x7;
    switch (flip_flag) {
    case HAL_TRANSFORM_FLIP_H:
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
        return 1;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_180:
    case HAL_TRANSFORM_ROT_270:
    case HAL_TRANSFORM_FLIP_V:
        break;
    }
    return 0;
}

static inline int vflipValueHAL2PP(unsigned char transform)
{
    int flip_flag = transform & 0x7;
    switch (flip_flag) {
    case HAL_TRANSFORM_FLIP_V:
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
        return 1;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_180:
    case HAL_TRANSFORM_ROT_270:
    case HAL_TRANSFORM_FLIP_H:
        break;
    }
    return 0;
}

static inline int multipleOf2(int number)
{
    if (number % 2 == 1)
        return (number - 1);
    else
        return number;
}

static inline int multipleOf4(int number)
{
    int remain_number = number % 4;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf8(int number)
{
    int remain_number = number % 8;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf16(int number)
{
    int remain_number = number % 16;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int widthOfPP( int pp_color_format, int number)
{
    switch (pp_color_format) {
    /* 422 1/2/3 plane */
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YUV422P:

    /* 420 2/3 plane */
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        return multipleOf2(number);

    default :
        return number;
    }

}

static inline int heightOfPP(int pp_color_format, int number)
{
    switch (pp_color_format) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        return multipleOf2(number);

    default :
        return number;
        break;
    }
    return number;
}

static unsigned int get_yuv_bpp(unsigned int fmt)
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

static unsigned int get_yuv_planes(unsigned int fmt)
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

static int runFimcCore(struct hwc_context_t *ctx,
        unsigned int src_phys_addr, sec_img *src_img, sec_rect *src_rect,
        uint32_t src_color_space,
        unsigned int dst_phys_addr, sec_img *dst_img, sec_rect *dst_rect,
        uint32_t dst_color_space, int transform)
{
    s5p_fimc_t        * fimc = &ctx->fimc;
    s5p_fimc_params_t * params = &(fimc->params);

    struct fimc_buf fimc_src_buf;
    int src_bpp, src_planes;

    struct fimc_buf fimc_dst_buf;
    int dst_bpp, dst_planes;
    unsigned int src_frame_size = 0;
    unsigned int dst_frame_size = 0;
    unsigned int frame_size = 0;

    bool src_cbcr_order = true;
    int rotate_value = rotateValueHAL2PP(transform);
    int hflip = hflipValueHAL2PP(transform);
    int vflip = vflipValueHAL2PP(transform);

    /* 1. param(fimc config)->src information
     *    - src_img,src_rect => s_fw,s_fh,s_w,s_h,s_x,s_y
     */
    params->src.full_width  = src_img->f_w;
    params->src.full_height = src_img->f_h;
    params->src.width       = src_rect->w;
    params->src.height      = src_rect->h;
    params->src.start_x     = src_rect->x;
    params->src.start_y     = src_rect->y;
    params->src.color_space = src_color_space;
    params->src.buf_addr_phy_rgb_y = src_phys_addr;

    params->dst.buf_addr_phy_rgb_y = dst_phys_addr;

    /* check src minimum */
    if (src_rect->w < 64 || src_rect->h < 32) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s src size is not supported by fimc : f_w=%d f_h=%d "
                "x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
                params->src.full_width, params->src.full_height,
                params->src.start_x, params->src.start_y,
                params->src.width, params->src.height,
                src_rect->w, src_rect->h,
                params->src.color_space);
        return -1;
    }

    params->dst.full_width  = dst_img->f_w;
    params->dst.full_height = dst_img->f_h;
    params->dst.start_x     = dst_rect->x;
    params->dst.start_y     = dst_rect->y;
    params->dst.width       = widthOfPP(dst_color_space, dst_rect->w);
    params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);
    params->dst.color_space = dst_color_space;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "runFimcCore()::"
            "SRC f.w(%d),f.h(%d),x(%d),y(%d),w(%d),h(%d)=>"
            "DST f.w(%d),f.h(%d),x(%d),y(%d),w(%d),h(%d)",
            params->src.full_width, params->src.full_height,
            params->src.start_x, params->src.start_y,
            params->src.width, params->src.height,
            params->dst.full_width, params->dst.full_height,
            params->dst.start_x, params->dst.start_y,
            params->dst.width, params->dst.height);

    /* check dst minimum */
#if (GSC_VERSION == GSC_EVT0)
    if (dst_rect->w  < 64 || dst_rect->h < 32) {
#else
    if (dst_rect->w  < 32 || dst_rect->h < 8) {
#endif

        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s dst size is not supported by fimc : f_w=%d f_h=%d "
                "x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
                params->dst.full_width, params->dst.full_height,
                params->dst.start_x, params->dst.start_y,
                params->dst.width, params->dst.height,
                dst_rect->w, dst_rect->h, params->dst.color_space);
        return -1;
    }

   /* 2. Set configuration related to destination (DMA-OUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */
    switch (dst_img->format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        dst_planes = 1;
        dst_bpp = 32;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
        dst_planes = 1;
        dst_bpp = 16;
        break;
    }

    dst_frame_size = params->dst.width * params->dst.height ;
    params->dst.planes = dst_planes;

    if (dst_planes == 1) {
        fimc_dst_buf.base[0] = params->dst.buf_addr_phy_rgb_y;
        if (dst_bpp == 32)
            fimc_dst_buf.size[0] = dst_frame_size * 4;
        else if (dst_bpp == 16)
             fimc_dst_buf.size[0] = dst_frame_size * 2;
    }

    if (fimc_v4l2_set_dst(fimc->dev_fd, &params->dst, rotate_value, hflip, vflip, dst_phys_addr) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "fimc_v4l2_set_dst is failed\n");
        return -1;
    }

    /* 3. Set input dma address (Y/RGB, Cb, Cr)
     * set source frame size
     */
    src_frame_size = params->src.full_width * params->src.full_height;
    fimc_src_buf.size[0] = src_frame_size;
    fimc_src_buf.size[1] = src_frame_size >> 2;
    fimc_src_buf.size[2] = src_frame_size >> 2;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "runFimcCore - Y_length=%d, U_length=%d, V_length=%d\n",
            fimc_src_buf.size[0], fimc_src_buf.size[1],fimc_src_buf.size[2]);

    /* set source Y image */
    fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
    /* set source Cb,Cr images for 2 or 3 planes */
    src_bpp    = get_yuv_bpp(src_color_space);
    src_planes = get_yuv_planes(src_color_space);
    if (2 == src_planes) {          /* 2 planes */
        frame_size = params->src.full_width * params->src.full_height;
        params->src.buf_addr_phy_cb =
            params->src.buf_addr_phy_rgb_y + frame_size;
        /* CbCr */
        fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
    } else if (3 == src_planes) {   /* 3 planes */
        frame_size = params->src.full_width * params->src.full_height;
        params->src.buf_addr_phy_cb =
            params->src.buf_addr_phy_rgb_y + frame_size;
        if (12 == src_bpp)
            params->src.buf_addr_phy_cr =
                params->src.buf_addr_phy_cb + (frame_size >> 2);
        else
            params->src.buf_addr_phy_cr =
                params->src.buf_addr_phy_cb + (frame_size >> 1);
        /* Cb, Cr */
        if (src_cbcr_order == true) {
            fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
            fimc_src_buf.base[2] = params->src.buf_addr_phy_cr;
        }
        else {
            fimc_src_buf.base[2] = params->src.buf_addr_phy_cb;
            fimc_src_buf.base[1] = params->src.buf_addr_phy_cr;
        }
    }

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "runFimcCore - Y=0x%X, U=0x%X, V=0x%X\n",
            fimc_src_buf.base[0], fimc_src_buf.base[1],fimc_src_buf.base[2]);

    int ret = 0;
    params->src.planes = src_planes;

   /* 4. Set configuration related to source (DMA-INPUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */
    if (fimc_v4l2_set_src(fimc->dev_fd, &params->src) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "fimc_v4l2_set_src is failed\n");
        return -1;
    }

    fimc_src_buf.planes = src_planes;
    fimc_dst_buf.planes = dst_planes;

    /* 5. Run FIMC
     *    - stream on => queue => dequeue => stream off => clear buf
     */
    ret = fimc_handle_oneshot(fimc->dev_fd, &fimc_src_buf, &fimc_dst_buf);

    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,"fimc_handle_oneshot = %d\n",ret);
        if (ret == -2) {
            fimc_v4l2_clr_buf(fimc->dev_fd, V4L2_BUF_TYPE_OUTPUT);
        } else if (ret == -3) {
            fimc_v4l2_clr_buf(fimc->dev_fd, V4L2_BUF_TYPE_OUTPUT);
            fimc_v4l2_clr_buf(fimc->dev_fd, V4L2_BUF_TYPE_CAPTURE);
        }
        return ret;
    }

    return 0;
}

#ifdef SUB_TITLES_HWC
int createG2d(sec_g2d_t *g2d)
{
    g2d->dev_fd = open(SEC_G2D_DEV_NAME, O_RDWR);

    if (g2d->dev_fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::G2d open error (%d)", __func__, errno);
        goto err;
    }

    return 0;
err:
    if (0 < g2d->dev_fd)
        close(g2d->dev_fd);
    g2d->dev_fd =0;

    return -1;
}

int destroyG2d(sec_g2d_t *g2d)
{
    // close
    if (0 < g2d->dev_fd)
        close(g2d->dev_fd);
    g2d->dev_fd = 0;

    return 0;
}
#endif

int createVideoDev(s5p_fimc_t *fimc)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_control vc;

    // open device file
    if (fimc->dev_fd <= 0)
        fimc->dev_fd = open(PP_DEVICE_DEV_NAME, O_RDWR);

    if (fimc->dev_fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Post processor open error (%d)",
                __func__, errno);
        goto err;
    }

    /* Initial debug log level for video driver.
        User can use command below to change log level.
        # echo 7 > /sys/module/gsc/parameters/gsc_dbg
        # echo 8 > /proc/sys/kernel/printk
        Each number for gsc_dgb means,
        3: error
        4: waring
        6: info
        7: debug
    */
    system("echo 3 > /sys/module/gsc/parameters/gsc_dbg");

    // check capability
    if (ioctl(fimc->dev_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "VIDIOC_QUERYCAP failed");
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%d has no streaming support", fimc->dev_fd);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%d is no video output", fimc->dev_fd);
        goto err;
    }

    /*
     * malloc fimc_outinfo structure
     */
    fmt.type = V4L2_BUF_TYPE_OUTPUT;
    if (ioctl(fimc->dev_fd, VIDIOC_G_FMT, &fmt) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_G_FMT", __func__);
        goto err;
    }

    return 0;

err:
    if (0 < fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd =0;

    return -1;
}

int destroyVideoDev(s5p_fimc_t *fimc)
{
    if (fimc->out_buf.virt_addr != NULL) {
        fimc->out_buf.virt_addr = NULL;
        fimc->out_buf.length = 0;
    }

    // close
    if (0 < fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd = 0;

    return 0;
}

int runFimc(struct hwc_context_t *ctx,
            struct sec_img *src_img, struct sec_rect *src_rect,
            struct sec_img *dst_img, struct sec_rect *dst_rect,
            uint32_t transform)
{
    s5p_fimc_t *  fimc = &ctx->fimc;

    unsigned int src_phys_addr  = 0;
    unsigned int dst_phys_addr  = 0;
    int          rotate_value   = 0;
    int          flag_force_memcpy = 0;
    int32_t      src_color_space;
    int32_t      dst_color_space;

    /* 1. source address and size */
    src_phys_addr = get_src_phys_addr(ctx, src_img, src_rect);
    if (0 == src_phys_addr)
        return -1;

    /* 2. destination address and size */
    dst_phys_addr = get_dst_phys_addr(ctx, dst_img, dst_rect, &flag_force_memcpy);
    if (0 == dst_phys_addr)
        return -2;

    /* 3. check whether fimc supports the src format */
    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    if (0 > src_color_space)
        return -3;
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    if (0 > dst_color_space)
        return -4;

    /* 4. FIMC: src_rect of src_img => dst_rect of dst_img */
    if (runFimcCore(ctx, src_phys_addr, src_img, src_rect,
                (uint32_t)src_color_space, dst_phys_addr, dst_img, dst_rect,
                (uint32_t)dst_color_space, transform) < 0)
        return -5;

    return 0;
}

#ifdef SUB_TITLES_HWC
static int get_g2d_src_phys_addr(struct hwc_context_t  *ctx, g2d_rect *src_rect)
{
    sec_g2d_t  *g2d = &ctx->g2d;
    struct s3c_mem_alloc *ptr_mem_alloc = &ctx->s3c_mem.mem_alloc[0];
#ifdef USE_HW_PMEM
    sec_pmem_alloc_t *pm_alloc = &ctx->sec_pmem.sec_pmem_alloc[0];
#endif

    unsigned int src_virt_addr  = 0;
    unsigned int src_phys_addr  = 0;
    unsigned int src_frame_size = 0;

    struct pmem_region region;

    // error check routine
    if (0 == src_rect->virt_addr) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s invalid src address\n", __func__);
        return 0;
    }

    src_frame_size = FRAME_SIZE(src_rect->color_format,
            src_rect->full_w, src_rect->full_h);
    if (src_frame_size == 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FRAME_SIZE fail", __func__);
        return 0;
    }

#ifdef USE_HW_PMEM
    if (0 <= checkPmem(&ctx->sec_pmem, 0, src_frame_size)) {
        src_virt_addr   = pm_alloc->virt_addr;
        src_phys_addr   = pm_alloc->phys_addr;
        pm_alloc->size  = src_frame_size;
    } else
#endif
    if (0 <= checkMem(&ctx->s3c_mem, 0, src_frame_size)) {
        src_virt_addr       = ptr_mem_alloc->vir_addr;
        src_phys_addr       = ptr_mem_alloc->phy_addr;
        ptr_mem_alloc->size = src_frame_size;
    } else {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::check_mem fail", __func__);
        return 0;
    }
    memcpy((void *)src_virt_addr, (void*)((unsigned int)src_rect->virt_addr), src_frame_size);

    return src_phys_addr;
}

int get_HAL_2_G2D_FORMAT(int format)
{
    switch (format) {
    case    HAL_PIXEL_FORMAT_RGBA_8888:     return  G2D_ABGR_8888;
    case    HAL_PIXEL_FORMAT_RGBX_8888:     return  G2D_XBGR_8888;
    case    HAL_PIXEL_FORMAT_BGRA_8888:     return  G2D_ARGB_8888;
    case    HAL_PIXEL_FORMAT_RGB_888:       return  G2D_PACKED_BGR_888;
    case    HAL_PIXEL_FORMAT_RGB_565:       return  G2D_RGB_565;
    case    HAL_PIXEL_FORMAT_RGBA_5551:     return  G2D_RGBA_5551;
    case    HAL_PIXEL_FORMAT_RGBA_4444:     return  G2D_RGBA_4444;
    default:
        return -1;
    }
}

static inline int rotateValueHAL2G2D(unsigned char transform)
{
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:  return G2D_ROT_90;
    case HAL_TRANSFORM_ROT_180: return G2D_ROT_180;
    case HAL_TRANSFORM_ROT_270: return G2D_ROT_270;
    default:
        return G2D_ROT_0;
    }
}

int runG2d(struct hwc_context_t *ctx, g2d_rect *src_rect, g2d_rect *dst_rect,
            uint32_t transform)
{
    sec_g2d_t *  g2d = &ctx->g2d;
    g2d_flag flag = {G2D_ROT_0, G2D_ALPHA_BLENDING_OPAQUE, 0, 0, 0, 0, 0, 0};
    int          rotate_value   = 0;

    // 1 : source address and size
    src_rect->phys_addr = get_g2d_src_phys_addr(ctx, src_rect);
    if (0 == src_rect->phys_addr)
        return -1;

    // 2 : destination address and size
    if (0 == dst_rect->phys_addr)
        return -2;

    // check whether g2d supports the src format
    src_rect->color_format = get_HAL_2_G2D_FORMAT(src_rect->color_format);
    if (0 > src_rect->color_format)
        return -3;

    dst_rect->color_format = get_HAL_2_G2D_FORMAT(dst_rect->color_format);
    if (0 > dst_rect->color_format)
        return -4;

    flag.rotate_val = rotateValueHAL2G2D(transform);

   // scale and rotate and alpha with FIMG
    if(stretchSecFimg(src_rect, dst_rect, &flag) < 0)
        return -5;

    return 0;
}
#endif

int createMem(struct s3c_mem_t *mem, unsigned int index, unsigned int size)
{
    struct s3c_mem_alloc *ptr_mem_alloc;
    struct s3c_mem_alloc mem_alloc_info;

    if (index >= NUM_OF_MEM_OBJ) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::invalid index (%d >= %d)",
                __func__, index, NUM_OF_MEM_OBJ);
        goto err;
    }

    ptr_mem_alloc = &mem->mem_alloc[index];

    if (mem->fd <= 0) {
        mem->fd = open(S3C_MEM_DEV_NAME, O_RDWR);
        if (mem->fd <= 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::open(%s) fail(%s)",
                    __func__, S3C_MEM_DEV_NAME, strerror(errno));
            goto err;
        }
    }

    // kcoolsw : what the hell of this line??
    if (0 == size)
        return 0;

    mem_alloc_info.size = size;

    if (ioctl(mem->fd, S3C_MEM_CACHEABLE_ALLOC, &mem_alloc_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3C_MEM_ALLOC(size : %d) fail",
                __func__, mem_alloc_info.size);
        goto err;
    }

    ptr_mem_alloc->phy_addr = mem_alloc_info.phy_addr;
    ptr_mem_alloc->vir_addr = mem_alloc_info.vir_addr;
    ptr_mem_alloc->size     = mem_alloc_info.size;

    return 0;

err:
    if (0 < mem->fd)
        close(mem->fd);
    mem->fd = 0;

    return 0;
}

int destroyMem(struct s3c_mem_t *mem)
{
    int i;
    struct s3c_mem_alloc *ptr_mem_alloc;

    if (mem->fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::invalied fd(%d) fail", __func__, mem->fd);
        return -1;
    }

    for (i = 0; i < NUM_OF_MEM_OBJ; i++) {
        ptr_mem_alloc = &mem->mem_alloc[i];

        if (0 != ptr_mem_alloc->vir_addr) {
            if (ioctl(mem->fd, S3C_MEM_FREE, ptr_mem_alloc) < 0) {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3C_MEM_FREE fail", __func__);
                return -1;
            }

            ptr_mem_alloc->phy_addr = 0;
            ptr_mem_alloc->vir_addr = 0;
            ptr_mem_alloc->size     = 0;
        }
    }

    close(mem->fd);
    mem->fd = 0;

    return 0;
}

int checkMem(struct s3c_mem_t *mem, unsigned int index, unsigned int size)
{
    int ret;
    struct s3c_mem_alloc *ptr_mem_alloc;
    struct s3c_mem_alloc mem_alloc_info;

    if (index >= NUM_OF_MEM_OBJ) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::invalid index (%d >= %d)", __func__,
                index, NUM_OF_MEM_OBJ);
        return -1;
    }

    if (mem->fd <= 0) {
        ret = createMem(mem, index, size);
        return ret;
    }

    ptr_mem_alloc = &mem->mem_alloc[index];

    if (ptr_mem_alloc->size < (int)size) {
        if (0 < ptr_mem_alloc->size) {
            // free allocated mem
            if (ioctl(mem->fd, S3C_MEM_FREE, ptr_mem_alloc) < 0) {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3C_MEM_FREE fail", __func__);
                return -1;
            }
        }

        // allocate mem with requested size
        mem_alloc_info.size = size;
        if (ioctl(mem->fd, S3C_MEM_CACHEABLE_ALLOC, &mem_alloc_info) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3C_MEM_ALLOC(size : %d)  fail",
                    __func__, mem_alloc_info.size);
            return -1;
        }

        ptr_mem_alloc->phy_addr = mem_alloc_info.phy_addr;
        ptr_mem_alloc->vir_addr = mem_alloc_info.vir_addr;
        ptr_mem_alloc->size     = mem_alloc_info.size;
    }

    return 0;
}

#ifdef USE_HW_PMEM
int createPmem(sec_pmem_t *pm, unsigned int buf_size)
{
    int    master_fd, err = 0, i;
    void  *base;
    unsigned int phys_base;
    size_t size, sub_size[NUM_OF_MEM_OBJ];
    struct pmem_region region;

    master_fd = open(PMEM_DEVICE_DEV_NAME, O_RDWR, 0);
    if (master_fd < 0) {
        pm->pmem_master_fd = -1;
        if (EACCES == errno) {
            return 0;
        } else {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::open(%s) fail(%s)",
                    __func__, PMEM_DEVICE_DEV_NAME, strerror(errno));
            return -errno;
        }
    }

    if (ioctl(master_fd, PMEM_GET_TOTAL_SIZE, &region) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "PMEM_GET_TOTAL_SIZE failed, default mode");
        size = 8<<20;   // 8 MiB
    } else {
        size = region.len;
    }

    base = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, master_fd, 0);
    if (base == MAP_FAILED) {
        SEC_HWC_Log(HWC_LOG_ERROR, "[%s] mmap failed : %d (%s)", __func__,
                errno, strerror(errno));
        base = 0;
        close(master_fd);
        master_fd = -1;
        return -errno;
    }

    if (ioctl(master_fd, PMEM_GET_PHYS, &region) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "PMEM_GET_PHYS failed, limp mode");
        region.offset = 0;
    }

    pm->pmem_master_fd   = master_fd;
    pm->pmem_master_base = base;
    pm->pmem_total_size  = size;
    //pm->pmem_master_phys_base = region.offset;
    phys_base = region.offset;

    // sec_pmem_alloc[0] for temporary buffer for source
    sub_size[0] = buf_size;
    sub_size[0] = roundUpToPageSize(sub_size[0]);

    for (i = 0; i < NUM_OF_MEM_OBJ; i++) {
        sec_pmem_alloc_t *pm_alloc = &(pm->sec_pmem_alloc[i]);
        int fd, ret;
        int offset = i ? sub_size[i-1] : 0;
        struct pmem_region sub = { offset, sub_size[i] };

        // create the "sub-heap"
        if (0 > (fd = open(PMEM_DEVICE_DEV_NAME, O_RDWR, 0))) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "[%s][index=%d] open failed (%dL) : %d (%s)",
                    __func__, i, __LINE__, errno, strerror(errno));
            return -errno;
        }

        // connect to it
        if (0 != (ret = ioctl(fd, PMEM_CONNECT, pm->pmem_master_fd))) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "[%s][index=%d] ioctl(PMEM_CONNECT) failed : %d (%s)",
                    __func__, i, errno, strerror(errno));
            close(fd);
            return -errno;
        }

        // make it available to the client process
        if (0 != (ret = ioctl(fd, PMEM_MAP, &sub))) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "[%s][index=%d] ioctl(PMEM_MAP) failed : %d (%s)",
                    __func__, i, errno, strerror(errno));
            close(fd);
            return -errno;
        }

        pm_alloc->fd         = fd;
        pm_alloc->total_size = sub_size[i];
        pm_alloc->offset     = offset;
        pm_alloc->virt_addr  = (unsigned int)base + (unsigned int)offset;
        pm_alloc->phys_addr  = (unsigned int)phys_base + (unsigned int)offset;

#if defined (PMEM_DEBUG)
        SEC_HWC_Log(HWC_LOG_DEBUG, "[%s] pm_alloc[%d] fd=%d total_size=%d "
                "offset=0x%x virt_addr=0x%x phys_addr=0x%x",
                __func__, i, pm_alloc->fd, pm_alloc->total_size,
                pm_alloc->offset, pm_alloc->virt_addr, pm_alloc->phys_addr);
#endif
    }

    return err;
}

int destroyPmem(sec_pmem_t *pm)
{
    int i, err;

    for (i=0; i<NUM_OF_MEM_OBJ; i++) {
        sec_pmem_alloc_t *pm_alloc = &(pm->sec_pmem_alloc[i]);

        if (0 <= pm_alloc->fd) {
            struct pmem_region sub = { pm_alloc->offset, pm_alloc->total_size };

            if (0 > (err = ioctl(pm_alloc->fd, PMEM_UNMAP, &sub)))
                SEC_HWC_Log(HWC_LOG_ERROR,
                        "[%s][index=%d] ioctl(PMEM_UNMAP) failed : %d (%s)",
                        __func__, i, errno, strerror(errno));
#if defined (PMEM_DEBUG)
            else
                SEC_HWC_Log(HWC_LOG_DEBUG,
                        "[%s] pm_alloc[%d] unmap fd=%d total_size=%d offset=0x%x",
                        __func__, i, pm_alloc->fd, pm_alloc->total_size,
                        pm_alloc->offset);
#endif
            close(pm_alloc->fd);

            pm_alloc->fd         = -1;
            pm_alloc->total_size = 0;
            pm_alloc->offset     = 0;
            pm_alloc->virt_addr  = 0;
            pm_alloc->phys_addr  = 0;
        }
    }

    if (0 <= pm->pmem_master_fd) {
        munmap(pm->pmem_master_base, pm->pmem_total_size);
        close(pm->pmem_master_fd);
        pm->pmem_master_fd = -1;
    }

    pm->pmem_master_base = 0;
    pm->pmem_total_size  = 0;

    return 0;
}

int checkPmem(sec_pmem_t *pm, unsigned int index, unsigned int requested_size)
{
    sec_pmem_alloc_t *pm_alloc = &(pm->sec_pmem_alloc[index]);

    if (0 < pm_alloc->virt_addr &&
            requested_size <= (unsigned int)(pm_alloc->total_size))
        return 0;

    pm_alloc->size = 0;
    return -1;
}

#endif
