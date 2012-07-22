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
//#define DEBUG_HDMI_HW_LEVEL
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

#include "sec_utils_v4l2.h"
#include "s5p_tvout_v4l2.h"

#include "videodev2.h"

#if defined(BOARD_USES_HDMI_FIMGAPI)
#include "sec_g2d_4x.h"
#include "FimgApi.h"
#endif

#include "audio.h"
#include "video.h"
#include "../libhdmi/libsForhdmi/libedid/libedid.h"
#include "../libhdmi/libsForhdmi/libcec/libcec.h"

#include "SecGscaler.h"
#include "SecHdmiCommon.h"
#include "SecHdmiV4L2Utils.h"

namespace android {

unsigned int output_type  = V4L2_OUTPUT_TYPE_DIGITAL;
v4l2_std_id  t_std_id     = V4L2_STD_1080P_30;
int          g_hpd_state  = HPD_CABLE_OUT;
unsigned int g_hdcp_en    = 0;

#if defined(BOARD_USES_HDMI_FIMGAPI)
unsigned int g2d_reserved_memory0     = 0;
unsigned int g2d_reserved_memory1     = 0;
unsigned int g2d_reserved_memory_size = 0;
unsigned int cur_g2d_address          = 0;
#endif

void display_menu(void)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    ALOGI("=============== HDMI Audio  =============\n");

    if (EDIDAudioModeSupport(&audio))
        ALOGI("=  2CH_PCM 44100Hz audio supported      =\n");

    ALOGI("========= HDMI Mode & Color Space =======\n");

    video.mode = HDMI;
    if (EDIDHDMIModeSupport(&video)) {
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  1. HDMI(YCbCr)                       =\n");

        video.colorSpace = HDMI_CS_RGB;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  2. HDMI(RGB)                         =\n");
    } else {
        video.mode = DVI;
        if (EDIDHDMIModeSupport(&video))
            ALOGI("=  3. DVI                               =\n");
    }

    ALOGI("===========    HDMI Rseolution   ========\n");

    /* 480P */
    video.resolution = v720x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  4. 480P_60_16_9    (0x04000000)    =\n");

    video.resolution = v640x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  5. 480P_60_4_3 (0x05000000)    =\n");

    /* 576P */
    video.resolution = v720x576p_50Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  6. 576P_50_16_9    (0x06000000)    =\n");

    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  7. 576P_50_4_3 (0x07000000)    =\n");

    /* 720P 60 */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  8. 720P_60         (0x08000000)    =\n");

    /* 720P_50 */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  9. 720P_50         (0x09000000)    =\n");

    /* 1080P_60 */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  a. 1080P_60        (0x0a000000)    =\n");

    /* 1080P_50 */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  b. 1080P_50        (0x0b000000)    =\n");

    /* 1080I_60 */
    video.resolution = v1920x1080i_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  c. 1080I_60        (0x0c000000)    =\n");

    /* 1080I_50 */
    video.resolution = v1920x1080i_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  d. 1080I_50        (0x0d000000)    =\n");

    /* 1080P_30 */
    video.resolution = v1920x1080p_30Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  e. 1080P_30        (0x12000000)    =\n");

    ALOGI("===========    HDMI 3D Format   ========\n");

    /* 720P_60_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  f. 720P_60_SBS_HALF    (0x13000000)    =\n");

    /* 720P_59_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  10. 720P_59_SBS_HALF    (0x14000000)    =\n");

    /* 720P_50_TB */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  11. 720P_50_TB          (0x15000000)    =\n");

    /* 1080P_24_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  12. 1080P_24_TB          (0x16000000)    =\n");

    /* 1080P_23_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  13. 1080P_24_TB          (0x17000000)    =\n");
    ALOGI("=========================================\n");
}

int tvout_init(int fd_tvout, __u32 preset_id)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::preset_id = 0x%x", __func__, preset_id);
#endif

    int ret;
    struct v4l2_output output;
    struct v4l2_dv_preset preset;

    unsigned int matched = 0, i = 0;
    int output_index;

    if (fd_tvout <= 0) {
        fd_tvout = open(TVOUT0_DEV_G0, O_RDWR);
        if (fd_tvout < 0) {
            ALOGE("%s::fd_tvout open failed", __func__);
            return -1;
        }
    }
/*
    if (output_type >= V4L2_OUTPUT_TYPE_DIGITAL &&
        output_type <= V4L2_OUTPUT_TYPE_DVI)
        if (ioctl(fd_tvout, VIDIOC_HDCP_ENABLE, g_hdcp_en) < 0)
            ALOGE("%s::VIDIOC_HDCP_ENABLE failed %d", __func__, errno);
*/
    i = 0;

