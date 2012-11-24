/*
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

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "sec_utils.h"
#include "s5p_tvout.h"
#include "SecFimc.h"
#if defined(BOARD_USES_FIMGAPI)
#include "FimgApi.h"
#endif

#include "audio.h"
#include "video.h"
#include "../libhdmi/libsForhdmi/libedid/libedid.h"
#include "../libhdmi/libsForhdmi/libcec/libcec.h"

#include "SecHdmiCommon.h"
#include "SecHdmiV4L2Utils.h"

namespace android {

unsigned int output_type = V4L2_OUTPUT_TYPE_DIGITAL;
v4l2_std_id t_std_id     = V4L2_STD_1080P_30;
int g_hpd_state   = HPD_CABLE_OUT;
unsigned int g_hdcp_en = 0;

int fp_tvout    = -1;
int fp_tvout_v  = -1;
int fp_tvout_g0 = -1;
int fp_tvout_g1 = -1;

struct vid_overlay_param vo_param;

#if defined(BOARD_USES_FIMGAPI)
unsigned int g2d_reserved_memory[HDMI_G2D_OUTPUT_BUF_NUM];
unsigned int g2d_reserved_memory_size   = 0;
unsigned int cur_g2d_address            = 0;
unsigned int g2d_buf_index              = 0;
#endif

void display_menu(void)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    LOG_LIB_HDMI_V4L2("%s", __func__);

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    LOGI("=============== HDMI Audio  =============\n");

    if (EDIDAudioModeSupport(&audio))
        LOGI("=  2CH_PCM 44100Hz audio supported      =\n");

    LOGI("========= HDMI Mode & Color Space =======\n");

    video.mode = HDMI;
    if (EDIDHDMIModeSupport(&video)) {
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
            LOGI("=  1. HDMI(YCbCr)                       =\n");

        video.colorSpace = HDMI_CS_RGB;
        if (EDIDColorSpaceSupport(&video))
            LOGI("=  2. HDMI(RGB)                         =\n");
    } else {
        video.mode = DVI;
        if (EDIDHDMIModeSupport(&video))
            LOGI("=  3. DVI                               =\n");
    }

    LOGI("===========    HDMI Rseolution   ========\n");

    /* 480P */
    video.resolution = v720x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  4. 480P_60_16_9    (0x04000000)    =\n");

    video.resolution = v640x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  5. 480P_60_4_3 (0x05000000)    =\n");

    /* 576P */
    video.resolution = v720x576p_50Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  6. 576P_50_16_9    (0x06000000)    =\n");

    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  7. 576P_50_4_3 (0x07000000)    =\n");

    /* 720P 60 */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  8. 720P_60         (0x08000000)    =\n");

    /* 720P_50 */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  9. 720P_50         (0x09000000)    =\n");

    /* 1080P_60 */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  a. 1080P_60        (0x0a000000)    =\n");

    /* 1080P_50 */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  b. 1080P_50        (0x0b000000)    =\n");

    /* 1080I_60 */
    video.resolution = v1920x1080i_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  c. 1080I_60        (0x0c000000)    =\n");

    /* 1080I_50 */
    video.resolution = v1920x1080i_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  d. 1080I_50        (0x0d000000)    =\n");

    /* 1080P_30 */
    video.resolution = v1920x1080p_30Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  e. 1080P_30        (0x12000000)    =\n");

    LOGI("===========    HDMI 3D Format   ========\n");

    /* 720P_60_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  f. 720P_60_SBS_HALF    (0x13000000)    =\n");

    /* 720P_59_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  10. 720P_59_SBS_HALF    (0x14000000)    =\n");

    /* 720P_50_TB */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  11. 720P_50_TB          (0x15000000)    =\n");

    /* 1080P_24_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  12. 1080P_24_TB          (0x16000000)    =\n");

    /* 1080P_23_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        LOGI("=  13. 1080P_24_TB          (0x17000000)    =\n");
    LOGI("=========================================\n");
}

int tvout_open(const char *fp_name)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int fp;

    fp = open(fp_name, O_RDWR);
    if (fp < 0)
        LOGE("drv (%s) open failed!!\n", fp_name);

    return fp;
}

int tvout_init(v4l2_std_id std_id)
{
    LOG_LIB_HDMI_V4L2("%s:: std_id = 0x%x", __func__, std_id);

    int ret;
    struct v4l2_output output;
    struct v4l2_standard std;
    v4l2_std_id std_g_id;
    struct tvout_param tv_g_param;

    unsigned int matched = 0, i = 0;
    int output_index;

    // It was initialized already
    if (fp_tvout <= 0) {
        fp_tvout = tvout_open(TVOUT_DEV);
        if (fp_tvout < 0) {
            LOGE("tvout video drv open failed\n");
            return -1;
        }
    }

    if (output_type >= V4L2_OUTPUT_TYPE_DIGITAL &&
        output_type <= V4L2_OUTPUT_TYPE_DVI)
        if (ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, g_hdcp_en) < 0)
            LOGE("tvout_init" "VIDIOC_HDCP_ENABLE failed %d\n", errno);

    /* ============== query capability============== */
    tvout_v4l2_querycap(fp_tvout);

    tvout_v4l2_enum_std(fp_tvout, &std, std_id);

    // set std
    tvout_v4l2_s_std(fp_tvout, std_id);
    tvout_v4l2_g_std(fp_tvout, &std_g_id);

    i = 0;

    do {
        output.index = i;
        ret = tvout_v4l2_enum_output(fp_tvout, &output);
        if (output.type == output_type) {
            matched = 1;
            break;
        }
        i++;
    } while (ret >=0);

    if (!matched) {
        LOGE("no matched output type [type : 0x%08x]\n", output_type);
        return -1;
    }

    // set output
    tvout_v4l2_s_output(fp_tvout, output.index);
    output_index = 0;
    tvout_v4l2_g_output(fp_tvout, &output_index);

    //set fmt param
    vo_param.src.base_y         = (void *)0x0;
    vo_param.src.base_c         = (void *)0x0;
    vo_param.src.pix_fmt.width  = 0;
    vo_param.src.pix_fmt.height = 0;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    vo_param.src.pix_fmt.pixelformat = V4L2_PIX_FMT_NV12T;

    vo_param.src_crop.left    = 0;
    vo_param.src_crop.top     = 0;
    vo_param.src_crop.width   = 0;
    vo_param.src_crop.height  = 0;

    return fp_tvout;
}

