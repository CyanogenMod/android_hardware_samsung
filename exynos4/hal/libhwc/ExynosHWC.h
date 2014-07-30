/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_EXYNOS_HWC_H_
#define ANDROID_EXYNOS_HWC_H_
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <s3c-fb.h>

#include <EGL/egl.h>

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <cutils/compiler.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/gralloc.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware_legacy/uevent.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include <sync/sync.h>

#include "ion.h"
#include "gralloc_priv.h"
#include "exynos_fimc.h"
#include "exynos_format.h"
#include "exynos_v4l2.h"
#include "s5p_tvout_v4l2.h"
//#include "ExynosHWCModule.h"
#include "ExynosRect.h"
#include "videodev2.h"

#define VSYNC_DEV_NAME "/sys/devices/platform/samsung-pd.2/s3cfb.0/graphics/fb0/vsync_event"
#define GRALLOC_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

#ifdef USE_FB_PHY_LINEAR
const size_t NUM_HW_WIN_FB_PHY = 2;
#undef DUAL_VIDEO_OVERLAY_SUPORT
#endif
const size_t NUM_HW_WINDOWS = 5;
const size_t NO_FB_NEEDED = NUM_HW_WINDOWS + 1;
const size_t MAX_PIXELS = 480 * 800 * 2;
const size_t FIMC_W_ALIGNMENT = 16;
const size_t FIMC_H_ALIGNMENT = 16;
const size_t FIMC_DST_H_ALIGNMENT_RGB888 = 1;
const size_t FIMC_DST_W_ALIGNMENT_RGB888 = 32;
const size_t FIMC_DST_CROP_W_ALIGNMENT_RGB888 = 32;
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
const size_t FIMD_FIMC_IDX = 0;
const size_t FIMD_FIMC_SEC_IDX = 1;
const size_t FIMD_FIMC_SBS_IDX = 2;
const size_t FIMD_FIMC_TB_IDX = 3;
const size_t FIMD_FIMC_FINAL_INDEX = 3;
const size_t HDMI_FIMC_IDX = 4;
const size_t HDMI_FIMC_SBS_IDX = 5;
const size_t HDMI_FIMC_TB_IDX = 6;
const int FIMD_FIMC_USAGE_IDX[] = {FIMD_FIMC_IDX, FIMD_FIMC_SEC_IDX,
                                                    FIMD_FIMC_SBS_IDX, FIMD_FIMC_TB_IDX};
const int AVAILABLE_FIMC_UNITS[] = { 0, 3, 0, 0, 3, 3, 3 };
#else
const size_t FIMD_FIMC_IDX = 0;
const size_t HDMI_FIMC_IDX = 1;
const size_t FIMD_FIMC_SBS_IDX = 2;
const size_t FIMD_FIMC_TB_IDX = 3;
const size_t HDMI_FIMC_SBS_IDX = 4;
const size_t HDMI_FIMC_TB_IDX = 5;
const int AVAILABLE_FIMC_UNITS[] = { 0, 3, 0, 0, 3, 3 };
#endif
const size_t NUM_FIMC_UNITS = sizeof(AVAILABLE_FIMC_UNITS) /
        sizeof(AVAILABLE_FIMC_UNITS[0]);
const size_t BURSTLEN_BYTES = 16 * 8;
const size_t NUM_HDMI_BUFFERS = 3;
#define DIRECT_FB_SRC_BUF_WA

#ifdef SKIP_STATIC_LAYER_COMP
#define NUM_VIRT_OVER   5
#endif

#ifdef FIMC_VIDEO
#define NUM_VIRT_OVER_HDMI 5
#endif

#ifdef HWC_SERVICES
#include "../libhwcService/ExynosHWCService.h"
namespace android {
class ExynosHWCService;
}
#endif

#define FIMC_SKIP_DUPLICATE_FRAME_PROCESSING

#ifdef HWC_DYNAMIC_RECOMPOSITION
#define HWC_FIMD_BW_TH  1   /* valid range 1 to 5 */
#define HWC_FPS_TH          3    /* valid range 1 to 60 */
#define VSYNC_INTERVAL (1000000000.0 / 60)
typedef enum _COMPOS_MODE_SWITCH {
    NO_MODE_SWITCH,
    HWC_2_GLES = 1,
    GLES_2_HWC,
} HWC_COMPOS_MODE_SWITCH;
#endif