    do {
        output.index = i;
        ret = tvout_v4l2_enum_output(fd_tvout, &output);
        ALOGV("%s::output_type=%d output.index=%d .name=%s", __func__, output_type, output.index, output.name);
        if (output.type == output_type) {
            matched = 1;
            break;
        }
        i++;
    } while (ret >=0);
/*
    if (!matched) {
        ALOGE("%s::no matched output type [type=0x%08x]", __func__, output_type);
        return -1;
    }

    tvout_v4l2_s_output(fd_tvout, output.index);
    output_index = 0;
    tvout_v4l2_g_output(fd_tvout, &output_index);
*/

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::input preset_id=0x%08x", __func__, preset_id);
#endif

    if (output.capabilities & V4L2_OUT_CAP_PRESETS) {
        tvout_std_v4l2_enum_dv_presets(fd_tvout);
        preset.preset = preset_id;
        if (tvout_std_v4l2_s_dv_preset(fd_tvout, &preset) < 0 ) {
            ALOGE("%s::tvout_std_v4l2_s_dv_preset failed", __func__);
            return -1;
        }
    }

    return fd_tvout;
}

int tvout_deinit()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    return 0;
}

int tvout_std_v4l2_querycap(int fd, char *node)
{
#ifdef DEBUG_HDMI_HW_LEVEL
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

int tvout_std_v4l2_s_fmt(int fd, enum v4l2_buf_type type, enum v4l2_field field, int w, int h, int colorformat, int num_planes)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    struct v4l2_format fmt;

    fmt.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_G_FMT failed", __func__);
        return -1;
    }

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        fmt.fmt.pix.width       = w;
        fmt.fmt.pix.height      = h;
        fmt.fmt.pix.pixelformat = colorformat;
        fmt.fmt.pix.field       = field;
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        fmt.fmt.pix_mp.width       = w;
        fmt.fmt.pix_mp.height      = h;
        fmt.fmt.pix_mp.pixelformat = colorformat;
        fmt.fmt.pix_mp.field       = field;
        fmt.fmt.pix_mp.num_planes  = num_planes;
        break;
    default:
        ALOGE("%s::invalid buffer type", __func__);
        return -1;
        break;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_S_FMT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_s_crop(int fd, enum v4l2_buf_type type, enum v4l2_field, int x, int y, int w, int h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_crop crop;

    crop.type     = type;
    crop.c.left   = x;
    crop.c.top    = y;
    crop.c.width  = w;
    crop.c.height = h;

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_S_CROP (x=%d, y=%d, w=%d, h=%d) failed",
            __func__, x, y, w, h);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_s_ctrl(int fd, int id, int value)
{
#ifdef DEBUG_HDMI_HW_LEVEL
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

int tvout_std_v4l2_reqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int num_bufs)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_requestbuffers reqbuf;

    reqbuf.type   = type;
    reqbuf.memory = memory;
    reqbuf.count  = num_bufs;

    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        ALOGE("%s::VIDIOC_REQBUFS failed", __func__);
        return -1;
    }

    if (reqbuf.count < num_bufs) {
        ALOGE("%s::VIDIOC_REQBUFS failed ((reqbuf.count(%d) < num_bufs(%d))",
            __func__, reqbuf.count, num_bufs);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_querybuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, unsigned int buf_index, unsigned int num_planes, SecBuffer *secBuf)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_MIXER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    if (MAX_BUFFERS_MIXER <= buf_index || MAX_PLANES_MIXER <= num_planes) {
        ALOGE("%s::exceed MAX! : buf_index=%d, num_plane=%d", __func__, buf_index, num_planes);
        return -1;
    }

    buf.type     = type;
    buf.memory   = V4L2_MEMORY_MMAP;
    buf.index    = buf_index;
    buf.length   = num_planes;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_QUERYBUF failed, plane_cnt=%d", __func__, buf.length);
        return -1;
    }

    for (unsigned int i = 0; i < num_planes; i++) {
        if ((secBuf->virt.extP[i] = (char *)mmap(0, buf.m.planes[i].length,
                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[i].m.mem_offset)) < 0) {
            ALOGE("%s::mmap failed", __func__);
            ALOGE("%s::Offset = 0x%x", __func__, buf.m.planes[i].m.mem_offset);
            ALOGE("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
            ALOGE("%s::vaddr[%d][%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
            return -1;
        }
        secBuf->size.extS[i] = buf.m.planes[i].length;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::vaddr[bufindex=%d][planeidx=%d] = 0x%x", __func__, buf_index, i, (unsigned int)secBuf->virt.extP[i]);
    ALOGD("%s::Legnth = %d"  , __func__, buf.m.planes[i].length);
#endif
    }

    return 0;
}

int tvout_std_v4l2_qbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int buf_index, int num_planes, SecBuffer *secBuf)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_MIXER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_planes;
    buf.index    = buf_index;
    buf.m.planes = planes;

    for (unsigned int i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)secBuf->virt.extP[i];
        buf.m.planes[i].length    = secBuf->size.extS[i];
        buf.m.planes[i].bytesused = buf.m.planes[i].length;