int tvout_deinit()
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    if (0 < fp_tvout) {
        close(fp_tvout);
        fp_tvout = -1;
    }
    return 0;
}

int tvout_v4l2_querycap(int fp)
{
    LOG_LIB_HDMI_V4L2("%s:: fp = 0x%x", __func__, fp);

    struct v4l2_capability cap;
    int ret;

    ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);

    if (ret < 0) {
        LOGE("tvout_v4l2_querycap" "VIDIOC_QUERYCAP failed %d\n", errno);
        return ret;
    }

    LOG_LIB_HDMI_V4L2("tvout_v4l2_querycap" "DRIVER : %s, CARD : %s, CAP.: 0x%08x\n",
            cap.driver, cap.card, cap.capabilities);

    return ret;
}

/*
   ioctl VIDIOC_G_STD, VIDIOC_S_STD
   To query and select the current video standard applications use the VIDIOC_G_STD and
   VIDIOC_S_STD ioctls which take a pointer to a v4l2_std_id type as argument. VIDIOC_G_STD can
   return a single flag or a set of flags as in struct v4l2_standard field id
   */

int tvout_v4l2_g_std(int fp, v4l2_std_id *std_id)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, VIDIOC_G_STD, std_id);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_std" "VIDIOC_G_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_s_std(int fp, v4l2_std_id std_id)
{
    LOG_LIB_HDMI_V4L2("%s:: std_id = 0x%x", __func__, std_id);

    int ret;

    ret = ioctl(fp, VIDIOC_S_STD, &std_id);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_std" "VIDIOC_S_STD failed %d\n", errno);
        return ret;
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUMSTD
   To query the attributes of a video standard, especially a custom (driver defined) one, applications
   initialize the index field of struct v4l2_standard and call the VIDIOC_ENUMSTD ioctl with a pointer
   to this structure. Drivers fill the rest of the structure or return an EINVAL error code when the index
   is out of bounds.
   */
int tvout_v4l2_enum_std(int fp, struct v4l2_standard *std, v4l2_std_id std_id)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    std->index = 0;
    while (0 == ioctl (fp, VIDIOC_ENUMSTD, std)) {
        if (std->id & std_id)
            LOGV("tvout_v4l2_enum_std" "Current video standard: %s\n", std->name);

        std->index++;
    }

    return 0;
}

/*
   ioctl VIDIOC_ENUMOUTPUT
   To query the attributes of a video outputs applications initialize the index field of struct v4l2_output
   and call the VIDIOC_ENUMOUTPUT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code when the index is out of bounds
   */
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, VIDIOC_ENUMOUTPUT, output);

    if (ret >=0)
        LOGV("tvout_v4l2_enum_output" "enum. output [index = %d] :: type : 0x%08x , name = %s\n",
                output->index,output->type,output->name);

    return ret;
}

