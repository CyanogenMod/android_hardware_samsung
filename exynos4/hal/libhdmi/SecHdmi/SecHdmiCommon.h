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
**
** @author Sangwoo, Park(sw5771.park@samsung.com)
** @date   2010-09-10
**
*/

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>

#include "videodev2.h"
#if defined(BOARD_USE_V4L2)
#include "s5p_tvout_v4l2.h"
#else
#include "s5p_tvout.h"
#endif

//#define DEBUG_MSG_ENABLE
//#define DEBUG_HDMI_HW_LEVEL
#define BOARD_USES_EDID
//#define BOARD_USES_CEC
#if defined(SAMSUNG_EXYNOS4x12)
//#define SUPPORT_G2D_UI_MODE
#endif

#define DEFAULT_FB      (0)
#define TVOUT_FB_G0     (10)
#define TVOUT_FB_G1     (11)

#define MAX_BUFFERS_MIXER           (1)
#define MAX_PLANES_MIXER            (3)

#define HDMI_NUM_MIXER_BUF          (2)
#define GRALLOC_BUF_SIZE            (32768)
#define SIZE_1K                     (1024)

#define HDMI_FIMC_OUTPUT_BUF_NUM    (4)
#define HDMI_G2D_OUTPUT_BUF_NUM     (2)
#define HDMI_FIMC_BUFFER_BPP_SIZE   (1.5)   //NV12 Tiled is 1.5 bytes, RGB565 is 2, RGB888 is 4, Default is NV12 Tiled
#define HDMI_G2D_BUFFER_BPP_SIZE    (4)     //NV12 Tiled is 1.5 bytes, RGB565 is 2, RGB888 is 4
#define HDMI_FB_BPP_SIZE            (4)     //ARGB888 is 4
#define SUPPORT_1080P_FIMC_OUT
#define HDMI_MAX_WIDTH              (1920)
#define HDMI_MAX_HEIGHT             (1080)

#define ALIGN(x, a)    (((x) + (a) - 1) & ~((a) - 1))

#if defined(STD_NTSC_M)
    #define DEFAULT_OUPUT_MODE            (COMPOSITE_OUTPUT_MODE)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (1080960) // 1080P_60
    #define DEFAULT_HDMI_PRESET_ID        (V4L2_DV_1080P60)
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_1080P_60)
    #define DEFALULT_DISPLAY_WIDTH            (720)
    #define DEFALULT_DISPLAY_HEIGHT            (480)
    #define DEFAULT_COMPOSITE_STD         (COMPOSITE_STD_NTSC_M)
#elif (STD_1080P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_RGB)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (1080960) // 1080P_60
    #define DEFAULT_HDMI_PRESET_ID        (V4L2_DV_1080P60)
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_1080P_60)
    #define DEFALULT_DISPLAY_WIDTH            (1920)
    #define DEFALULT_DISPLAY_HEIGHT            (1080)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#elif defined(STD_720P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (720960) // 720P_60
    #define DEFAULT_HDMI_PRESET_ID        (V4L2_DV_720P60)
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_720P_60)
    #define DEFALULT_DISPLAY_WIDTH            (1280)
    #define DEFALULT_DISPLAY_HEIGHT            (720)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#elif defined(STD_480P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (4809601) // 480P_60_4_3
    #define DEFAULT_HDMI_PRESET_ID        (V4L2_DV_480P60)
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_480P_60_16_9)
    #define DEFALULT_DISPLAY_WIDTH            (720)
    #define DEFALULT_DISPLAY_HEIGHT            (480)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#else
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE (4809602) // 480P_60_4_3
    #define DEFAULT_HDMI_PRESET_ID        (V4L2_DV_480P60)
    #define DEFAULT_HDMI_STD_ID           (V4L2_STD_480P_60_4_3)
    #define DEFALULT_DISPLAY_WIDTH            (720)
    #define DEFALULT_DISPLAY_HEIGHT            (480)
    #define DEFAULT_COMPOSITE_STD      (COMPOSITE_STD_NTSC_M)
#endif

enum hdp_cable_status {
    HPD_CABLE_OUT = 0, // HPD_CABLE_OUT indicates HDMI cable out.
    HPD_CABLE_IN       // HPD_CABLE_IN indicates HDMI cable in.
};

enum state {
    OFF = 0,
    ON = 1,
    NOT_SUPPORT = 2,
};

enum tv_mode {
    HDMI_OUTPUT_MODE_YCBCR = 0,
    HDMI_OUTPUT_MODE_RGB = 1,
    HDMI_OUTPUT_MODE_DVI = 2,
    COMPOSITE_OUTPUT_MODE = 3
};

enum composite_std {
    COMPOSITE_STD_NTSC_M = 0,
    COMPOSITE_STD_PAL_BDGHI = 1,
    COMPOSITE_STD_PAL_M = 2,
    COMPOSITE_STD_PAL_N = 3,
    COMPOSITE_STD_PAL_Nc = 4,
    COMPOSITE_STD_PAL_60 = 5,
    COMPOSITE_STD_NTSC_443 = 6
};

enum hdmi_layer {
    HDMI_LAYER_BASE   = 0,
    HDMI_LAYER_VIDEO,
    HDMI_LAYER_GRAPHIC_0,
    HDMI_LAYER_GRAPHIC_1,
    HDMI_LAYER_MAX,
};