#ifdef USES_WFD
/* This value will be changed to 1080p if needed */
#define EXYNOS4_WFD_DEFAULT_WIDTH       1280
#define EXYNOS4_WFD_DEFAULT_HEIGHT      720
#define EXYNOS4_WFD_FORMAT              HAL_PIXEL_FORMAT_YCbCr_420_SP
#define EXYNOS4_WFD_OUTPUT_ALIGNMENT    16

#endif

struct exynos4_hwc_composer_device_1_t;

#ifdef SUPPORT_FIMC_LOCAL_PATH
#define FIMC_OUT_WA /* sequence change */
#define FORCEFB_YUVLAYER /* video or camera preview */
#define NUM_CONFIG_STABLE   100
#endif
#ifdef FORCEFB_YUVLAYER
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t fw;
    uint32_t fh;
    uint32_t format;
    uint32_t rot;
    uint32_t cacheable;
    uint32_t drmMode;
} video_layer_config;
#endif

struct exynos4_fimc_map_t {
    enum {
        FIMC_NONE = 0,
        FIMC_M2M,
        // TODO: FIMC_LOCAL_PATH
#ifdef SUPPORT_FIMC_LOCAL_PATH
        FIMC_LOCAL,
#endif
    } mode;
    int idx;
};

struct exynos4_hwc_post_data_t {
    int                 overlay_map[NUM_HW_WINDOWS];
    exynos4_fimc_map_t   fimc_map[NUM_HW_WINDOWS];
    size_t              fb_window;
};

const size_t NUM_FIMC_DST_BUFS = 3;
struct exynos4_fimc_data_t {
    void            *fimc;
    exynos_fimc_img  src_cfg;
    exynos_fimc_img  dst_cfg;
    buffer_handle_t dst_buf[NUM_FIMC_DST_BUFS];
    int             dst_buf_fence[NUM_FIMC_DST_BUFS];
    size_t          current_buf;
#ifdef SUPPORT_FIMC_LOCAL_PATH
    int             fimc_mode;
#endif
#ifdef FIMC_SKIP_DUPLICATE_FRAME_PROCESSING
    uint32_t    last_fimc_lay_hnd;
#endif
};

struct hdmi_layer_t {
    int     id;
    int     fd;
    bool    enabled;
    exynos_fimc_img  cfg;

    bool    streaming;
    size_t  current_buf;
    size_t  queued_buf;
};

#if defined(USE_GRALLOC_FLAG_FOR_HDMI) || defined(USES_WFD)
#include "FimgApi.h"
#endif

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
#define HWC_SKIP_HDMI_RENDERING 0x80000000

const size_t NUM_COMPOSITE_BUFFER_FOR_EXTERNAL = 4;
const size_t NUM_BUFFER_U4A = 3;

struct sec_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

struct s3cfb_user_window {
    int x;
    int y;
};

struct s3cfb_extdsp_time_stamp {
    int     y_fd;
    int     uv_fd;
    struct timeval  time_marker;
};

#define EXYNOS4_U4A_FB_DEV              "/dev/graphics/fb1"
#define S3CFB_EXTDSP_PUT_FD             _IOW ('F', 323, struct s3cfb_extdsp_time_stamp)

struct FB_TARGET_Info {
    int32_t         fd;
    unsigned int    mapped_addr;
    int             map_size;
};
#define NUM_FB_TARGET 4
#endif

struct exynos4_hwc_composer_device_1_t {
    hwc_composer_device_1_t base;

    int                     fd;
    int                     vsync_fd;
    exynos4_hwc_post_data_t bufs;

    const private_module_t  *gralloc_module;
    alloc_device_t          *alloc_device;
    const hwc_procs_t       *procs;
    pthread_t               vsync_thread;
    int                     force_gpu;

    int32_t                 xres;
    int32_t                 yres;
    int32_t                 xdpi;
    int32_t                 ydpi;
    int32_t                 vsync_period;

    int  hdmi_mixer0;
    bool hdmi_hpd;
    bool hdmi_enabled;
    bool hdmi_blanked;
    int  hdmi_w;
    int  hdmi_h;

#ifdef USES_WFD
    bool wfd_hpd;
    bool wfd_enabled;
    bool wfd_blanked;
    int  wfd_w;
    int  wfd_h;
    int  wfd_disp_w;
    int  wfd_disp_h;
    int  wfd_buf_fd[3];
    struct wfd_layer_t      wfd_info;
    int  wfd_locked_fd;
    bool mPresentationMode;
    int wfd_skipping;
    int wfd_sleepctrl;

#endif