/*
   ioctl VIDIOC_G_OUTPUT, VIDIOC_S_OUTPUT
   To query the current video output applications call the VIDIOC_G_OUTPUT ioctl with a pointer to an
   integer where the driver stores the number of the output, as in the struct v4l2_output index field.
   This ioctl will fail only when there are no video outputs, returning the EINVAL error code
   */
int tvout_v4l2_s_output(int fp, int index)
{
    LOG_LIB_HDMI_V4L2("%s:: index = 0x%x", __func__, index);

    int ret;

    ret = ioctl(fp, VIDIOC_S_OUTPUT, &index);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_output" "VIDIOC_S_OUTPUT failed %d\n", errno);
        return ret;
    }

    return ret;
}

int tvout_v4l2_g_output(int fp, int *index)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, VIDIOC_G_OUTPUT, index);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_output" "VIDIOC_G_OUTPUT failed %d\n", errno);
        return ret;
    } else {
        LOGV("tvout_v4l2_g_output" "Current output index %d\n", *index);
    }

    return ret;
}

/*
   ioctl VIDIOC_ENUM_FMT
   To enumerate image formats applications initialize the type and index field of struct v4l2_fmtdesc
   and call the VIDIOC_ENUM_FMT ioctl with a pointer to this structure. Drivers fill the rest of the
   structure or return an EINVAL error code. All formats are enumerable by beginning at index zero
   and incrementing by one until EINVAL is returned.
   */
int tvout_v4l2_enum_fmt(int fp, struct v4l2_fmtdesc *desc)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    desc->index = 0;
    while (0 == ioctl(fp, VIDIOC_ENUM_FMT, desc)) {
        LOGV("tvout_v4l2_enum_fmt" "enum. fmt [id : 0x%08x] :: type = 0x%08x, name = %s, pxlfmt = 0x%08x\n",
                desc->index,
                desc->type,
                desc->description,
                desc->pixelformat);
        desc->index++;
    }

    return 0;
}

int tvout_v4l2_g_fmt(int fp, int buf_type, void* ptr)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;
    struct v4l2_format format;
    struct v4l2_pix_format_s5p_tvout *fmt_param = (struct v4l2_pix_format_s5p_tvout*)ptr;

    format.type = (enum v4l2_buf_type)buf_type;

    ret = ioctl(fp, VIDIOC_G_FMT, &format);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_fmt" "type : %d, VIDIOC_G_FMT failed %d\n", buf_type, errno);
        return ret;
    } else {
        memcpy(fmt_param, format.fmt.raw_data, sizeof(struct v4l2_pix_format_s5p_tvout));
        LOGV("tvout_v4l2_g_fmt" "get. fmt [base_c : 0x%08x], [base_y : 0x%08x] type = 0x%08x, width = %d, height = %d\n",
                fmt_param->base_c,
                fmt_param->base_y,
                fmt_param->pix_fmt.pixelformat,
                fmt_param->pix_fmt.width,
                fmt_param->pix_fmt.height);
    }

    return 0;
}

int tvout_v4l2_s_fmt(int fp, int buf_type, void *ptr)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    struct v4l2_format format;
    int ret;

    format.type = (enum v4l2_buf_type)buf_type;
    switch (buf_type) {
    case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        format.fmt.win =  *((struct v4l2_window *) ptr);
        break;

    case V4L2_BUF_TYPE_PRIVATE: {
        struct v4l2_vid_overlay_src *fmt_param =
            (struct v4l2_vid_overlay_src *) ptr;

        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_vid_overlay_src));
        break;
    }
    case V4L2_BUF_TYPE_VIDEO_OUTPUT: {
        struct v4l2_pix_format_s5p_tvout *fmt_param =
            (struct v4l2_pix_format_s5p_tvout *)ptr;
        memcpy(format.fmt.raw_data, fmt_param,
                sizeof(struct v4l2_pix_format_s5p_tvout));
        break;
    }
    default:
        break;
    }

    ret = ioctl(fp, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_fmt [tvout_v4l2_s_fmt] : type : %d, VIDIOC_S_FMT failed %d\n",
                buf_type, errno);
        return ret;
    }
    return 0;

}

int tvout_v4l2_g_fbuf(int fp, struct v4l2_framebuffer *frame)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, VIDIOC_G_FBUF, frame);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }

    LOGV("tvout_v4l2_g_fbuf" "get. fbuf: base = 0x%08X, pixel format = %d\n",
            frame->base,
            frame->fmt.pixelformat);
    return 0;
}