#ifdef DEBUG_HDMI_HW_LEVEL
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

int tvout_std_v4l2_dqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory, int *buf_index, int num_planes)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[MAX_PLANES_MIXER];

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    for (int i = 0; i < MAX_PLANES_GSCALER; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));


    buf.type     = type;
    buf.memory   = memory;
    buf.length   = num_planes;
    buf.m.planes = planes;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        ALOGE("%s::VIDIOC_DQBUF failed", __func__);
        return -1;
    }
    *buf_index = buf.index;

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::buf.index=%d", __func__, buf.index);
#endif

    return 0;
}

int tvout_std_v4l2_streamon(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMON failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_streamoff(int fd, enum v4l2_buf_type type)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("%s::VIDIOC_STREAMOFF failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_v4l2_enum_output(int fd, struct v4l2_output *output)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    int ret = -1 ;
    ret = ioctl(fd, VIDIOC_ENUMOUTPUT, output);

    if (ret < 0) {
        if (errno == EINVAL)
            return -1;
        ALOGE("%s::VIDIOC_ENUMOUTPUT", __func__);
        return -1;
    }
    ALOGD("%s::index=%d, type=0x%08x, name=%s",
          __func__, output->index, output->type, output->name);

    return ret;
}

int tvout_v4l2_s_output(int fd, int index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_S_OUTPUT, &index) < 0) {
        ALOGE("%s::VIDIOC_S_OUTPUT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_v4l2_g_output(int fd, int *index)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_G_OUTPUT, index) < 0) {
        ALOGE("%s::VIDIOC_G_OUTPUT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_enum_dv_presets(int fd)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_dv_enum_preset enum_preset;
    int ret = -1;

    for (int index = 0; ; index++) {
        enum_preset.index = index;
        ret = ioctl(fd, VIDIOC_ENUM_DV_PRESETS, &enum_preset);

        if (ret < 0) {
            if (errno == EINVAL)
                break;
            ALOGE("%s::VIDIOC_ENUM_DV_PRESETS", __func__);
            return -1;
        }
        ALOGD("%s::index=%d, preset=0x%08x, name=%s, w=%d, h=%d",
              __func__, enum_preset.index, enum_preset.preset, enum_preset.name, enum_preset.width, enum_preset.height);
    }

    return 0;
}

int tvout_std_v4l2_s_dv_preset(int fd, struct v4l2_dv_preset *preset)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (ioctl(fd, VIDIOC_S_DV_PRESET, preset) < 0) {
        ALOGE("%s::VIDIOC_S_DV_PRESET failed preset_id=%d", __func__, preset->preset);
        return -1;
    }
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::preset_id=%d", __func__, preset->preset);
#endif
    return 0;
}

int tvout_std_subdev_s_fmt(int fd, unsigned int pad, int w, int h, enum v4l2_mbus_pixelcode code)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_subdev_format fmt;

    fmt.pad   = pad;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.width  = w;
    fmt.format.height = h;
    fmt.format.code   = code;

    if (ioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_FMT", __func__);
        return -1;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::format w=%d, h=%d", __func__, fmt.format.width, fmt.format.height);
#endif
    return 0;
}
int tvout_std_subdev_s_crop(int fd, unsigned int pad, int x, int y, int w, int h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::pad=%d, crop x=%d, y=%d, w=%d, h=%d", __func__, pad, x, y, w, h);
#endif

    struct v4l2_subdev_crop   crop;

    crop.pad   = pad;
    crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    crop.rect.left   = x;
    crop.rect.top    = y;
    crop.rect.width  = w;
    crop.rect.height = h;

    if (ioctl(fd, VIDIOC_SUBDEV_S_CROP, &crop) < 0) {
        ALOGE("%s::VIDIOC_SUBDEV_S_CROP", __func__);
        return -1;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s::pad=%d, crop x=%d, y=%d, w=%d, h=%d", __func__, pad, crop.rect.left, crop.rect.top, crop.rect.width, crop.rect.height);
#endif

    return 0;
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

int hdmi_set_videolayer(int fd, int hdmiW, int hdmiH, struct v4l2_rect * rect)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    if (tvout_std_subdev_s_fmt(fd, MIXER_V_SUBDEV_PAD_SINK, hdmiW, hdmiH, V4L2_MBUS_FMT_YUV8_1X24) < 0) {
        ALOGE("%s::tvout_std_subdev_s_fmt(PAD=%d)[videolayer] failed", __func__, MIXER_V_SUBDEV_PAD_SINK);
        return -1;
    }

    if (tvout_std_subdev_s_crop(fd, MIXER_V_SUBDEV_PAD_SINK, 0, 0, rect->width, rect->height) < 0) {
        ALOGE("%s::tvout_std_subdev_s_crop(PAD=%d)[videolayer] failed", __func__, MIXER_V_SUBDEV_PAD_SINK);
        return -1;
    }

    if (tvout_std_subdev_s_crop(fd, MIXER_V_SUBDEV_PAD_SOURCE, rect->left, rect->top, rect->width, rect->height) < 0) {
        ALOGE("%s::tvout_std_subdev_s_crop(PAD=%d)[videolayer] failed", __func__, MIXER_V_SUBDEV_PAD_SOURCE);
        return -1;
    }
    return 0;
}

int hdmi_set_graphiclayer(int fd_subdev, int fd_videodev,int layer,
        int srcColorFormat,
        int src_w, int src_h,
        unsigned int src_address, SecBuffer * dstBuffer,
        int dst_x, int dst_y, int dst_w, int dst_h,
        int rotVal)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
#if defined(BOARD_USES_HDMI_FIMGAPI)
    int             dst_color_format;
    int             dst_bpp;
    unsigned char   *dst_addr;
    fimg2d_blit     BlitParam;
    rotation        g2d_rotation;

    fimg2d_addr  srcAddr;
    fimg2d_image srcImage;
    fimg2d_rect  srcRect;

    fimg2d_addr  dstAddr;
    fimg2d_image dstImage;
    fimg2d_rect  dstRect;

    fimg2d_clip  dstClip;
    fimg2d_scale Scaling;

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
        break;
    case V4L2_STD_480P_60_16_9:
    case V4L2_STD_576P_50_16_9:
    case V4L2_STD_720P_60:
    case V4L2_STD_TVOUT_720P_50_TB:
    default:
        dst_color_format = CF_ARGB_4444;
        dst_bpp = 2;
        break;
    }

    static unsigned int prev_src_addr = 0;

    if ((cur_g2d_address == 0) || (src_address != prev_src_addr)) {
        if ((cur_g2d_address == 0) || (cur_g2d_address == g2d_reserved_memory1))
            dst_addr = (unsigned char *)g2d_reserved_memory0;
        else
            dst_addr = (unsigned char *)g2d_reserved_memory1;

        cur_g2d_address = (unsigned int)dst_addr;
        prev_src_addr = src_address;

        srcAddr  = {(addr_space)ADDR_USER, (unsigned long)src_address, src_w*src_h*4, 1, 0};
        srcImage = {srcAddr, srcAddr, src_w, src_h, src_w*4, AX_RGB, CF_ARGB_8888};
        srcRect = {0, 0, src_w, src_h};

        dstAddr  = {(addr_space)ADDR_USER, (unsigned long)dst_addr, dst_w*dst_h*dst_bpp, 1, 0};
        dstImage = {dstAddr, dstAddr, dst_w, dst_h, dst_w*dst_bpp, AX_RGB, (color_format)dst_color_format};
        dstRect  = {0, 0, dst_w, dst_h};
        dstClip  = {0, 0, 0, dst_w, dst_h};

        if (rotVal == 0 || rotVal == 180)
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_w, dst_h};
        else
            Scaling = {SCALING_BILINEAR, SCALING_PIXELS, 0, 0, src_w, src_h, dst_h, dst_w};

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
            ALOGE("%s::invalid rotVal(%d) : failed", __func__, rotVal);
            return -1;
            break;
        }

        BlitParam = {BLIT_OP_SRC, NON_PREMULTIPLIED, 0xff, 0, g2d_rotation, &Scaling, 0, 0, &dstClip, 0, &srcImage, &dstImage, NULL, &srcRect, &dstRect, NULL, 0};

        if (stretchFimgApi(&BlitParam) < 0) {
            ALOGE("%s::stretchFimgApi() failed", __func__);
            return -1;
        }

        dstBuffer->virt.p = (char *)dst_addr;
    }