    hdmi_layer_t            hdmi_layers[2];

    exynos4_fimc_data_t      fimc[NUM_FIMC_UNITS];

    struct s3c_fb_win_config last_config[NUM_HW_WINDOWS];
    size_t                  last_fb_window;
    const void              *last_handles[NUM_HW_WINDOWS];
    exynos4_fimc_map_t       last_fimc_map[NUM_HW_WINDOWS];
#ifdef SKIP_STATIC_LAYER_COMP
    const void              *last_lay_hnd[NUM_VIRT_OVER];
    int                     last_ovly_win_idx;
    int                     last_ovly_lay_idx;
    int                     virtual_ovly_flag;
#endif
#ifdef HWC_SERVICES

#define S3D_ERROR -1
#define HDMI_PRESET_DEFAULT V4L2_DV_1080P60
#define HDMI_PRESET_ERROR -1

    android::ExynosHWCService   *mHWCService;
    int mHdmiPreset;
    int mHdmiCurrentPreset;
    bool mHdmiResolutionChanged;
    bool mHdmiResolutionHandled;
#if defined(S3D_SUPPORT)
    int mS3DMode;
#endif
    bool mUseSubtitles;
    int video_playback_status;
#endif

#ifdef HWC_DYNAMIC_RECOMPOSITION
    int VsyncInterruptStatus;
    int CompModeSwitch;
    uint64_t LastVsyncTimeStamp;
    uint64_t LastModeSwitchTimeStamp;
    int invalidateStatus;
    int needInvalidate;
    int totPixels;
    int setCallCnt;
    pthread_t   vsync_stat_thread;
    int vsyn_event_cnt;
    int invalid_trigger;
#endif

#if defined(USES_CEC)
    int mCecFd;
    int mCecPaddr;
    int mCecLaddr;
#endif

#ifdef FIMC_OUT_WA
    bool                    need_reqbufs;
    int                     wait_vsync_cnt;
#endif

#ifdef FORCEFB_YUVLAYER
    bool                    forcefb_yuvlayer;
    int                     count_sameconfig;
    /* g3d = 0, fimc = 1 */
    int                     configmode;
    int                     fimc_use;
    video_layer_config      prev_src_config;
    video_layer_config      prev_dst_config;
#endif

    bool                    force_mirror_mode;
    int                     hdmi_video_rotation;    /* HAL_TRANSFORM_ROT_XXX */
    bool                    external_display_pause;
    bool                    local_external_display_pause;
    bool                    popup_play_drm_contents;
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    bool                    use_blocking_layer;
    int                     num_of_ext_disp_layer;
    int                     num_of_ext_disp_video_layer;
    int                     num_of_ext_only_layer;
    int                     num_of_ext_flexible_layer;

    buffer_handle_t         composite_buffer_for_external[NUM_COMPOSITE_BUFFER_FOR_EXTERNAL];
    unsigned long           va_composite_buffer_for_external[NUM_COMPOSITE_BUFFER_FOR_EXTERNAL]; /* mapped address */
    size_t                  composite_buf_index;
    int                     composite_buf_width;
    int                     composite_buf_height;
    struct sec_rect         saved_layer_for_external[4];
    int                     saved_layer_count;
    bool                    is_change_external_surface;
    private_handle_t        *prev_handle_external_surfaces[5];
    private_handle_t        *prev_handle_flexible_surfaces[5];
    bool                    already_mapped_vfb;
    int                     vfb_fd;
    int                     surface_fd_for_vfb[NUM_BUFFER_U4A];  /* for ubuntu */
    int                     num_of_ext_vfb_layer;
    struct FB_TARGET_Info   fb_target_info[NUM_FB_TARGET];
    private_handle_t        *prev_handle_vfb;
#endif

    int  is_fb_layer;
    int  is_video_layer;
    int  fb_started;
    int  video_started;

#ifdef FIMC_VIDEO
    const void              *last_lay_hnd_hdmi[NUM_VIRT_OVER];
    int                     virtual_ovly_flag_hdmi;
#endif

};

#if defined(HWC_SERVICES)
enum {
    S3D_MODE_DISABLED = 0,
    S3D_MODE_READY,
    S3D_MODE_RUNNING,
    S3D_MODE_STOPPING,
};

enum {
    S3D_FB = 0,
    S3D_SBS,
    S3D_TB,
    S3D_NONE,
};
#endif
enum {
    NO_DRM = 0,
    NORMAL_DRM,
    SECURE_DRM,
};
#endif