int tvout_v4l2_s_fbuf(int fp, struct v4l2_framebuffer *frame)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, VIDIOC_S_FBUF, frame);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_fbuf" "VIDIOC_STREAMON failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_s_baseaddr(int fp, void *base_addr)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;

    ret = ioctl(fp, S5PTVFB_WIN_SET_ADDR, base_addr);
    if (ret < 0) {
        LOGE("tvout_v4l2_baseaddr" "VIDIOC_S_BASEADDR failed %d\n", errno);
        return ret;
    }
    return 0;
}

int tvout_v4l2_g_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret;
    struct v4l2_crop crop;
    crop.type = (enum v4l2_buf_type)type;
    ret = ioctl(fp, VIDIOC_G_CROP, &crop);
    if (ret < 0) {
        LOGE("tvout_v4l2_g_crop" "VIDIOC_G_CROP failed %d\n", errno);
        return ret;
    }

    rect->left  = crop.c.left;
    rect->top   = crop.c.top;
    rect->width = crop.c.width;
    rect->height    = crop.c.height;

    LOGV("tvout_v4l2_g_crop" "get. crop : left = %d, top = %d, width  = %d, height = %d\n",
            rect->left,
            rect->top,
            rect->width,
            rect->height);
    return 0;
}

int tvout_v4l2_s_crop(int fp, unsigned int type, struct v4l2_rect *rect)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    struct v4l2_crop crop;
    int ret;

    crop.type   = (enum v4l2_buf_type)type;

    crop.c.left     = rect->left;
    crop.c.top      = rect->top;
    crop.c.width    = rect->width;
    crop.c.height   = rect->height;

    ret = ioctl(fp, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
        LOGE("tvout_v4l2_s_crop" "VIDIOC_S_CROP failed %d\n", errno);
        return ret;
    }

    return 0;
}