#else
    dstBuffer->virt.p = (char *)src_address;
#endif

    return 0;
}

int hdmi_set_g_Params(int fd_subdev, int fd_videodev, int layer,
                      int srcColorFormat,
                      int src_w, int src_h,
                      int dst_x, int dst_y, int dst_w, int dst_h)
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif
    struct v4l2_rect rect;
    int src_pad = 0;
    int sink_pad = 0;
    int v4l2ColorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(srcColorFormat);

    switch (layer) {
    case HDMI_LAYER_GRAPHIC_0:
        sink_pad = MIXER_G0_SUBDEV_PAD_SINK;
        src_pad  = MIXER_G0_SUBDEV_PAD_SOURCE;
        break;
    case HDMI_LAYER_GRAPHIC_1:
        sink_pad = MIXER_G1_SUBDEV_PAD_SINK;
        src_pad  = MIXER_G1_SUBDEV_PAD_SOURCE;
        break;
    default:
        ALOGE("%s::invalid layer(%d)", __func__, layer);
        break;
    };

    rect.left   = dst_x;
    rect.top    = dst_y;

#if defined(BOARD_USES_HDMI_FIMGAPI)
    rect.width  = dst_w;
    rect.height = dst_h;
#else
    rect.width  = src_w;
    rect.height = src_h;