int tvout_v4l2_start_overlay(int fp)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret, start = 1;

    ret = ioctl(fp, VIDIOC_OVERLAY, &start);
    if (ret < 0) {
        LOGE("tvout_v4l2_start_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}

int tvout_v4l2_stop_overlay(int fp)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int ret, stop =0;

    ret = ioctl(fp, VIDIOC_OVERLAY, &stop);
    if (ret < 0) {
        LOGE("tvout_v4l2_stop_overlay" "VIDIOC_OVERLAY failed\n");
        return ret;
    }

    return ret;
}

int hdmi_init_layer(int layer)
{
    int fd = -1;

    LOG_LIB_HDMI_V4L2("### %s (layer = %d) called", __func__, layer);

    switch (layer) {
    case HDMI_LAYER_VIDEO :
        if (fp_tvout_v <= 0) {
            fp_tvout_v = tvout_open(TVOUT_DEV_V);
            if (fp_tvout_v < 0) {
                LOGE("tvout video layer open failed\n");
                return -1;
            }
            fd = fp_tvout_v;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (fp_tvout_g0 <= 0) {
            fp_tvout_g0 = fb_open(TVOUT_FB_G0);
            if (fp_tvout_g0 < 0) {
                LOGE("tvout graphic layer 0 open failed\n");
                return -1;
            }
            fd = fp_tvout_g0;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (fp_tvout_g1 <= 0) {
            fp_tvout_g1 = fb_open(TVOUT_FB_G1);
            if (fp_tvout_g1 < 0) {
                LOGE("tvout graphic layer 1 open failed\n");
                return -1;
            }
            fd = fp_tvout_g1;
        }
        break;
    default :
        LOGE("%s::unmathced layer(%d) fail", __func__, layer);
        fd = -1;
        break;
    }

    return fd;
}

int hdmi_deinit_layer(int layer)
{
    int ret = 0;

    LOG_LIB_HDMI_V4L2("### %s(layer = %d) called", __func__, layer);

    switch (layer) {
    case HDMI_LAYER_VIDEO :
        if (0 < fp_tvout_v) {
            close(fp_tvout_v);
            fp_tvout_v = -1;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0 :
        if (0 < fp_tvout_g0) {
            close(fp_tvout_g0);
            fp_tvout_g0 = -1;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1 :
        if (0 < fp_tvout_g1) {
            close(fp_tvout_g1);
            fp_tvout_g1 = -1;
        }
        break;
    default :
        LOGE("%s::unmathced layer(%d) fail", __func__, layer);
        ret = -1;
        break;
    }

    return ret;
}

#define ROUND_UP(value, boundary) ((((uint32_t)(value)) + \
                                  (((uint32_t) boundary)-1)) & \
                                  (~(((uint32_t) boundary)-1)))

void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect)
{
    if (dst_w * src_h <= dst_h * src_w) {
        dst_rect->left   = 0;
        dst_rect->top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        dst_rect->width  = dst_w;
        dst_rect->height = ((dst_w * src_h) / src_w);
    } else {
        dst_rect->left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        dst_rect->top    = 0;
        dst_rect->width  = ((dst_h * src_w) / src_h);
        dst_rect->height = dst_h;
    }
}

int hdmi_set_v_param(int layer,
        int src_w, int src_h, int colorFormat,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_w, int dst_h)
{
    LOG_LIB_HDMI_V4L2("%s", __func__);

    int round_up_src_w;
    int round_up_src_h;
    if (fp_tvout_v <= 0) {
        LOGE("fp_tvout is < 0 fail\n");
        return -1;
    }

    /* src_w, src_h round up to DWORD because of VP restriction */
#if defined(SAMSUNG_EXYNOS4x12)
    round_up_src_w = ROUND_UP(src_w, 16);
#else defined(SAMSUNG_EXYNOS4210)
    round_up_src_w = ROUND_UP(src_w, 8);
#endif
    round_up_src_h = ROUND_UP(src_h, 8);

    vo_param.src.base_y         = (void *)src_y_address;
    vo_param.src.base_c         = (void *)src_c_address;
    vo_param.src.pix_fmt.width  = round_up_src_w;
    vo_param.src.pix_fmt.height = round_up_src_h;
    vo_param.src.pix_fmt.field  = V4L2_FIELD_NONE;
    vo_param.src.pix_fmt.pixelformat = colorFormat;

    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src);

    vo_param.src_crop.width   = src_w;
    vo_param.src_crop.height  = src_h;

    tvout_v4l2_s_crop(fp_tvout_v, V4L2_BUF_TYPE_PRIVATE, &vo_param.src_crop);

    if (dst_w * src_h <= dst_h * src_w) {
        vo_param.dst_win.w.left   = 0;
        vo_param.dst_win.w.top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        vo_param.dst_win.w.width  = dst_w;
        vo_param.dst_win.w.height = ((dst_w * src_h) / src_w);
    } else {
        vo_param.dst_win.w.left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        vo_param.dst_win.w.top    = 0;
        vo_param.dst_win.w.width  = ((dst_h * src_w) / src_h);
        vo_param.dst_win.w.height = dst_h;
    }

    vo_param.dst.fmt.priv = 10;
    vo_param.dst_win.global_alpha = 255;
    tvout_v4l2_s_fbuf(fp_tvout_v, &vo_param.dst);
    tvout_v4l2_s_fmt(fp_tvout_v, V4L2_BUF_TYPE_VIDEO_OVERLAY, &vo_param.dst_win);

    return 0;
}

int hdmi_gl_set_param(int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_y_address, unsigned int src_c_address,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal)
{
#if defined(BOARD_USES_FIMGAPI)
    int             dst_color_format;
    int             dst_bpp;
    unsigned char   *dst_addr;
    fimg2d_blit     BlitParam;
    fimg2d_param    g2d_param;
    rotation        g2d_rotation;

    fimg2d_addr srcAddr;
    fimg2d_image srcImage;
    fimg2d_rect srcRect;

    fimg2d_addr dstAddr;
    fimg2d_image dstImage;
    fimg2d_rect dstRect;

    fimg2d_scale Scaling;
    fimg2d_repeat Repeat;
    fimg2d_bluscr Bluscr;
    fimg2d_clip Clipping;

    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    int fp_tvout_g;

    if(layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    switch (t_std_id) {
    case V4L2_STD_1080P_60:
    case V4L2_STD_1080P_30:
    case V4L2_STD_1080I_60:
    case V4L2_STD_TVOUT_720P_60_SBS_HALF:
    case V4L2_STD_TVOUT_720P_59_SBS_HALF:
    case V4L2_STD_TVOUT_1080P_24_TB:
    case V4L2_STD_TVOUT_1080P_23_TB:
        dst_color_format = CF_ARGB_8888;
        dst_bpp = 4;
        var.bits_per_pixel = 32;
        var.transp.length = 8;
        break;
    case V4L2_STD_480P_60_16_9:
    case V4L2_STD_576P_50_16_9:
    case V4L2_STD_720P_60:
    case V4L2_STD_TVOUT_720P_50_TB:
    default:
        dst_color_format = CF_ARGB_4444;
        dst_bpp = 2;
        var.bits_per_pixel = 16;
        var.transp.length = 4;
        break;
    }

    static unsigned int prev_src_addr = 0;

    if ((cur_g2d_address == 0) || (src_y_address != prev_src_addr)) {
        dst_addr = (unsigned char *)g2d_reserved_memory[g2d_buf_index];

        g2d_buf_index++;
        if (g2d_buf_index >= HDMI_G2D_OUTPUT_BUF_NUM)
            g2d_buf_index = 0;

        cur_g2d_address = (unsigned int)dst_addr;
        prev_src_addr = src_y_address;


        srcAddr = {(addr_space)ADDR_PHYS, (unsigned long)src_y_address};
        srcRect = {0, 0, src_w, src_h};
        srcImage = {src_w, src_h, src_w*4, AX_RGB, CF_ARGB_8888, srcAddr, srcAddr, srcRect, false};

        dstAddr = {(addr_space)ADDR_PHYS, (unsigned long)dst_addr};
        dstRect = {0, 0, dst_w, dst_h};
        dstImage = {dst_w, dst_h, dst_w*dst_bpp, AX_RGB, (color_format)dst_color_format, dstAddr, dstAddr, dstRect, false};

        if (rotVal == 0 || rotVal == 180)
            Scaling = {SCALING_BILINEAR, src_w, src_h, dst_w, dst_h};
        else
            Scaling = {SCALING_BILINEAR, src_w, src_h, dst_h, dst_w};

        switch (rotVal) {
        case 0:
            g2d_rotation = ORIGIN;
            break;
        case 90:
            g2d_rotation = ROT_90;
            break;
        case 180:
            g2d_rotation = ROT_180;
            break;
        case 270:
            g2d_rotation = ROT_270;
            break;
        default:
            LOGE("%s::invalid rotVal(%d) fail", __func__, rotVal);
            return -1;
            break;
        }
        Repeat = {NO_REPEAT, 0};
        Bluscr = {OPAQUE, 0, 0};
        Clipping = {false, 0,  0, 0, 0};

        g2d_param = {0, 0xff, 0, g2d_rotation, NON_PREMULTIPLIED, Scaling, Repeat, Bluscr, Clipping};
        BlitParam = {BLIT_OP_SRC, g2d_param, &srcImage, NULL, NULL, &dstImage, BLIT_SYNC, 0};

        if (stretchFimgApi(&BlitParam) < 0) {
            LOGE("%s::stretchFimgApi() fail", __func__);
            return -1;
        }

        var.xres = dst_w;
        var.yres = dst_h;

        var.xres_virtual = var.xres;
        var.yres_virtual = var.yres;
        var.xoffset = 0;
        var.yoffset = 0;
        var.width = 0;
        var.height = 0;
        var.activate = FB_ACTIVATE_FORCE;

        window.x = dst_x;
        window.y = dst_y;

        tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)dst_addr);
        put_vscreeninfo(fp_tvout_g, &var);

        if (ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window) < 0) {
            LOGE("%s::S5PTVFB_WIN_POSITION ioctl failed.", __func__);
            return -1;
        }
    }

    return 0;
#else
    struct fb_var_screeninfo var;
    struct s5ptvfb_user_window window;

    struct overlay_param ov_param;

    // set base address for grp layer0 of mixer
    int fp_tvout_g;

    LOG_LIB_HDMI_V4L2("hdmi_gl_set_param:: \n \\
                layer=%d,\n \\
                srcColorFormat=%d,\n \\
                src_w=%d, src_h=%d,\n\\
                src_y_address=0x%x, src_c_address=0x%x,\n\\
                dst_x=%d, dst_y=%d, dst_w=%d, dst_h=%d ",
                layer,
                srcColorFormat,
                src_w, src_h,
                src_y_address, src_c_address,
                dst_x, dst_y, dst_w, dst_h);

    if (layer == HDMI_LAYER_GRAPHIC_0)
        fp_tvout_g = fp_tvout_g0;
    else
        fp_tvout_g = fp_tvout_g1;

    var.xres = src_w;
    var.yres = src_h;
    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = src_w;
    var.height = src_h;
    var.activate = FB_ACTIVATE_FORCE;
    if (srcColorFormat == HAL_PIXEL_FORMAT_RGB_565) {
        var.bits_per_pixel = 16;
        var.transp.length = 0;
    }
    else {
        var.bits_per_pixel = 32;
        var.transp.length = 8;
    }

    window.x = dst_x;
    window.y = dst_y;

    tvout_v4l2_s_baseaddr(fp_tvout_g, (void *)src_y_address);
    put_vscreeninfo(fp_tvout_g, &var);
    if (ioctl(fp_tvout_g, S5PTVFB_WIN_POSITION, &window) < 0) {
        LOGE("%s:: S5PTVFB_WIN_POSITION ioctl failed.", __func__);
        return -1;
    }

    return 0;