#endif

    /* set sub device for mixer graphic layer input */
    if (tvout_std_subdev_s_fmt(fd_subdev, sink_pad, rect.width, rect.height, V4L2_MBUS_FMT_XRGB8888_4X8_LE) < 0) {
        ALOGE("%s::tvout_std_subdev_s_fmt(PAD=%d)[graphic layer] failed", __func__, sink_pad);
        return -1;
    }

    if (tvout_std_subdev_s_crop(fd_subdev, sink_pad, 0, 0, rect.width, rect.height) < 0) {
        ALOGE("%s::tvout_std_subdev_s_crop(PAD=%d)[graphic layer] failed", __func__, sink_pad);
        return -1;
    }

    if (tvout_std_subdev_s_crop(fd_subdev, src_pad, rect.left, rect.top, rect.width, rect.height) < 0) {
        ALOGE("%s::tvout_std_subdev_s_crop(PAD=%d)[graphic layer] failed", __func__, src_pad);
        return -1;
    }

    /* set format for mixer graphic layer input device*/
    if (tvout_std_v4l2_s_fmt(fd_videodev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, rect.width, rect.height, v4l2ColorFormat, 1) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_fmt()[graphic layer] failed", __func__);
        return -1;
    }

    if (tvout_std_v4l2_s_crop(fd_videodev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_ANY, rect.left, rect.top, rect.width, rect.height) < 0) {
        ALOGE("%s::tvout_std_v4l2_s_crop()[graphic layer] failed", __func__);
        return -1;
    }

    /* request buffer for mixer graphic layer input device */
    if (tvout_std_v4l2_reqbuf(fd_videodev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_USERPTR, 2) < 0) {
        ALOGE("%s::tvout_std_v4l2_reqbuf(buf_num=%d)[graphic layer] failed", __func__, 2);
        return -1;
    }

    return 0;
}

int hdmi_cable_status()
{
#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("%s", __func__);
#endif

    int cable_status = 0;
    int fd = 0;
    struct v4l2_control ctrl;

    fd = open(TVOUT0_DEV_G0, O_RDWR);
    if (fd <= 0) {
        ALOGE("%s: graphic layer 0 drv open failed", __func__);
        return -1;
    }

    ctrl.id = V4L2_CID_TV_HPD_STATUS;

    if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        ALOGE("Get HPD_STATUS fail");
        cable_status = -1;
    } else {
        cable_status = ctrl.value;
    }

#ifdef DEBUG_HDMI_HW_LEVEL
    ALOGD("HPD_STATUS = %d", cable_status);
#endif

    close(fd);

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
        ALOGE("%s::unmathced HDMI_mode(%d)", __func__, output_mode);
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
        ALOGE("%s::unmathced v4l2_output_type(%d)", __func__, v4l2_output_type);
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
        ALOGE("%s::unmathced composite_std(%d)", __func__, std);
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
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_YCBCR444;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
            ALOGI("Change mode into HDMI_RGB\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_RGB;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
            ALOGI("Change mode into HDMI_YCBCR\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_DVI:
        video.mode = DVI;
        if (!EDIDHDMIModeSupport(&video)) {
            video.colorSpace = HDMI_CS_YCBCR444;
            if (!EDIDColorSpaceSupport(&video)) {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
                ALOGI("Change mode into HDMI_RGB\n");
            } else {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
                ALOGI("Change mode into HDMI_YCBCR\n");
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
        ALOGE("%s::unmathced std_id(%lld)", __func__, std_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video)) {
#ifdef DEBUG_MSG_ENABLE
        ALOGD("%s::EDIDVideoResolutionSupport(%llx) fail (not suppoted std_id) \n", __func__, std_id);
#endif
        return -1;
    }

    return 0;
}

int hdmi_resolution_2_std_id(unsigned int resolution, int * w, int * h, v4l2_std_id * std_id, __u32 *preset_id)
{
    int ret = 0;

    switch (resolution) {
    case 1080960:
        *std_id = V4L2_STD_1080P_60;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P60;
        break;
    case 1080950:
        *std_id = V4L2_STD_1080P_50;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P50;
        break;
    case 1080930:
        *std_id = V4L2_STD_1080P_30;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P30;
        break;
    case 1080924:
        *std_id = V4L2_STD_TVOUT_1080P_24_TB;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080P24_TB;
        break;
    case 1080160:
        *std_id = V4L2_STD_1080I_60;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080I60;
        break;
    case 1080150:
        *std_id = V4L2_STD_1080I_50;
        *w      = 1920;
        *h      = 1080;
        *preset_id = V4L2_DV_1080I50;
        break;
    case 720960:
        *std_id = V4L2_STD_720P_60;
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P60;
        break;
    case 7209601:
        *std_id = V4L2_STD_TVOUT_720P_60_SBS_HALF;
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P60_SB_HALF;
        break;
    case 720950:
        *std_id = V4L2_STD_720P_50;
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P50;
        break;
    case 7209501:
        *std_id = V4L2_STD_TVOUT_720P_50_TB;
        *w      = 1280;
        *h      = 720;
        *preset_id = V4L2_DV_720P50_TB;
        break;
    case 5769501:
        *std_id = V4L2_STD_576P_50_16_9;
        *w      = 720;
        *h      = 576;
        *preset_id = V4L2_DV_576P50;
        break;
    case 5769502:
        *std_id = V4L2_STD_576P_50_4_3;
        *w      = 720;
        *h      = 576;
        *preset_id = V4L2_DV_576P50;
       break;
    case 4809601:
        *std_id = V4L2_STD_480P_60_16_9;
        *w      = 720;
        *h      = 480;
        *preset_id = V4L2_DV_480P60;
       break;
    case 4809602:
        *std_id = V4L2_STD_480P_60_4_3;
        *w     = 720;
        *h     = 480;
        *preset_id = V4L2_DV_480P60;
      break;
    default:
        ALOGE("%s::unmathced resolution(%d)", __func__, resolution);
        ret = -1;
        break;
    }

    return ret;
}

int hdmi_enable_hdcp(int fd, unsigned int hdcp_en)
{
    if (ioctl(fd, VIDIOC_HDCP_ENABLE, hdcp_en) < 0) {
        ALOGD("%s::VIDIOC_HDCP_ENABLE(%d) fail \n", __func__, hdcp_en);
        return -1;
    }

    return 0;
}

int hdmi_check_audio(int fd)
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
        if (ioctl(fd, VIDIOC_INIT_AUDIO, 1) < 0) {
            ALOGE("%s::VIDIOC_INIT_AUDIO(1) failed", __func__);
            ret = -1;
        }
    } else {
        if (ioctl(fd, VIDIOC_INIT_AUDIO, 0) < 0) {
            ALOGE("%s::VIDIOC_INIT_AUDIO(0) failed", __func__);
            ret = -1;
        }
    }

    return ret;
}

}