#endif
}

int hdmi_cable_status()
{
    int cable_status = 0;
    int fp_hpd = 0;

    fp_hpd = open(HPD_DEV, O_RDWR);
    if (fp_hpd <= 0) {
        LOGE("hpd drv open failed\n");
        return -1;
    }

    //Delay about 0.3s
    usleep(500000);
    if (ioctl(fp_hpd, HPD_GET_STATE, &cable_status) < 0) {
        LOGE("hpd drv HPD_GET_STATE ioctl failed\n");
        cable_status = -1;
    }

    close(fp_hpd);

    return cable_status;
}

int hdmi_outputmode_2_v4l2_output_type(int output_mode)
{
    int v4l2_output_type = -1;

    switch (output_mode) {
    case HDMI_OUTPUT_MODE_YCBCR:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DIGITAL;
        break;
    case HDMI_OUTPUT_MODE_RGB:
        v4l2_output_type = V4L2_OUTPUT_TYPE_HDMI_RGB;
        break;
    case HDMI_OUTPUT_MODE_DVI:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DVI;
        break;
    case COMPOSITE_OUTPUT_MODE:
        v4l2_output_type = V4L2_OUTPUT_TYPE_COMPOSITE;
        break;
    default:
        LOGE("%s::unmathced HDMI_mode(%d)", __func__, output_mode);
        v4l2_output_type = -1;
        break;
    }

    return v4l2_output_type;
}

int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type)
{
    int outputMode = -1;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL:
        outputMode = HDMI_OUTPUT_MODE_YCBCR;
        break;
    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        outputMode = HDMI_OUTPUT_MODE_RGB;
        break;
    case V4L2_OUTPUT_TYPE_DVI:
        outputMode = HDMI_OUTPUT_MODE_DVI;
        break;
    case V4L2_OUTPUT_TYPE_COMPOSITE:
        outputMode = COMPOSITE_OUTPUT_MODE;
        break;
    default:
        LOGE("%s::unmathced v4l2_output_type(%d)", __func__, v4l2_output_type);
        outputMode = -1;
        break;
    }

    return outputMode;
}

int composite_std_2_v4l2_std_id(int std)
{
    int std_id = -1;

    switch (std) {
    case COMPOSITE_STD_NTSC_M:
        std_id = V4L2_STD_NTSC_M;
        break;
    case COMPOSITE_STD_NTSC_443:
        std_id = V4L2_STD_NTSC_443;
        break;
    case COMPOSITE_STD_PAL_BDGHI:
        std_id = V4L2_STD_PAL_BDGHI;
        break;
    case COMPOSITE_STD_PAL_M:
        std_id = V4L2_STD_PAL_M;
        break;
    case COMPOSITE_STD_PAL_N:
        std_id = V4L2_STD_PAL_N;
        break;
    case COMPOSITE_STD_PAL_Nc:
        std_id = V4L2_STD_PAL_Nc;
        break;
    case COMPOSITE_STD_PAL_60:
        std_id = V4L2_STD_PAL_60;
        break;
    default:
        LOGE("%s::unmathced composite_std(%d)", __func__, std);
        break;
    }

    return std_id;
}

int hdmi_check_output_mode(int v4l2_output_type)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;
    int    calbirate_v4l2_mode = v4l2_output_type;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL :
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            LOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_YCBCR444;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
            LOGI("Change mode into HDMI_RGB\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            LOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_RGB;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
            LOGI("Change mode into HDMI_YCBCR\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_DVI:
        video.mode = DVI;
        if (!EDIDHDMIModeSupport(&video)) {
            video.colorSpace = HDMI_CS_YCBCR444;
            if (!EDIDColorSpaceSupport(&video)) {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
                LOGI("Change mode into HDMI_RGB\n");
            } else {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
                LOGI("Change mode into HDMI_YCBCR\n");
            }
            break;
        }

        break;

    default:
        break;
    }
    return calbirate_v4l2_mode;
}

int hdmi_check_resolution(v4l2_std_id std_id)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    switch (std_id) {
    case V4L2_STD_480P_60_16_9:
        video.resolution = v720x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_480P_60_4_3:
        video.resolution = v640x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_576P_50_16_9:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_576P_50_4_3:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_60:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_50:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_60:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_50:
        video.resolution = v1920x1080p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_60:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_50:
        video.resolution = v1920x1080i_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_480P_59:
        video.resolution = v720x480p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_720P_59:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080I_59:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_59:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_1080P_30:
        video.resolution = v1920x1080p_30Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_60_SBS_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_59_SBS_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_STD_TVOUT_720P_50_TB:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_STD_TVOUT_1080P_24_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_STD_TVOUT_1080P_23_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    default:
        LOGE("%s::unmathced std_id(%lld)", __func__, std_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video)) {
        LOG_LIB_HDMI_V4L2("%s::EDIDVideoResolutionSupport(%llx) fail (not suppoted std_id) \n", __func__, std_id);
        return -1;
    }

    return 0;
}

int hdmi_resolution_2_std_id(unsigned int resolution, unsigned int s3dMode, int * w, int * h, v4l2_std_id * std_id)
{
    int ret = 0;

    if (s3dMode == HDMI_2D) {
        switch (resolution) {
        case 1080960:
            *std_id = V4L2_STD_1080P_60;
            *w      = 1920;
            *h      = 1080;
            break;
        case 1080950:
            *std_id = V4L2_STD_1080P_50;
            *w      = 1920;
            *h      = 1080;
            break;
        case 1080930:
            *std_id = V4L2_STD_1080P_30;
            *w      = 1920;
            *h      = 1080;
            break;
        case 1080160:
            *std_id = V4L2_STD_1080I_60;
            *w      = 1920;
            *h      = 1080;
            break;
        case 1080150:
            *std_id = V4L2_STD_1080I_50;
            *w      = 1920;
            *h      = 1080;
            break;
        case 720960:
            *std_id = V4L2_STD_720P_60;
            *w      = 1280;
            *h      = 720;
            break;
        case 720950:
            *std_id = V4L2_STD_720P_50;
            *w      = 1280;
            *h      = 720;
            break;
        case 5769501:
            *std_id = V4L2_STD_576P_50_16_9;
            *w      = 720;
            *h      = 576;
            break;
        case 5769502:
            *std_id = V4L2_STD_576P_50_4_3;
            *w      = 720;
            *h      = 576;
            break;
        case 4809601:
            *std_id = V4L2_STD_480P_60_16_9;
            *w      = 720;
            *h      = 480;
            break;
        case 4809602:
            *std_id = V4L2_STD_480P_60_4_3;
            *w     = 720;
            *h     = 480;
            break;
        default:
            LOGE("%s::unmathced resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    } else if (s3dMode == HDMI_S3D_TB) {
        switch (resolution) {
        case 1080924:
            *std_id = V4L2_STD_TVOUT_1080P_24_TB;
            *w      = 1920;
            *h      = 1080;
            break;
        case 720950:
            *std_id = V4L2_STD_TVOUT_720P_50_TB;
            *w      = 1280;
            *h      = 720;
            break;
        default:
            LOGE("%s::unmathced S3D TB resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    } else {
        switch (resolution) {
        case 720960:
            *std_id = V4L2_STD_TVOUT_720P_60_SBS_HALF;
            *w      = 1280;
            *h      = 720;
            break;
        default:
            LOGE("%s::unmathced S3D SBS resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    }

    return ret;
}

int hdmi_enable_hdcp(unsigned int hdcp_en)
{
    if (ioctl(fp_tvout, VIDIOC_HDCP_ENABLE, hdcp_en) < 0) {
        LOGD("%s::VIDIOC_HDCP_ENABLE(%d) fail \n", __func__, hdcp_en);
        return -1;
    }

    return 0;
}

int hdmi_check_audio(void)
{
    struct HDMIAudioParameter audio;
    enum state audio_state = ON;
    int ret = 0;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

#if defined(BOARD_USES_EDID)
    if (!EDIDAudioModeSupport(&audio))
        audio_state = NOT_SUPPORT;
    else
        audio_state = ON;
#endif
    if (audio_state == ON) {
        if (ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 1) < 0) {
            LOGE("%s::VIDIOC_INIT_AUDIO(1) fail", __func__);
            ret = -1;
        }
    } else {
        if (ioctl(fp_tvout, VIDIOC_INIT_AUDIO, 0) < 0) {
            LOGE("%s::VIDIOC_INIT_AUDIO(0) fail", __func__);
            ret = -1;
        }
    }

    return ret;
}

}
