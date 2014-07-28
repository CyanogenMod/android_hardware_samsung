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
#include "ExynosHWC.h"

#if defined(USES_CEC)
#include "libcec.h"
#endif

#undef SUPPORT_FIMC_LOCAL_PATH

static void exynos4_cleanup_fimc_m2m(exynos4_hwc_composer_device_1_t *pdev,
        size_t fimc_idx);

static void dump_handle(private_handle_t *h)
{
    ALOGI("\t\tformat = %d, width = %u, height = %u, stride = %u, vstride = %u",
            h->format, h->width, h->height, h->stride, h->vstride);
}

static void dump_layer(hwc_layer_1_t const *l)
{
    ALOGI("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform,
            l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);

    if(l->handle && !(l->flags & HWC_SKIP_LAYER))
        dump_handle(private_handle_t::dynamicCast(l->handle));
}

static void dump_config(s3c_fb_win_config &c)
{
    ALOGI("\tstate = %u", c.state);
    if (c.state == c.S3C_FB_WIN_STATE_BUFFER) {
        ALOGI("\t\tfd = %d, offset = %u, stride = %u, "
                "x = %d, y = %d, w = %u, h = %u, "
                "format = %u, blending = %u",
                c.fd, c.offset, c.stride,
                c.x, c.y, c.w, c.h,
                c.format, c.blending);
    }
    else if (c.state == c.S3C_FB_WIN_STATE_COLOR) {
        ALOGI("\t\tcolor = %u", c.color);
    }
}

static void dump_fimc_img(exynos_fimc_img &c)
{
    ALOGI("\tx = %u, y = %u, w = %u, h = %u, fw = %u, fh = %u",
            c.x, c.y, c.w, c.h, c.fw, c.fh);
    ALOGI("\taddr = {%u, %u, %u}, rot = %u, cacheable = %u, drmMode = %u",
            c.yaddr, c.uaddr, c.vaddr, c.rot, c.cacheable, c.drmMode);
}

inline int WIDTH(const hwc_rect &rect) { return rect.right - rect.left; }
inline int HEIGHT(const hwc_rect &rect) { return rect.bottom - rect.top; }
template<typename T> inline T max(T a, T b) { return (a > b) ? a : b; }
template<typename T> inline T min(T a, T b) { return (a < b) ? a : b; }

template<typename T> void align_crop_and_center(T &w, T &h,
        hwc_rect_t *crop, size_t alignment)
{
    double aspect = 1.0 * h / w;
    T w_orig = w, h_orig = h;

    w = ALIGN(w, alignment);
    h = round(aspect * w);
    if (crop) {
        crop->left = (w - w_orig) / 2;
        crop->top = (h - h_orig) / 2;
        crop->right = crop->left + w_orig;
        crop->bottom = crop->top + h_orig;
    }
}

static bool is_transformed(const hwc_layer_1_t &layer)
{
    return layer.transform != 0;
}

static bool is_rotated(const hwc_layer_1_t &layer)
{
    return (layer.transform & HAL_TRANSFORM_ROT_90) ||
            (layer.transform & HAL_TRANSFORM_ROT_180);
}

static bool is_scaled(const hwc_layer_1_t &layer)
{
    return WIDTH(layer.displayFrame) != WIDTH(layer.sourceCrop) ||
            HEIGHT(layer.displayFrame) != HEIGHT(layer.sourceCrop);
}

static inline bool fimc_dst_cfg_changed(exynos_fimc_img &c1, exynos_fimc_img &c2)
{
    return c1.x != c2.x ||
            c1.y != c2.y ||
            c1.w != c2.w ||
            c1.h != c2.h ||
            c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode;
}

static inline bool fimc_src_cfg_changed(exynos_fimc_img &c1, exynos_fimc_img &c2)
{
    return fimc_dst_cfg_changed(c1, c2) ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh;
}

static inline bool mxr_src_cfg_changed(exynos_fimc_img &c1, exynos_fimc_img &c2)
{
    return c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh;
}
static enum s3c_fb_pixel_format exynos4_format_to_s3c_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return S3C_FB_PIXEL_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return S3C_FB_PIXEL_FORMAT_RGBX_8888;
    case HAL_PIXEL_FORMAT_RGBA_5551:
        return S3C_FB_PIXEL_FORMAT_RGBA_5551;
    case HAL_PIXEL_FORMAT_RGB_565:
        return S3C_FB_PIXEL_FORMAT_RGB_565;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return S3C_FB_PIXEL_FORMAT_BGRA_8888;
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
        return S3C_FB_PIXEL_FORMAT_BGRX_8888;
#endif
    default:
        return S3C_FB_PIXEL_FORMAT_MAX;
    }
}

static bool exynos4_format_is_supported(int format)
{
    return exynos4_format_to_s3c_format(format) < S3C_FB_PIXEL_FORMAT_MAX;
}

static bool exynos4_format_is_rgb(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        return true;

    default:
        return false;
    }
}

static bool exynos4_format_is_supported_by_fimcaler(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        return true;

    default:
        return false;
    }
}

static bool exynos4_format_is_ycrcb(int format)
{
    return format == HAL_PIXEL_FORMAT_EXYNOS_YV12;
}

static bool exynos4_format_requires_fimcaler(int format)
{
    return (exynos4_format_is_supported_by_fimcaler(format) &&
           (format != HAL_PIXEL_FORMAT_RGBX_8888) && (format != HAL_PIXEL_FORMAT_RGB_565));
}

static uint8_t exynos4_format_to_bpp(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        return 32;

    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 16;

    default:
        ALOGW("unrecognized pixel format %u", format);
        return 0;
    }
}

static bool is_x_aligned(const hwc_layer_1_t &layer, int format)
{
    if (!exynos4_format_is_supported(format))
        return true;

    uint8_t bpp = exynos4_format_to_bpp(format);
    uint8_t pixel_alignment = 32 / bpp;

    return (layer.displayFrame.left % pixel_alignment) == 0 &&
            (layer.displayFrame.right % pixel_alignment) == 0;
}

static bool dst_crop_w_aligned(int dest_w)
{
    int dst_crop_w_alignement;

   /* FIMC's dst crop size should be aligned 128Bytes */
    dst_crop_w_alignement = FIMC_DST_CROP_W_ALIGNMENT_RGB888;

    return (dest_w % dst_crop_w_alignement) == 0;
}

static uint32_t exynos4_format_to_fimc_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return HAL_PIXEL_FORMAT_BGRA_8888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return HAL_PIXEL_FORMAT_RGBA_8888;
    default:
        return format;
    }
}

#ifdef SUPPORT_FIMC_LOCAL_PATH
static int exynos4_fimc_out_down_scl_ratio(int xres, int yres)
{
    if (((xres == 720) || (xres == 640)) && (yres == 480))
        return 4;
    else if ((xres == 1280) && (yres == 720))
        return 4;
    else if ((xres == 1280) && (yres == 800))
        return 3;
    else if ((xres == 1920) && (yres == 1080))
        return 2;
    else if ((xres == 800) && (yres == 1280))
        return 2;
    else
        return 1;
}

static bool exynos4_format_is_supported_fimc_local(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_EXYNOS_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        return true;
    default:
        return false;
    }
}
#endif

static int exynos4_get_drmMode(int flags)
{
    if (flags & GRALLOC_USAGE_PROTECTED) {
#ifdef USE_NORMAL_DRM
        if (flags & GRALLOC_USAGE_PRIVATE_NONSECURE)
            return NORMAL_DRM;
        else
#endif
            return SECURE_DRM;
    } else {
        return NO_DRM;
    }
}

#ifdef SUPPORT_FIMC_LOCAL_PATH
static bool exynos4_supports_fimcaler(struct exynos4_hwc_composer_device_1_t *pdev,
        hwc_layer_1_t &layer, int format,
        bool local_path, int loc_out_downscale)
#else
static bool exynos4_supports_fimcaler(hwc_layer_1_t &layer, int format,
        bool local_path)
#endif
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    int max_w = is_rotated(layer) ? 2048 : 4800;
    int max_h = is_rotated(layer) ? 2048 : 3344;

    bool rot90or270 = !!(layer.transform & HAL_TRANSFORM_ROT_90);
    // n.b.: HAL_TRANSFORM_ROT_270 = HAL_TRANSFORM_ROT_90 |
    //                               HAL_TRANSFORM_ROT_180

    int src_w = WIDTH(layer.sourceCrop), src_h = HEIGHT(layer.sourceCrop);
    int dest_w, dest_h;
    if (rot90or270) {
        dest_w = HEIGHT(layer.displayFrame);
        dest_h = WIDTH(layer.displayFrame);
    } else {
        dest_w = WIDTH(layer.displayFrame);
        dest_h = HEIGHT(layer.displayFrame);
    }

    if (exynos4_get_drmMode(handle->flags) != NO_DRM)
        align_crop_and_center(dest_w, dest_h, NULL,
                FIMC_DST_CROP_W_ALIGNMENT_RGB888);

#ifdef SUPPORT_FIMC_LOCAL_PATH
    int max_downscale = local_path ? loc_out_downscale : 16;
#else
    int max_downscale = local_path ? 4 : 16;
#endif
    const int max_upscale = 8;

#ifdef SUPPORT_FIMC_LOCAL_PATH
    /* check whether FIMC can handle with local path */
    if (local_path) {
        /* FIMC OTF can't handle rot90 or rot270 */
        if (rot90or270)
            return 0;
        /*
         * if display co-ordinates are out of the lcd resolution,
         * skip that scenario to OpenGL.
         * FIMC OTF can't handle such scenarios.
         */
        if (layer.displayFrame.left < 0 || layer.displayFrame.top < 0 ||
            layer.displayFrame.right > pdev->xres || layer.displayFrame.bottom > pdev->yres)
            return 0;

        /* FIMC OTF can't handle GRALLOC_USAGE_PROTECTED layer */
        if (exynos4_get_drmMode(handle->flags) != NO_DRM)
            return 0;

        return exynos4_format_is_supported_by_fimcaler(format) &&
            exynos4_format_is_supported_fimc_local(format) &&
            handle->stride <= max_w &&
            src_w <= dest_w * max_downscale &&
            dest_w <= src_w * max_upscale &&
            handle->vstride <= max_h &&
            src_h <= dest_h * max_downscale &&
            dest_h <= src_h * max_upscale &&
            // per 46.2
            (dest_w % 2 == 0) &&
            (dest_h % 2 == 0);
     }
#endif

    /* check whether FIMC can handle with M2M */
    return exynos4_format_is_supported_by_fimcaler(format) &&
            dst_crop_w_aligned(dest_w) &&
            handle->stride <= max_w &&
            handle->stride % FIMC_W_ALIGNMENT == 0 &&
            src_w <= dest_w * max_downscale &&
            dest_w <= src_w * max_upscale &&
            handle->vstride <= max_h &&
            handle->vstride % FIMC_H_ALIGNMENT == 0 &&
            src_h <= dest_h * max_downscale &&
            dest_h <= src_h * max_upscale &&
            // per 46.2
            (!rot90or270 || layer.sourceCrop.top % 2 == 0) &&
            (!rot90or270 || layer.sourceCrop.left % 2 == 0);
            // per 46.3.1.6
}

static bool exynos4_requires_fimcaler(hwc_layer_1_t &layer, int format)
{
    return exynos4_format_requires_fimcaler(format) || is_scaled(layer)
            || is_transformed(layer) || !is_x_aligned(layer, format);
}

bool hdmi_is_preset_supported(struct exynos4_hwc_composer_device_1_t *dev, int preset)
{
    struct v4l2_dv_enum_preset enum_preset;
    bool found = false;
    int index = 0;
    int ret;

    while (true) {
        enum_preset.index = index++;
        ret = ioctl(dev->hdmi_layers[0].fd, VIDIOC_ENUM_DV_PRESETS, &enum_preset);

        if (ret < 0) {
            if (errno == EINVAL)
                break;
            ALOGE("%s: enum_dv_presets error, %d", __func__, errno);
            return -1;
        }

        ALOGI("%s: %d preset=%02d width=%d height=%d name=%s",
                __func__, enum_preset.index, enum_preset.preset,
                enum_preset.width, enum_preset.height, enum_preset.name);

        if (preset == enum_preset.preset) {
            dev->hdmi_w  = enum_preset.width;
            dev->hdmi_h  = enum_preset.height;
            found = true;
#if defined(HWC_SERVICES)
            dev->mHdmiCurrentPreset = preset;
#endif
        }
    }

    return found;
}

#ifdef USES_WFD
static void wfd_output(buffer_handle_t buf, exynos4_hwc_composer_device_1_t *pdev,
        exynos4_fimc_data_t *fimc, hwc_layer_1_t &layer)
{
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    private_handle_t *handle = private_handle_t::dynamicCast(buf);

    if (pdev->wfd_skipping) {
        pdev->wfd_skipping--;
    } else {
        pdev->wfd_buf_fd[0] = handle->fd;
        pdev->wfd_buf_fd[1] = handle->fd1;

        pdev->wfd_info.isPresentation = !!pdev->mPresentationMode;
        pdev->wfd_info.isDrm = !!(exynos4_get_drmMode(src_handle->flags) == SECURE_DRM);

        gettimeofday(&pdev->wfd_info.tv_stamp, NULL);
    }

    if (fimc->dst_cfg.releaseFenceFd > 0) {
        close(fimc->dst_cfg.releaseFenceFd);
        fimc->dst_cfg.releaseFenceFd = -1;
    }
    fimc->current_buf = (fimc->current_buf + 1) % NUM_FIMC_DST_BUFS;
    private_handle_t *next_h = private_handle_t::dynamicCast(fimc->dst_buf[fimc->current_buf]);
    if (next_h->fd == pdev->wfd_locked_fd)
        fimc->current_buf = (fimc->current_buf + 1) % NUM_FIMC_DST_BUFS;
}

static int wfd_enable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (dev->wfd_enabled)
        return 0;

    if (dev->wfd_blanked)
        return 0;

    if (dev->procs)
        dev->procs->hotplug(dev->procs, HWC_DISPLAY_EXTERNAL, dev->wfd_hpd);

    dev->wfd_locked_fd = -1;
    dev->wfd_buf_fd[0] = dev->wfd_buf_fd[1] = 0;
    dev->wfd_enabled = true;
    ALOGE("Wifi-Display is ON !!!");
    return 0;
}

static void wfd_disable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (!dev->wfd_enabled)
        return;

    if (dev->procs)
        dev->procs->hotplug(dev->procs, HWC_DISPLAY_EXTERNAL, dev->wfd_hpd);

    exynos4_cleanup_fimc_m2m(dev, HDMI_FIMC_IDX);

    dev->wfd_enabled = false;
    ALOGE("Wifi-Display is OFF !!!");
}

void wfd_get_config(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (dev->wfd_w == 0)
        dev->wfd_w = dev->wfd_disp_w = EXYNOS4_WFD_DEFAULT_WIDTH;

    if (dev->wfd_h == 0)
        dev->wfd_h = dev->wfd_disp_h = EXYNOS4_WFD_DEFAULT_HEIGHT;

    /* Case: YUV420, 2P: MIN(w) = 32, MIN(h) = 16 */
    if (dev->wfd_w < EXYNOS4_WFD_OUTPUT_ALIGNMENT * 2)
        dev->wfd_w = EXYNOS4_WFD_OUTPUT_ALIGNMENT * 2;
    if (dev->wfd_h < EXYNOS4_WFD_OUTPUT_ALIGNMENT)
        dev->wfd_h = EXYNOS4_WFD_OUTPUT_ALIGNMENT;
    dev->hdmi_w = dev->wfd_w;
    dev->hdmi_h = dev->wfd_h;
}
#endif

int hdmi_get_config(struct exynos4_hwc_composer_device_1_t *dev)
{
    struct v4l2_dv_preset preset;
    struct v4l2_dv_enum_preset enum_preset;
    int index = 0;
    bool found = false;
    int ret;

    if (!dev->hdmi_hpd)
        return -1;

    if (ioctl(dev->hdmi_layers[0].fd, VIDIOC_G_DV_PRESET, &preset) < 0) {
        ALOGE("%s: g_dv_preset error, %d", __func__, errno);
        return -1;
    }

    return hdmi_is_preset_supported(dev, preset.preset) ? 0 : -1;
}

static enum s3c_fb_blending exynos4_blending_to_s3c_blending(int32_t blending)
{
    switch (blending) {
    case HWC_BLENDING_NONE:
        return S3C_FB_BLENDING_NONE;
    case HWC_BLENDING_PREMULT:
        return S3C_FB_BLENDING_PREMULT;
    case HWC_BLENDING_COVERAGE:
        return S3C_FB_BLENDING_COVERAGE;

    default:
        return S3C_FB_BLENDING_MAX;
    }
}

static bool exynos4_blending_is_supported(int32_t blending)
{
    return exynos4_blending_to_s3c_blending(blending) < S3C_FB_BLENDING_MAX;
}

#if defined(USE_GRALLOC_FLAG_FOR_HDMI) || defined(USES_WFD)
static inline rotation rotateValueHAL2G2D(unsigned char transform)
{
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:  return ROT_90;
    case HAL_TRANSFORM_ROT_180: return ROT_180;
    case HAL_TRANSFORM_ROT_270: return ROT_270;
    }
    return ORIGIN;
}

static unsigned int formatValueHAL2G2D(int hal_format,
        color_format *g2d_format,
        pixel_order *g2d_order,
        uint32_t *g2d_bpp)
{
    *g2d_format = MSK_FORMAT_END;
    *g2d_order  = ARGB_ORDER_END;
    *g2d_bpp    = 0;

    switch (hal_format) {
    /* 16bpp */
    case HAL_PIXEL_FORMAT_RGB_565:
        *g2d_format = CF_RGB_565;
        *g2d_order  = AX_RGB;
        *g2d_bpp    = 2;
        break;
    case HAL_PIXEL_FORMAT_RGBA_4444:
        *g2d_format = CF_ARGB_4444;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 2;
        break;
        /* 32bpp */
    case HAL_PIXEL_FORMAT_RGBX_8888:
        *g2d_format = CF_XRGB_8888;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 4;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        *g2d_format = CF_ARGB_8888;
        *g2d_order  = AX_RGB;
        *g2d_bpp    = 4;
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        *g2d_format = CF_ARGB_8888;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 4;
        break;
        /* 12bpp */
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        *g2d_format = CF_YCBCR_420;
        *g2d_order  = P2_CBCR;
        *g2d_bpp    = 1;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        *g2d_format = CF_YCBCR_420;
        *g2d_order  = P2_CRCB;
        *g2d_bpp    = 1;
        break;
    default:
        ALOGE("%s: no matching color format(0x%x): failed",
                __func__, hal_format);
        return -1;
        break;
    }
    return 0;
}

int runCompositor(exynos4_hwc_composer_device_1_t *pdev,
        hwc_layer_1_t &src_layer, private_handle_t *dst_handle,
        uint32_t transform, uint32_t global_alpha, unsigned long solid,
        blit_op mode, bool force_clear, unsigned long srcAddress, unsigned long dstAddress)
{
    int ret = 0;
    unsigned long srcYAddress;
    unsigned long srcCbCrAddress;
    unsigned long dstYAddress;
    unsigned long dstCbCrAddress;

    ExynosRect   srcImgRect, dstImgRect;

    fimg2d_blit  BlitParam;
    fimg2d_param g2d_param;
    rotation     g2d_rotation;

    fimg2d_addr  srcYAddr;
    fimg2d_addr  srcCbCrAddr;
    fimg2d_image srcImage;
    fimg2d_rect  srcRect;

    fimg2d_addr  dstYAddr;
    fimg2d_addr  dstCbCrAddr;
    fimg2d_image dstImage;
    fimg2d_rect  dstRect;

    fimg2d_scale  Scaling;
    fimg2d_repeat Repeat;
    fimg2d_bluscr Bluscr;
    fimg2d_clip   Clipping;

    pixel_order  g2d_order;
    color_format g2d_format;
    addr_space   addr_type = ADDR_USER;

    uint32_t srcG2d_bpp, dstG2d_bpp;
    uint32_t srcImageSize, dstImageSize;
    bool src_ion_mapped = false;
    bool dst_ion_mapped = false;

    private_handle_t *src_handle = private_handle_t::dynamicCast(src_layer.handle);

    if (!force_clear) {
        srcImgRect = {src_layer.sourceCrop.left, src_layer.sourceCrop.top,
                WIDTH(src_layer.sourceCrop), HEIGHT(src_layer.sourceCrop),
                src_handle->stride, src_handle->vstride,
                src_handle->format};
    }

    int w, h;
#ifdef USES_WFD
    if (pdev->wfd_hpd) {
        w = pdev->wfd_w;
        h = pdev->wfd_h;
    } else
#endif
    {
        w = pdev->hdmi_w;
        h = pdev->hdmi_h;
    }
    dstImgRect = {src_layer.displayFrame.left, src_layer.displayFrame.top,
            WIDTH(src_layer.displayFrame), HEIGHT(src_layer.displayFrame),
            w, h,
            dst_handle->format};

    g2d_rotation = rotateValueHAL2G2D(transform);

    ALOGI("%s: \n"
            "s_fw %d s_fh %d s_w %d s_h %d s_x %d s_y %d s_f %x address %x \n"
            "d_fw %d d_fh %d d_w %d d_h %d d_x %d d_y %d d_f %x address %x \n rot %d ",
            __func__,
            srcImgRect.fullW, srcImgRect.fullH, srcImgRect.w, srcImgRect.h,
            srcImgRect.x, srcImgRect.y, srcImgRect.colorFormat, src_handle->fd,
            dstImgRect.fullW, dstImgRect.fullH, dstImgRect.w, dstImgRect.h,
            dstImgRect.x, dstImgRect.y, dstImgRect.colorFormat, dst_handle->fd, transform);

    if (!force_clear && src_handle->fd != 0) {
        int rotatedDstW = dstImgRect.w;
        int rotatedDstH = dstImgRect.h;
        if ((g2d_rotation == ROT_90) || (g2d_rotation == ROT_270)) {
            if ((srcImgRect.w != dstImgRect.h) || (srcImgRect.h != dstImgRect.w)) {
                rotatedDstW = dstImgRect.h;
                rotatedDstH = dstImgRect.w;
            }
        } else {
            if ((srcImgRect.w != dstImgRect.w) || (srcImgRect.h != dstImgRect.h)) {
                rotatedDstW = dstImgRect.w;
                rotatedDstH = dstImgRect.h;
            }
        }

        if (formatValueHAL2G2D(srcImgRect.colorFormat, &g2d_format, &g2d_order, &srcG2d_bpp) < 0) {
            ALOGE("%s: formatValueHAL2G2D() failed", __func__);
            return -1;
        }
        srcImageSize = srcImgRect.fullW*srcImgRect.fullH;
        if (srcAddress) {
            srcYAddress = srcAddress;
        } else {
            srcYAddress = (long unsigned)ion_map(src_handle->fd, srcImageSize*srcG2d_bpp, 0);
            src_ion_mapped = true;
        }

        srcYAddr    = {addr_type, srcYAddress};
        srcCbCrAddr = {addr_type, 0};
        srcRect     = {srcImgRect.x, srcImgRect.y, srcImgRect.x + srcImgRect.w, srcImgRect.y + srcImgRect.h};
        srcImage    = {srcImgRect.fullW, srcImgRect.fullH, srcImgRect.fullW*srcG2d_bpp,
                g2d_order, g2d_format, srcYAddr, srcCbCrAddr, srcRect, false};
        Scaling = {SCALING_BILINEAR, srcImgRect.w, srcImgRect.h, rotatedDstW, rotatedDstH};
    } else {
        memset(&srcImage, 0, sizeof(srcImage));
        Scaling = {NO_SCALING, 0, 0, 0, 0};
    }

    if (dst_handle->fd != 0) {
        if (dstImgRect.colorFormat == HAL_PIXEL_FORMAT_RGBA_8888)
            dstImgRect.colorFormat = HAL_PIXEL_FORMAT_BGRA_8888;

        if (formatValueHAL2G2D(dstImgRect.colorFormat, &g2d_format, &g2d_order, &dstG2d_bpp) < 0) {
            ALOGE("%s: formatValueHAL2G2D() failed", __func__);
            return -1;
        }
        dstImageSize = dstImgRect.fullW*dstImgRect.fullH;
        if (dstAddress) {
            dstYAddress = dstAddress;
        } else {
#ifdef USES_WFD
            if (dstImgRect.colorFormat == EXYNOS4_WFD_FORMAT) {
                dstYAddress = (long unsigned)ion_map(dst_handle->fd, dstImageSize, 0);
                dstCbCrAddress = (long unsigned)ion_map(dst_handle->fd1, dstImageSize / 2, 0);
            } else
#else
            {
                dstYAddress = (long unsigned)ion_map(dst_handle->fd, dstImageSize*dstG2d_bpp, 0);
            }
#endif
            dst_ion_mapped = true;
        }

        dstYAddr = {addr_type, dstYAddress};
        dstCbCrAddr = {addr_type, dstCbCrAddress};

        if (force_clear)
            dstRect = {0, 0, dstImgRect.fullW, dstImgRect.fullH};
        else
            dstRect = {dstImgRect.x, dstImgRect.y, dstImgRect.x + dstImgRect.w, dstImgRect.y + dstImgRect.h};

        dstImage = {dstImgRect.fullW, dstImgRect.fullH, dstImgRect.fullW*dstG2d_bpp,
                g2d_order, g2d_format, dstYAddr, dstCbCrAddr, dstRect, false};
    } else {
        memset(&dstImage, 0, sizeof(dstImage));
    }

    Repeat   = {NO_REPEAT, NULL};
    Bluscr   = {OPAQUE, 0, 0};
    Clipping = {false, 0, 0, 0, 0};

    g2d_param = {solid, global_alpha, false, g2d_rotation, PREMULTIPLIED, Scaling, Repeat, Bluscr, Clipping};
    if (force_clear)
        BlitParam = {mode, g2d_param, NULL, NULL, NULL, &dstImage, BLIT_SYNC, 0};
    else
        BlitParam = {mode, g2d_param, &srcImage, NULL, NULL, &dstImage, BLIT_SYNC, 0};

    ret = stretchFimgApi(&BlitParam);


    if (src_ion_mapped)
        ion_unmap((void *)srcYAddress, srcImageSize*srcG2d_bpp);

    if (dst_ion_mapped)
#ifdef USES_WFD
        if (pdev->wfd_hpd) {
            ion_unmap((void *)dstYAddress, dstImageSize);
            ion_unmap((void *)dstCbCrAddress, dstImageSize / 2);
        } else
#else
        {
            ion_unmap((void *)dstYAddress, dstImageSize*dstG2d_bpp);
        }
#endif

    if (ret < 0) {
        ALOGE("stretch failed", __func__);
        return -1;
    }

    return 0;
}
#endif

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
static unsigned long get_mapped_addr_fb_target(exynos4_hwc_composer_device_1_t *pdev, int fd)
{
    for (int i = 0; i < NUM_FB_TARGET; i++) {
        if (pdev->fb_target_info[i].fd == fd)
            return pdev->fb_target_info[i].mapped_addr;

        if (pdev->fb_target_info[i].fd == -1) {
            pdev->fb_target_info[i].fd = fd;
            pdev->fb_target_info[i].mapped_addr = (unsigned long)ion_map(fd, pdev->hdmi_w * pdev->hdmi_h * 4, 0);
            pdev->fb_target_info[i].map_size = pdev->hdmi_w * pdev->hdmi_h * 4;
            return pdev->fb_target_info[i].mapped_addr;
        }
    }
    return NULL;
}

static buffer_handle_t *exynos4_external_layer_composite(exynos4_hwc_composer_device_1_t *pdev,
        hwc_layer_1_t src_layer, int buf_index, bool clear)
{
    int ret;
    hwc_layer_1_t &layer = src_layer;

    /* if resolution change, it first free composition buffer */
    if ((pdev->composite_buf_width && (pdev->composite_buf_width != pdev->hdmi_w)) &&
            (pdev->composite_buf_height && (pdev->composite_buf_height != pdev->hdmi_h))) {
        for (size_t i = 0; i < NUM_COMPOSITE_BUFFER_FOR_EXTERNAL; i++) {
            ion_unmap((void *)pdev->va_composite_buffer_for_external[i],
                    pdev->composite_buf_width * pdev->composite_buf_height * 4);
            pdev->va_composite_buffer_for_external[i] = NULL;
            pdev->alloc_device->free(pdev->alloc_device, pdev->composite_buffer_for_external[i]);
            pdev->composite_buffer_for_external[i] = NULL;
        }
    }

    /* allocate composition buffer */
    for (size_t i = 0; i < NUM_COMPOSITE_BUFFER_FOR_EXTERNAL; i++) {
        if (!pdev->composite_buffer_for_external[i]) {
            int dst_stride;
            int usage = GRALLOC_USAGE_SW_READ_NEVER |
                GRALLOC_USAGE_SW_WRITE_NEVER |
                GRALLOC_USAGE_HW_COMPOSER;

            int ret = pdev->alloc_device->alloc(pdev->alloc_device,
                    pdev->hdmi_w, pdev->hdmi_h,
                    HAL_PIXEL_FORMAT_RGBA_8888, usage, &pdev->composite_buffer_for_external[i],
                    &dst_stride);
            if (ret < 0) {
                ALOGE("failed to allocate destination buffer: %s",
                        strerror(-ret));
            }
            pdev->composite_buf_width  = pdev->hdmi_w;
            pdev->composite_buf_height = pdev->hdmi_h;
            buffer_handle_t dst_buf = pdev->composite_buffer_for_external[i];
            private_handle_t *dst_handle = private_handle_t::dynamicCast(dst_buf);
            pdev->va_composite_buffer_for_external[i]
                = (unsigned long)ion_map(dst_handle->fd, pdev->composite_buf_width * pdev->composite_buf_height * 4, 0);
            ALOGD("composite_buffer_for_external[%d] ion_mapped address: 0x%08x\n", i, pdev->va_composite_buffer_for_external[i]);
        }
    }

    buffer_handle_t dst_buf = pdev->composite_buffer_for_external[buf_index];
    private_handle_t *dst_handle = private_handle_t::dynamicCast(dst_buf);

    unsigned long srcAddr = 0;
    if (src_layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
        private_handle_t *src_handle = private_handle_t::dynamicCast(src_layer.handle);
        srcAddr = get_mapped_addr_fb_target(pdev, src_handle->fd);
    }

    /* clear composite buffer */
    if (clear)
        ret = runCompositor(pdev, layer, dst_handle, 0, 0xff, 0xff000000, BLIT_OP_SRC_OVER, true,
                0, pdev->va_composite_buffer_for_external[buf_index]);

    /* composite src buffer to dest buffer */
    ret = runCompositor(pdev, layer, dst_handle, 0, 0xff, NULL, BLIT_OP_SRC, false, srcAddr,
            pdev->va_composite_buffer_for_external[buf_index]);

    return &dst_buf;
}
#endif

static int hdmi_enable_layer(struct exynos4_hwc_composer_device_1_t *dev,
                             hdmi_layer_t &hl)
{
    if (hl.enabled)
        return 0;

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count  = NUM_HDMI_BUFFERS;
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_DMABUF;
    if (exynos_v4l2_reqbufs(hl.fd, &reqbuf) < 0) {
        ALOGE("%s: layer%d: reqbufs failed %d", __func__, hl.id, errno);
        return -1;
    }

    if (reqbuf.count != NUM_HDMI_BUFFERS) {
        ALOGE("%s: layer%d: didn't get buffer", __func__, hl.id);
        return -1;
    }

    if (hl.id == 1) {
        if (exynos_v4l2_s_ctrl(hl.fd, V4L2_CID_TV_PIXEL_BLEND_ENABLE, 1) < 0) {
            ALOGE("%s: layer%d: PIXEL_BLEND_ENABLE failed %d", __func__,
                                                                hl.id, errno);
            return -1;
        }
    }

    ALOGI("%s: layer%d enabled", __func__, hl.id);
    hl.enabled = true;
    return 0;
}

static void hdmi_disable_layer(struct exynos4_hwc_composer_device_1_t *dev,
                               hdmi_layer_t &hl)
{
    if (!hl.enabled)
        return;

    if (hl.streaming) {
        if (exynos_v4l2_streamoff(hl.fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0)
            ALOGE("%s: layer%d: streamoff failed %d", __func__, hl.id, errno);
        hl.streaming = false;
    }

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_DMABUF;
    if (exynos_v4l2_reqbufs(hl.fd, &reqbuf) < 0)
        ALOGE("%s: layer%d: reqbufs failed %d", __func__, hl.id, errno);

    memset(&hl.cfg, 0, sizeof(hl.cfg));
    hl.current_buf = 0;
    hl.queued_buf = 0;
    hl.enabled = false;

    ALOGI("%s: layer%d disabled", __func__, hl.id);
}

static int hdmi_enable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (dev->hdmi_enabled)
        return 0;

    if (dev->hdmi_blanked)
        return 0;

    struct v4l2_subdev_format sd_fmt;
    memset(&sd_fmt, 0, sizeof(sd_fmt));
    sd_fmt.pad   = MIXER_G0_SUBDEV_PAD_SINK;
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.width  = dev->hdmi_w;
    sd_fmt.format.height = dev->hdmi_h;
    sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
    if (exynos_subdev_s_fmt(dev->hdmi_mixer0, &sd_fmt) < 0) {
        ALOGE("%s: s_fmt failed pad=%d", __func__, sd_fmt.pad);
        return -1;
    }

    struct v4l2_subdev_crop sd_crop;
    memset(&sd_crop, 0, sizeof(sd_crop));
    sd_crop.pad   = MIXER_G0_SUBDEV_PAD_SINK;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_crop.rect.width  = dev->hdmi_w;
    sd_crop.rect.height = dev->hdmi_h;
    if (exynos_subdev_s_crop(dev->hdmi_mixer0, &sd_crop) < 0) {
        ALOGE("%s: s_crop failed pad=%d", __func__, sd_crop.pad);
        return -1;
    }

    memset(&sd_fmt, 0, sizeof(sd_fmt));
    sd_fmt.pad   = MIXER_G0_SUBDEV_PAD_SOURCE;
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.width  = dev->hdmi_w;
    sd_fmt.format.height = dev->hdmi_h;
    sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
    if (exynos_subdev_s_fmt(dev->hdmi_mixer0, &sd_fmt) < 0) {
        ALOGE("%s: s_fmt failed pad=%d", __func__, sd_fmt.pad);
        return -1;
    }

    memset(&sd_crop, 0, sizeof(sd_crop));
    sd_crop.pad   = MIXER_G0_SUBDEV_PAD_SOURCE;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_crop.rect.width  = dev->hdmi_w;
    sd_crop.rect.height = dev->hdmi_h;
    if (exynos_subdev_s_crop(dev->hdmi_mixer0, &sd_crop) < 0) {
        ALOGE("%s: s_crop failed pad=%d", __func__, sd_crop.pad);
        return -1;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("persist.hdmi.hdcp_enabled", value, "1");
    int hdcp_enabled = atoi(value);

    if (exynos_v4l2_s_ctrl(dev->hdmi_layers[1].fd, V4L2_CID_TV_HDCP_ENABLE,
                           hdcp_enabled) < 0)
        ALOGE("%s: s_ctrl(CID_TV_HDCP_ENABLE) failed %d", __func__, errno);

    /* "3" is RGB709_16_235 */
    property_get("persist.hdmi.color_range", value, "3");
    int color_range = atoi(value);

    if (exynos_v4l2_s_ctrl(dev->hdmi_layers[1].fd, V4L2_CID_TV_SET_COLOR_RANGE,
                           color_range) < 0)
        ALOGE("%s: s_ctrl(CID_TV_COLOR_RANGE) failed %d", __func__, errno);

    hdmi_enable_layer(dev, dev->hdmi_layers[1]);

    dev->hdmi_enabled = true;
    return 0;
}

static void hdmi_disable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (!dev->hdmi_enabled)
        return;

    hdmi_disable_layer(dev, dev->hdmi_layers[0]);
    hdmi_disable_layer(dev, dev->hdmi_layers[1]);

    exynos4_cleanup_fimc_m2m(dev, HDMI_FIMC_IDX);
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    for (size_t i = 0; i < NUM_COMPOSITE_BUFFER_FOR_EXTERNAL; i++) {
        ion_unmap((void *)dev->va_composite_buffer_for_external[i],
                dev->composite_buf_width * dev->composite_buf_height * 4);
        dev->va_composite_buffer_for_external[i] = NULL;
        dev->alloc_device->free(dev->alloc_device, dev->composite_buffer_for_external[i]);
        dev->composite_buffer_for_external[i] = NULL;
    }

    for (int i = 0; i < NUM_FB_TARGET; i++) {
        if (dev->fb_target_info[i].fd != -1) {
            ion_unmap((void *)dev->fb_target_info[i].mapped_addr, dev->fb_target_info[i].map_size);
            dev->fb_target_info[i].fd = -1;
            dev->fb_target_info[i].mapped_addr = NULL;
            dev->fb_target_info[i].map_size = 0;
        }
    }
#endif
    dev->hdmi_enabled = false;
}

static int hdmi_output(struct exynos4_hwc_composer_device_1_t *dev,
                       hdmi_layer_t &hl,
                       hwc_layer_1_t &layer,
                       private_handle_t *h,
                       int acquireFenceFd,
                       int *releaseFenceFd)
{
    int ret = 0;

    exynos_fimc_img cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.x = layer.displayFrame.left;
    cfg.y = layer.displayFrame.top;
    cfg.w = WIDTH(layer.displayFrame);
    cfg.h = HEIGHT(layer.displayFrame);

    if (fimc_src_cfg_changed(hl.cfg, cfg) || dev->fb_started || dev->video_started) {
        struct v4l2_subdev_crop sd_crop;
        memset(&sd_crop, 0, sizeof(sd_crop));
        if (hl.id == 0)
            sd_crop.pad   = MIXER_G0_SUBDEV_PAD_SOURCE;
        else
            sd_crop.pad   = MIXER_G1_SUBDEV_PAD_SOURCE;

        if ((mxr_src_cfg_changed(hl.cfg, cfg) && (hl.id == 0)) || (hl.id == 1) || dev->video_started) {
            hdmi_disable_layer(dev, hl);

            struct v4l2_format fmt;
            memset(&fmt, 0, sizeof(fmt));
            fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            if (hl.id == 0) {
                fmt.fmt.pix_mp.width   = dev->hdmi_w;
                fmt.fmt.pix_mp.height  = dev->hdmi_h;
            } else {
                fmt.fmt.pix_mp.width   = h->stride;
                fmt.fmt.pix_mp.height  = cfg.h;
            }
            fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_BGR32;
            fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;
            fmt.fmt.pix_mp.num_planes  = 1;
            ret = exynos_v4l2_s_fmt(hl.fd, &fmt);
            if (ret < 0) {
                ALOGE("%s: layer%d: s_fmt failed %d", __func__, hl.id, errno);
                goto err;
            }

            struct v4l2_subdev_format sd_fmt;
            memset(&sd_fmt, 0, sizeof(sd_fmt));
            sd_fmt.pad   = sd_crop.pad;
            sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
            if (hl.id == 0) {
                sd_fmt.format.width    = dev->hdmi_w;
                sd_fmt.format.height   = dev->hdmi_h;
            } else {
                sd_fmt.format.width    = h->stride;
                sd_fmt.format.height   = cfg.h;
            }
            sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
            if (exynos_subdev_s_fmt(dev->hdmi_mixer0, &sd_fmt) < 0) {
                ALOGE("%s: s_fmt failed pad=%d", __func__, sd_fmt.pad);
                return -1;
            }

            hdmi_enable_layer(dev, hl);
        }

        sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        sd_crop.rect.left   = cfg.x;
        sd_crop.rect.top    = cfg.y;
        sd_crop.rect.width  = cfg.w;
        sd_crop.rect.height = cfg.h;
        if (exynos_subdev_s_crop(dev->hdmi_mixer0, &sd_crop) < 0) {
            ALOGE("%s: s_crop failed pad=%d", __func__, sd_crop.pad);
            goto err;
        }

        ALOGI("HDMI layer%d configuration:", hl.id);
        dump_fimc_img(cfg);
        hl.cfg = cfg;
    }

    struct v4l2_buffer buffer;
    struct v4l2_plane planes[1];

    if (hl.queued_buf == NUM_HDMI_BUFFERS) {
        memset(&buffer, 0, sizeof(buffer));
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_DMABUF;
        buffer.length = 1;
        buffer.m.planes = planes;
        ret = exynos_v4l2_dqbuf(hl.fd, &buffer);
        if (ret < 0) {
            ALOGE("%s: layer%d: dqbuf failed %d", __func__, hl.id, errno);
            goto err;
        }
        hl.queued_buf--;
    }

    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = hl.current_buf;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.flags = V4L2_BUF_FLAG_USE_SYNC;
    buffer.reserved = acquireFenceFd;
    buffer.length = 1;
    buffer.m.planes = planes;
    buffer.m.planes[0].m.fd = h->fd;
    if (exynos_v4l2_qbuf(hl.fd, &buffer) < 0) {
        ALOGE("%s: layer%d: qbuf failed %d", __func__, hl.id, errno);
        ret = -1;
        goto err;
    }

    if (releaseFenceFd)
        *releaseFenceFd = buffer.reserved;
    else
        close(buffer.reserved);

    hl.queued_buf++;
    hl.current_buf = (hl.current_buf + 1) % NUM_HDMI_BUFFERS;

    if (!hl.streaming) {
        if (exynos_v4l2_streamon(hl.fd, buffer.type) < 0) {
            ALOGE("%s: layer%d: streamon failed %d", __func__, hl.id, errno);
            ret = -1;
            goto err;
        }
        hl.streaming = true;
    }

err:
    if (acquireFenceFd >= 0)
        close(acquireFenceFd);

    return ret;
}

#if defined(FIMC_VIDEO)
static void hdmi_skip_static_layers(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t *contents, int ovly_idx)
{
    static int init_flag = 0;
    pdev->virtual_ovly_flag_hdmi = 0;

    if (contents->flags & HWC_GEOMETRY_CHANGED) {
        init_flag = 0;
        return;
    }

    if ((ovly_idx == -1) || (ovly_idx >= (contents->numHwLayers - 2)) ||
        ((contents->numHwLayers - ovly_idx - 1) >= NUM_VIRT_OVER_HDMI)) {
        init_flag = 0;
        return;
    }

    ovly_idx++;
    if (init_flag == 1) {
        for (size_t i = ovly_idx; i < contents->numHwLayers - 1; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (!layer.handle || (pdev->last_lay_hnd_hdmi[i - ovly_idx] !=  layer.handle)) {
                init_flag = 0;
                return;
            }
        }

        pdev->virtual_ovly_flag_hdmi = 1;
        for (size_t i = ovly_idx; i < contents->numHwLayers - 1; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.compositionType == HWC_FRAMEBUFFER)
                layer.compositionType = HWC_OVERLAY;
        }
        return;
    }

    init_flag = 1;
    for (size_t i = ovly_idx; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        pdev->last_lay_hnd_hdmi[i - ovly_idx] = layer.handle;
    }

    for (size_t i = contents->numHwLayers - ovly_idx; i < NUM_VIRT_OVER; i++)
        pdev->last_lay_hnd_hdmi[i - ovly_idx] = 0;

    return;
}
#endif

#if defined(HWC_SERVICES)
void hdmi_set_preset(exynos4_hwc_composer_device_1_t *pdev, int preset)
{
    pdev->mHdmiResolutionChanged = false;
    pdev->mHdmiResolutionHandled = false;
    pdev->hdmi_hpd = false;
    v4l2_dv_preset v_preset;
    v_preset.preset = preset;
    hdmi_disable(pdev);
    if (ioctl(pdev->hdmi_layers[0].fd, VIDIOC_S_DV_PRESET, &v_preset) != -1) {
        if (pdev->procs)
            pdev->procs->hotplug(pdev->procs, HWC_DISPLAY_EXTERNAL, false);
    }
}

int hdmi_3d_to_2d(int preset)
{
    switch (preset) {
    case V4L2_DV_720P60_FP:
    case V4L2_DV_720P60_SB_HALF:
    case V4L2_DV_720P60_TB:
        return V4L2_DV_720P60;
    case V4L2_DV_720P50_FP:
    case V4L2_DV_720P50_SB_HALF:
    case V4L2_DV_720P50_TB:
        return V4L2_DV_720P50;
    case V4L2_DV_1080P60_SB_HALF:
    case V4L2_DV_1080P60_TB:
        return V4L2_DV_1080P60;
    case V4L2_DV_1080P30_FP:
    case V4L2_DV_1080P30_SB_HALF:
    case V4L2_DV_1080P30_TB:
        return V4L2_DV_1080P30;
    default:
        return HDMI_PRESET_ERROR;
    }
}

int hdmi_S3D_format(int preset)
{
    switch (preset) {
    case V4L2_DV_720P60_SB_HALF:
    case V4L2_DV_720P50_SB_HALF:
    case V4L2_DV_1080P60_SB_HALF:
    case V4L2_DV_1080P30_SB_HALF:
        return S3D_SBS;
    case V4L2_DV_720P60_TB:
    case V4L2_DV_720P50_TB:
    case V4L2_DV_1080P60_TB:
    case V4L2_DV_1080P30_TB:
        return S3D_TB;
    default:
        return S3D_ERROR;
    }
}
#endif

#if defined(USES_CEC)
void handle_cec(exynos4_hwc_composer_device_1_t *pdev)
{
    unsigned char buffer[16];
    int size;
    unsigned char lsrc, ldst, opcode;

    size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 1000);

    /* no data available or ctrl-c */
    if (!size)
        return;

    /* "Polling Message" */
    if (size == 1)
        return;

    lsrc = buffer[0] >> 4;

    /* ignore messages with src address == mCecLaddr */
    if (lsrc == pdev->mCecLaddr)
        return;

    opcode = buffer[1];

    if (CECIgnoreMessage(opcode, lsrc)) {
        ALOGE("### ignore message coming from address 15 (unregistered)");
        return;
    }

    if (!CECCheckMessageSize(opcode, size)) {
        /*
         * For some reason the TV sometimes sends messages that are too long
         * Dropping these causes the connect process to fail, so for now we
         * simply ignore the extra data and process the message as if it had
         * the correct size
         */
        ALOGD("### invalid message size: %d(opcode: 0x%x) ###", size, opcode);
    }

    /* check if message broadcasted/directly addressed */
    if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
        ALOGE("### invalid message mode (directly addressed/broadcast) ###");
        return;
    }

    ldst = lsrc;

    /* TODO: macros to extract src and dst logical addresses */
    /* TODO: macros to extract opcode */

    switch (opcode) {
    case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
        /* respond with "Report Physical Address" */
        buffer[0] = (pdev->mCecLaddr << 4) | CEC_MSG_BROADCAST;
        buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
        buffer[2] = (pdev->mCecPaddr >> 8) & 0xFF;
        buffer[3] = pdev->mCecPaddr & 0xFF;
        buffer[4] = 3;
        size = 5;
        break;

    case CEC_OPCODE_SET_STREAM_PATH:
    case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
        /* respond with "Active Source" */
        buffer[0] = (pdev->mCecLaddr << 4) | CEC_MSG_BROADCAST;
        buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
        buffer[2] = (pdev->mCecPaddr >> 8) & 0xFF;
        buffer[3] = pdev->mCecPaddr & 0xFF;
        size = 4;
        break;

    case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
        /* respond with "Report Power Status" */
        buffer[0] = (pdev->mCecLaddr << 4) | ldst;
        buffer[1] = CEC_OPCODE_REPORT_POWER_STATUS;
        buffer[2] = 0;
        size = 3;
        break;

    case CEC_OPCODE_REPORT_POWER_STATUS:
        /* send Power On message */
        buffer[0] = (pdev->mCecLaddr << 4) | ldst;
        buffer[1] = CEC_OPCODE_USER_CONTROL_PRESSED;
        buffer[2] = 0x6D;
        size = 3;
        break;

    case CEC_OPCODE_USER_CONTROL_PRESSED:
        buffer[0] = (pdev->mCecLaddr << 4) | ldst;
        size = 1;
        break;
    case CEC_OPCODE_GIVE_DECK_STATUS:
        /* respond with "Deck Status" */
        buffer[0] = (pdev->mCecLaddr << 4) | ldst;
        buffer[1] = CEC_OPCODE_DECK_STATUS;
        buffer[2] = 0x11;
        size = 3;
        break;

    case CEC_OPCODE_ABORT:
    case CEC_OPCODE_FEATURE_ABORT:
    default:
        /* send "Feature Abort" */
        buffer[0] = (pdev->mCecLaddr << 4) | ldst;
        buffer[1] = CEC_OPCODE_FEATURE_ABORT;
        buffer[2] = CEC_OPCODE_ABORT;
        buffer[3] = 0x04;
        size = 4;
        break;
    }

    if (CECSendMessage(buffer, size) != size)
        ALOGE("CECSendMessage() failed!!!");
}

void start_cec(exynos4_hwc_composer_device_1_t *pdev)
{
    unsigned char buffer[CEC_MAX_FRAME_SIZE];
    int size;
    pdev->mCecFd = CECOpen();
    pdev->mCecPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;
    if (exynos_v4l2_g_ctrl(pdev->hdmi_layers[0].fd, V4L2_CID_TV_SOURCE_PHY_ADDR, &pdev->mCecPaddr) < 0)
        ALOGE("Error getting physical address");
    pdev->mCecLaddr = CECAllocLogicalAddress(pdev->mCecPaddr, CEC_DEVICE_PLAYER);
    /* Request power state from TV */
    buffer[0] = (pdev->mCecLaddr << 4);
    buffer[1] = CEC_OPCODE_GIVE_DEVICE_POWER_STATUS;
    size = 2;
    if (CECSendMessage(buffer, size) != size)
        ALOGE("CECSendMessage(%#x) failed!!!", buffer[0]);
}
#endif

bool exynos4_is_offscreen(hwc_layer_1_t &layer,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    return layer.sourceCrop.left > pdev->xres ||
            layer.sourceCrop.right < 0 ||
            layer.sourceCrop.top > pdev->yres ||
            layer.sourceCrop.bottom < 0;
}

size_t exynos4_visible_width(hwc_layer_1_t &layer, int format,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    int bpp;
    if (exynos4_requires_fimcaler(layer, format))
        bpp = 32;
    else
        bpp = exynos4_format_to_bpp(format);
    int left = max(layer.displayFrame.left, 0);
    int right = min(layer.displayFrame.right, pdev->xres);

    return (right - left) * bpp / 8;
}

bool exynos4_supports_overlay(hwc_layer_1_t &layer, size_t i,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    if (layer.flags & HWC_SKIP_LAYER) {
        ALOGI("\tlayer %u: skipping", i);
        return false;
    }

    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    if (!handle) {
        ALOGI("\tlayer %u: handle is NULL", i);
        return false;
    }

    if (exynos4_visible_width(layer, handle->format, pdev) < BURSTLEN_BYTES) {
        ALOGI("\tlayer %u: visible area is too narrow", i);
        return false;
    }
    if (exynos4_requires_fimcaler(layer, handle->format)) {
#ifdef SUPPORT_FIMC_LOCAL_PATH
        int down_ratio = exynos4_fimc_out_down_scl_ratio(pdev->xres, pdev->yres);
        /* Check whether FIMC can handle using local or M2M */
        if (!((exynos4_supports_fimcaler(pdev, layer, handle->format, false, down_ratio)) ||
            (exynos4_supports_fimcaler(pdev, layer, handle->format, true, down_ratio)))) {
#else
#ifdef USE_FB_PHY_LINEAR
    if (layer.displayFrame.left < 0 || layer.displayFrame.top < 0 ||
        layer.displayFrame.right > pdev->xres || layer.displayFrame.bottom > pdev->yres)
        return false;
#endif

        if (!exynos4_supports_fimcaler(layer, handle->format, false)) {
#endif
            ALOGI("\tlayer %u: fimcaler required but not supported", i);
            return false;
        }
    } else {
#ifdef USE_FB_PHY_LINEAR
        return false;
#endif
        if (!exynos4_format_is_supported(handle->format)) {
            ALOGI("\tlayer %u: pixel format %u not supported", i, handle->format);
            return false;
        }
    }
    if (!exynos4_blending_is_supported(layer.blending)) {
        ALOGI("\tlayer %u: blending %d not supported", i, layer.blending);
        return false;
    }
    if (CC_UNLIKELY(exynos4_is_offscreen(layer, pdev))) {
        ALOGW("\tlayer %u: off-screen", i);
        return false;
    }

    return true;
}

inline bool intersect(const hwc_rect &r1, const hwc_rect &r2)
{
    return !(r1.left > r2.right ||
        r1.right < r2.left ||
        r1.top > r2.bottom ||
        r1.bottom < r2.top);
}

inline hwc_rect intersection(const hwc_rect &r1, const hwc_rect &r2)
{
    hwc_rect i;
    i.top = max(r1.top, r2.top);
    i.bottom = min(r1.bottom, r2.bottom);
    i.left = max(r1.left, r2.left);
    i.right = min(r1.right, r2.right);
    return i;
}

#ifdef FORCEFB_YUVLAYER
static inline bool yuv_cfg_changed(video_layer_config &c1, video_layer_config &c2)
{
    return c1.x != c2.x ||
            c1.y != c2.y ||
            c1.w != c2.w ||
            c1.h != c2.h ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh ||
            c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode;
}

static bool exynos4_compare_yuvlayer_config(hwc_layer_1_t &layer,
        video_layer_config *pre_src_data, video_layer_config *pre_dst_data)
{
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;
    bool reconfigure = 1;

    video_layer_config new_src_cfg, new_dst_cfg;
    memset(&new_src_cfg, 0, sizeof(new_src_cfg));
    memset(&new_dst_cfg, 0, sizeof(new_dst_cfg));

    new_src_cfg.x = layer.sourceCrop.left;
    new_src_cfg.y = layer.sourceCrop.top;
    new_src_cfg.w = WIDTH(layer.sourceCrop);
    new_src_cfg.fw = src_handle->stride;
    new_src_cfg.h = HEIGHT(layer.sourceCrop);
    new_src_cfg.fh = src_handle->vstride;
    new_src_cfg.format = src_handle->format;
    new_src_cfg.drmMode = !!(exynos4_get_drmMode(src_handle->flags) == SECURE_DRM);

    new_dst_cfg.x = layer.displayFrame.left;
    new_dst_cfg.y = layer.displayFrame.top;
    new_dst_cfg.w = WIDTH(layer.displayFrame);
    new_dst_cfg.h = HEIGHT(layer.displayFrame);
    new_dst_cfg.rot = layer.transform;
    new_dst_cfg.drmMode = new_src_cfg.drmMode;

    /* check to save previous yuv layer configration */
    if (pre_src_data && pre_dst_data)
         reconfigure = yuv_cfg_changed(new_src_cfg, *pre_src_data) ||
            yuv_cfg_changed(new_dst_cfg, *pre_dst_data);

    memcpy(pre_src_data, &new_src_cfg, sizeof(new_src_cfg));
    memcpy(pre_dst_data, &new_dst_cfg, sizeof(new_dst_cfg));

    return reconfigure;

}
#endif

#ifdef SKIP_STATIC_LAYER_COMP
static void exynos4_skip_static_layers(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    static int init_flag = 0;
    int last_ovly_lay_idx = -1;

    pdev->virtual_ovly_flag = 0;
    pdev->last_ovly_win_idx = -1;
    if (contents->flags & HWC_GEOMETRY_CHANGED) {
        init_flag = 0;
        return;
    }

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        if (pdev->bufs.overlay_map[i] != -1) {
            last_ovly_lay_idx = pdev->bufs.overlay_map[i];
            pdev->last_ovly_win_idx = i;
        }
    }

    if ((last_ovly_lay_idx == -1) || (last_ovly_lay_idx >= (contents->numHwLayers - 2)) ||
        ((contents->numHwLayers - last_ovly_lay_idx - 1) >= NUM_VIRT_OVER)) {
        init_flag = 0;
        return;
    }
    pdev->last_ovly_lay_idx = last_ovly_lay_idx;
    last_ovly_lay_idx++;
    if (init_flag == 1) {
        for (size_t i = last_ovly_lay_idx; i < contents->numHwLayers -1; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (!layer.handle || (pdev->last_lay_hnd[i - last_ovly_lay_idx] !=  layer.handle)) {
                init_flag = 0;
                return;
            }
        }

        pdev->virtual_ovly_flag = 1;
        for (size_t i = last_ovly_lay_idx; i < contents->numHwLayers-1; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.compositionType == HWC_FRAMEBUFFER)
                layer.compositionType = HWC_OVERLAY;
        }
        return;
    }

    init_flag = 1;
    for (size_t i = last_ovly_lay_idx; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        pdev->last_lay_hnd[i - last_ovly_lay_idx] = layer.handle;
    }

    for (size_t i = contents->numHwLayers - last_ovly_lay_idx; i < NUM_VIRT_OVER; i++)
        pdev->last_lay_hnd[i] = 0;

    return;
}
#endif

static int exynos4_prepare_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    ALOGI("preparing %u layers for FIMD", contents->numHwLayers);

    memset(pdev->bufs.fimc_map, 0, sizeof(pdev->bufs.fimc_map));

    bool force_fb = pdev->force_gpu;

#ifdef FORCEFB_YUVLAYER
    pdev->forcefb_yuvlayer = 0;
    pdev->configmode = 0;
    /* check whether including the protected layer,
     * if including the protected layer, use the FIMC M2M
     */
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        if (layer.handle) {
            private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
            if (exynos4_get_drmMode(handle->flags) != NO_DRM) {
                ALOGI("included protected layer, should use FIMC M2M");
                goto retry;
            }
        }
    }
    /*
     * check whether same config or different config,
     * should be waited until meeting the NUM_COFIG)STABLE
     * before stablizing config, should be composed by GPU
     * faster stablizing config, should be returned by OVERLAY
     */
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        if (layer.handle) {
            private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
            if (exynos4_format_is_supported_fimc_local(handle->format) &&
                (pdev->fimc[FIMD_FIMC_IDX].fimc_mode != exynos4_fimc_map_t::FIMC_M2M)) {
                if ((layer.flags & HWC_SKIP_LAYER) ||
                    exynos4_compare_yuvlayer_config(layer, &pdev->prev_src_config, &pdev->prev_dst_config)) {
                    /* for preare */
                    force_fb = 1;
                    /* for set */
                    pdev->forcefb_yuvlayer = 1;
                    pdev->count_sameconfig = 0;
                } else {
                    if (pdev->count_sameconfig < NUM_CONFIG_STABLE) {
                        force_fb = 1;
                        pdev->forcefb_yuvlayer = 1;
                        pdev->count_sameconfig++;
                    } else {
                        pdev->configmode = 1;
                    }
                }
            }
        }
    }
#endif
retry:

#if defined(FORCE_YUV_OVERLAY)
    pdev->popup_play_drm_contents = false;
    int popup_drm_lay_idx = 0;
    bool contents_has_drm_surface = false;
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        if (layer.handle) {
            private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
            if (exynos4_get_drmMode(handle->flags) != NO_DRM) {
                contents_has_drm_surface = true;
                popup_drm_lay_idx = i;
                break;
            }
        }
    }
    pdev->popup_play_drm_contents = !!(contents_has_drm_surface && popup_drm_lay_idx);
#endif

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        pdev->bufs.overlay_map[i] = -1;

    bool fb_needed = false;
    size_t first_fb = 0, last_fb = 0;

    // find unsupported overlays
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGI("\tlayer %u: framebuffer target", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND && !force_fb) {
            ALOGI("\tlayer %u: background supported", i);
            dump_layer(&contents->hwLayers[i]);
            continue;
        }

        if (layer.handle) {
            private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

            /* external surfaces should not use overlay */
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            if (!(handle->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) &&
                    !(handle->flags & GRALLOC_USAGE_EXTERNAL_ONLY) &&
                    !(handle->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB)) {
#endif

#if defined(FORCE_YUV_OVERLAY)
            if (!pdev->popup_play_drm_contents ||
                (pdev->popup_play_drm_contents && (popup_drm_lay_idx == i))) {
#endif

#ifndef HWC_DYNAMIC_RECOMPOSITION
                if (exynos4_supports_overlay(contents->hwLayers[i], i, pdev) &&
                        !force_fb) {
#else
                pdev->totPixels += WIDTH(layer.displayFrame) * HEIGHT(layer.displayFrame);
                if (exynos4_supports_overlay(contents->hwLayers[i], i, pdev) &&
                        !force_fb && ((pdev->CompModeSwitch != HWC_2_GLES) ||
                        (exynos4_get_drmMode(handle->flags) != NO_DRM))) {
#endif
                    ALOGI("\tlayer %u: overlay supported", i);
                    layer.compositionType = HWC_OVERLAY;
#if defined(FORCE_YUV_OVERLAY)
                    if (pdev->popup_play_drm_contents)
                        layer.hints = HWC_HINT_CLEAR_FB;
#endif
                    dump_layer(&contents->hwLayers[i]);
                    continue;
                }
#if defined(FORCE_YUV_OVERLAY)
            }
#endif
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            }
#endif
        }

        if (!fb_needed) {
            first_fb = i;
            fb_needed = true;
        }
        last_fb = i;
        layer.compositionType = HWC_FRAMEBUFFER;

        dump_layer(&contents->hwLayers[i]);
    }

    // can't composite overlays sandwiched between framebuffers
    if (fb_needed) {
        for (size_t i = first_fb; i < last_fb; i++) {
#if defined(FORCE_YUV_OVERLAY)
            if (pdev->popup_play_drm_contents && (popup_drm_lay_idx == i)) {
                first_fb = 1;
                break;
            }
#endif
            contents->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
        }
    }

    // Incrementally try to add our supported layers to hardware windows.
    // If adding a layer would violate a hardware constraint, force it
    // into the framebuffer and try again.  (Revisiting the entire list is
    // necessary because adding a layer to the framebuffer can cause other
    // windows to retroactively violate constraints.)
    bool changed;
    bool fimc_used;
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
    int fimc_layers;
    int fimc_idx;
#endif
    do {
        android::Vector<hwc_rect> rects;
        android::Vector<hwc_rect> overlaps;
        size_t pixels_left, windows_left;

        fimc_used = false;

        if (fb_needed) {
            hwc_rect_t fb_rect;
            fb_rect.top = fb_rect.left = 0;
            fb_rect.right = pdev->xres - 1;
            fb_rect.bottom = pdev->yres - 1;
            pixels_left = MAX_PIXELS - pdev->xres * pdev->yres;
#ifdef USE_FB_PHY_LINEAR
            windows_left = NUM_HW_WIN_FB_PHY - 1;
#else
            windows_left = NUM_HW_WINDOWS - 1;
#endif

            rects.push_back(fb_rect);
        }
        else {
            pixels_left = MAX_PIXELS;
#ifdef USE_FB_PHY_LINEAR
            windows_left = 1;
#else
            windows_left = NUM_HW_WINDOWS;
#endif
        }

        changed = false;
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
        fimc_layers = 0;
        fimc_idx = 0;
#endif
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if ((layer.flags & HWC_SKIP_LAYER) ||
                    layer.compositionType == HWC_FRAMEBUFFER_TARGET)
                continue;

            private_handle_t *handle = private_handle_t::dynamicCast(
                    layer.handle);

            // we've already accounted for the framebuffer above
            if (layer.compositionType == HWC_FRAMEBUFFER)
                continue;

            // only layer 0 can be HWC_BACKGROUND, so we can
            // unconditionally allow it without extra checks
            if (layer.compositionType == HWC_BACKGROUND) {
                windows_left--;
                continue;
            }

            size_t pixels_needed = WIDTH(layer.displayFrame) *
                    HEIGHT(layer.displayFrame);
            bool can_compose = windows_left && pixels_needed <= pixels_left;
            bool fimc_required = exynos4_requires_fimcaler(layer, handle->format);
            if (fimc_required) {
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
                if (fimc_layers >= 2)
#endif
                can_compose = can_compose && !fimc_used;
            }

            // hwc_rect_t right and bottom values are normally exclusive;
            // the intersection logic is simpler if we make them inclusive
            hwc_rect_t visible_rect = layer.displayFrame;
            visible_rect.right--; visible_rect.bottom--;

            // no more than 2 layers can overlap on a given pixel
            for (size_t j = 0; can_compose && j < overlaps.size(); j++) {
                if (intersect(visible_rect, overlaps.itemAt(j)))
                    can_compose = false;
            }

            if (!can_compose) {
                layer.compositionType = HWC_FRAMEBUFFER;
                if (!fb_needed) {
                    first_fb = last_fb = i;
                    fb_needed = true;
                }
                else {
                    first_fb = min(i, first_fb);
                    last_fb = max(i, last_fb);
                }
                changed = true;
                break;
            }

            for (size_t j = 0; j < rects.size(); j++) {
                const hwc_rect_t &other_rect = rects.itemAt(j);
                if (intersect(visible_rect, other_rect))
                    overlaps.push_back(intersection(visible_rect, other_rect));
            }
            rects.push_back(visible_rect);
            pixels_left -= pixels_needed;
            windows_left--;
            if (fimc_required) {
                fimc_used = true;
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
                fimc_layers++;
#endif
            }
        }

        if (changed)
            for (size_t i = first_fb; i < last_fb; i++)
                contents->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
    } while(changed);

    unsigned int nextWindow = 0;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

#if defined(FORCE_YUV_OVERLAY)
        if (!pdev->popup_play_drm_contents)
#endif
        if (fb_needed && i == first_fb) {
            ALOGI("assigning framebuffer to window %u\n",
                    nextWindow);
            nextWindow++;
            continue;
        }

#ifdef FORCEFB_YUVLAYER
        if (layer.handle) {
            private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
            if (exynos4_format_is_supported_fimc_local(handle->format)) {
                /* in case of changing compostiontype form FIMC to FRAMEBUFFER for yuv layer */
                if ((pdev->configmode == 1) && (layer.compositionType == HWC_FRAMEBUFFER)) {
                    pdev->forcefb_yuvlayer = 1;
                    pdev->configmode = 0;
                    pdev->count_sameconfig = 0;
                    /* for prepare */
                    force_fb = 1;
                    goto retry;
                }
            }
         }
#endif
        if (layer.compositionType != HWC_FRAMEBUFFER &&
                layer.compositionType != HWC_FRAMEBUFFER_TARGET) {
            ALOGI("assigning layer %u to window %u", i, nextWindow);
            pdev->bufs.overlay_map[nextWindow] = i;
            if (layer.compositionType == HWC_OVERLAY) {
                private_handle_t *handle =
                        private_handle_t::dynamicCast(layer.handle);
                if (exynos4_requires_fimcaler(layer, handle->format)) {
#ifdef HWC_SERVICES
                    if (pdev->hdmi_hpd && (exynos4_get_drmMode(handle->flags) == SECURE_DRM)
                        && (!pdev->video_playback_status)) {
                        /*
                         * video is a DRM content and play status is normal. video display is going to be
                         * skipped on LCD.
                         */
                         ALOGI("DRM video layer-%d display is skipped on LCD", i);
                         pdev->bufs.overlay_map[nextWindow] = -1;
                         continue;
                    }
#endif
#ifdef SUPPORT_FIMC_LOCAL_PATH
                    int down_ratio = exynos4_fimc_out_down_scl_ratio(pdev->xres, pdev->yres);
                    if (!exynos4_supports_fimcaler(pdev, layer, handle->format, true, down_ratio)) {
                        ALOGI("\tusing fimcaler %u in M2M", AVAILABLE_FIMC_UNITS[nextWindow]);
                        pdev->bufs.fimc_map[nextWindow].mode = exynos4_fimc_map_t::FIMC_M2M;
                        pdev->fimc[nextWindow].fimc_mode = exynos4_fimc_map_t::FIMC_M2M;
                    } else {
                        ALOGI("\tusing fimcaler %u in LOCAL-PATH", AVAILABLE_FIMC_UNITS[nextWindow]);
                        pdev->bufs.fimc_map[nextWindow].mode = exynos4_fimc_map_t::FIMC_LOCAL;
                        pdev->fimc[nextWindow].fimc_mode = exynos4_fimc_map_t::FIMC_LOCAL;
                    }
                    pdev->bufs.fimc_map[nextWindow].idx = FIMD_FIMC_IDX;
#else
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
                    ALOGI("\tusing fimcaler %u",
                            AVAILABLE_FIMC_UNITS[FIMD_FIMC_USAGE_IDX[fimc_idx]]);
                    pdev->bufs.fimc_map[nextWindow].mode =
                            exynos4_fimc_map_t::FIMC_M2M;
                    pdev->bufs.fimc_map[nextWindow].idx = FIMD_FIMC_USAGE_IDX[fimc_idx];
                    fimc_idx++;
#else
                    ALOGI("\tusing fimcaler %u", AVAILABLE_FIMC_UNITS[FIMD_FIMC_IDX]);
                    pdev->bufs.fimc_map[nextWindow].mode =
                            exynos4_fimc_map_t::FIMC_M2M;
                    pdev->bufs.fimc_map[nextWindow].idx = FIMD_FIMC_IDX;
#endif
#endif
                }
            }
            nextWindow++;
        }
    }
#ifdef SKIP_STATIC_LAYER_COMP
#if defined(FORCE_YUV_OVERLAY)
    if (pdev->popup_play_drm_contents)
        pdev->virtual_ovly_flag = 0;
    else
#endif
        exynos4_skip_static_layers(pdev, contents);
    if (pdev->virtual_ovly_flag)
        fb_needed = 0;
#endif

#ifdef FORCEFB_YUVLAYER
    pdev->fimc_use = fimc_used;
#else
#ifdef DUAL_VIDEO_OVERLAY_SUPORT
    for (size_t i = fimc_layers; i < 2; i++)
            exynos4_cleanup_fimc_m2m(pdev, i);
#else
    if (!fimc_used)
        exynos4_cleanup_fimc_m2m(pdev, FIMD_FIMC_IDX);
#endif
#endif

    if (fb_needed)
        pdev->bufs.fb_window = first_fb;
    else
        pdev->bufs.fb_window = NO_FB_NEEDED;

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    pdev->saved_layer_count = 0;
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        private_handle_t *h = private_handle_t::dynamicCast(layer.handle);

        /* save EXTERNAL_FLEXIBLE surface`s displayFrame geometry, it will be used to display HDMI */
        if (h && (h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE)) {
            pdev->saved_layer_for_external[pdev->saved_layer_count].x = layer.displayFrame.left;
            pdev->saved_layer_for_external[pdev->saved_layer_count].y = layer.displayFrame.top;
            pdev->saved_layer_for_external[pdev->saved_layer_count].w = WIDTH(layer.displayFrame);
            pdev->saved_layer_for_external[pdev->saved_layer_count].h = HEIGHT(layer.displayFrame);
            pdev->saved_layer_count++;
        }
    }
#endif

    return 0;
}

static void hdmi_cal_dest_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect)
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
#ifdef SUPPORT_FIMC_LOCAL_PATH
static int exynos4_config_fimc_localout(exynos4_hwc_composer_device_1_t *pdev,
        hwc_layer_1_t &layer,
        exynos4_fimc_data_t *fimc_data,
        int fimc_idx)
{
    ALOGI("configuring fimcaler %u for memory-to-fimd-localout", fimc_idx);

    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;

    exynos_fimc_img src_cfg, dst_cfg;
    memset(&src_cfg, 0, sizeof(src_cfg));
    memset(&dst_cfg, 0, sizeof(dst_cfg));

    src_cfg.x = layer.sourceCrop.left;
    src_cfg.y = layer.sourceCrop.top;
    src_cfg.w = WIDTH(layer.sourceCrop);
    src_cfg.fw = src_handle->stride;
    src_cfg.h = HEIGHT(layer.sourceCrop);
    src_cfg.fh = src_handle->vstride;
    src_cfg.yaddr = src_handle->fd;
    if (exynos4_format_is_ycrcb(src_handle->format)) {
        src_cfg.uaddr = src_handle->fd2;
        src_cfg.vaddr = src_handle->fd1;
    } else {
        src_cfg.uaddr = src_handle->fd1;
        src_cfg.vaddr = src_handle->fd2;
    }
    src_cfg.format = src_handle->format;
    src_cfg.drmMode = !!(exynos4_get_drmMode(src_handle->flags) == SECURE_DRM);
    src_cfg.acquireFenceFd = layer.acquireFenceFd;

    dst_cfg.x = layer.displayFrame.left;
    dst_cfg.y = layer.displayFrame.top;
    dst_cfg.fw = pdev->xres;
    dst_cfg.fh = pdev->yres;
    dst_cfg.w = WIDTH(layer.displayFrame);
    dst_cfg.h = HEIGHT(layer.displayFrame);
    dst_cfg.w = min(dst_cfg.w, dst_cfg.fw - dst_cfg.x);
    dst_cfg.h = min(dst_cfg.h, dst_cfg.fh - dst_cfg.y);
    dst_cfg.format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    dst_cfg.rot = layer.transform;
    dst_cfg.drmMode = src_cfg.drmMode;

    dst_cfg.yaddr = NULL;

    ALOGI("source configuration:");
    dump_fimc_img(src_cfg);

    if (!fimc_data->fimc || fimc_src_cfg_changed(src_cfg, fimc_data->src_cfg) ||
            fimc_dst_cfg_changed(dst_cfg, fimc_data->dst_cfg)) {
        int dst_stride;

        int w = ALIGN(WIDTH(layer.displayFrame), FIMC_W_ALIGNMENT);
        int h = ALIGN(HEIGHT(layer.displayFrame), FIMC_H_ALIGNMENT);

        if (fimc_data->fimc) {
#ifdef FIMC_OUT_WA
            ret = exynos_fimc_stop_exclusive(fimc_data->fimc);
            pdev->need_reqbufs = true;
            pdev->count_sameconfig = 0;
#else
            ret = exynos_fimc_stop_exclusive(fimc_data->fimc);
#endif
            if (ret < 0) {
                ALOGE("failed to stop fimcaler %u", fimc_idx);
                goto err_fimc_local;
            }
        }

        if (!fimc_data->fimc) {
            fimc_data->fimc = exynos_fimc_create_exclusive(AVAILABLE_FIMC_UNITS[fimc_idx],
                FIMC_OUTPUT_MODE, FIMC_OUT_FIMD, false);
            if (!fimc_data->fimc) {
                ALOGE("failed to create fimcaler handle");
                ret = -1;
                goto err_fimc_local;
            }
        }

        ret = exynos_fimc_config_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
        if (ret < 0) {
            ALOGE("failed to configure fimcaler %u", fimc_idx);
            goto err_fimc_local;
        }
    }

    ALOGI("destination configuration:");
    dump_fimc_img(dst_cfg);

    ret = exynos_fimc_run_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
    if (ret < 0) {
        ALOGE("failed to run fimcaler %u", fimc_idx);
        goto err_fimc_local;
    }

    memcpy(&fimc_data->src_cfg, &src_cfg, sizeof(fimc_data->src_cfg));
    memcpy(&fimc_data->dst_cfg, &dst_cfg, sizeof(fimc_data->dst_cfg));

    layer.releaseFenceFd = src_cfg.releaseFenceFd;
    return 0;

err_fimc_local:
    if (src_cfg.acquireFenceFd >= 0)
        close(src_cfg.acquireFenceFd);

    exynos_fimc_destroy(fimc_data->fimc);
    fimc_data->fimc = NULL;

    memset(&fimc_data->src_cfg, 0, sizeof(fimc_data->src_cfg));
    memset(&fimc_data->dst_cfg, 0, sizeof(fimc_data->dst_cfg));

    return ret;
}
#endif

static int exynos4_prepare_hdmi(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    ALOGI("preparing %u layers for HDMI", contents->numHwLayers);
    hwc_layer_1_t *video_layer = NULL;
#if defined(FIMC_VIDEO)
    int numVideoLayers = 0;
    int videoIndex = -1;
#endif

    pdev->force_mirror_mode = false;
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    pdev->use_blocking_layer = false;
    pdev->num_of_ext_disp_layer = 0;
    pdev->num_of_ext_disp_video_layer = 0;
    pdev->num_of_ext_only_layer = 0;
    pdev->num_of_ext_flexible_layer = 0;
    int used_layer_count = 0;
    pdev->is_change_external_surface = false;
    pdev->num_of_ext_vfb_layer = 0;

    pdev->local_external_display_pause = pdev->external_display_pause;

    /* summerize layer`s information */
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        if (layer.handle) {
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);

            if ((h->flags & GRALLOC_USAGE_EXTERNAL_ONLY) ||
#if defined(FIMC_VIDEO)
                (h->flags & GRALLOC_USAGE_EXTERNAL_DISP) ||
#endif
                (h->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB) ||
                (h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE)) {
                pdev->num_of_ext_disp_layer++;
                if (h->flags & GRALLOC_USAGE_EXTERNAL_ONLY) {
                    if (pdev->prev_handle_external_surfaces[pdev->num_of_ext_only_layer] != h) {
                        pdev->prev_handle_external_surfaces[pdev->num_of_ext_only_layer] = h;
                        pdev->is_change_external_surface = true;
                    }
                    pdev->num_of_ext_only_layer++;
                } else if (h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) {
                    if (pdev->prev_handle_flexible_surfaces[pdev->num_of_ext_flexible_layer] != h) {
                        pdev->prev_handle_flexible_surfaces[pdev->num_of_ext_flexible_layer] = h;
                        pdev->is_change_external_surface = true;
                    }
                    pdev->num_of_ext_flexible_layer++;
                } else if (h->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB) {
                    pdev->num_of_ext_vfb_layer++;
                }
            }
            if (h->flags & GRALLOC_USAGE_EXTERNAL_BLOCK)
                pdev->use_blocking_layer = true;
        }
    }

    if (!pdev->num_of_ext_disp_layer)
        pdev->force_mirror_mode = true;
#endif
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGI("\tlayer %u: framebuffer target", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND) {
            ALOGI("\tlayer %u: background layer", i);
            dump_layer(&layer);
            continue;
        }

        if (layer.handle) {
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
            /*
             * Note: The following sections enclosed in the macro FIMC_VIDEO may or may
             *       not be modified/removed at a later time
             */

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            if (pdev->local_external_display_pause) {
                layer.compositionType = HWC_OVERLAY;
                layer.flags = HWC_SKIP_HDMI_RENDERING;
                continue;
            }
#endif

            /* IF MIRROR mode, all surfaces use G3D composition */
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            else if (pdev->force_mirror_mode) {
                if (h->flags & GRALLOC_USAGE_INTERNAL_ONLY) {
                    layer.compositionType = HWC_OVERLAY;
                    layer.flags = HWC_SKIP_HDMI_RENDERING;
                } else {
                    layer.compositionType = HWC_FRAMEBUFFER;
                }
                continue;
            } else {
#endif
                /* EXTENTION mode */
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
#ifdef USES_VFB
                if (pdev->num_of_ext_vfb_layer) {
                    if (h->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB) {
                        if (!pdev->already_mapped_vfb) {
                            struct s3cfb_extdsp_time_stamp time_stamp;
                            if (pdev->surface_fd_for_vfb[0] == -1) {
                                pdev->surface_fd_for_vfb[0] = h->fd;
                            } else if (pdev->surface_fd_for_vfb[1] == -1) {
                                pdev->surface_fd_for_vfb[1] = h->fd;
                            } else if (pdev->surface_fd_for_vfb[2] == -1) {
                                pdev->surface_fd_for_vfb[2] = h->fd;
                                pdev->already_mapped_vfb = true;
                            }
                            time_stamp.y_fd = h->fd;
                            time_stamp.uv_fd = -1;
                            if (ioctl(pdev->vfb_fd, S3CFB_EXTDSP_PUT_FD, &(time_stamp)) < 0) {
                                ALOGE("%s::S3CFB_EXTDSP_PUT_FD fail", __func__);
                            }
                        }
                        layer.compositionType = HWC_OVERLAY;
                        layer.flags = 0;
                    } else {
                        /* SKIP HDMI rendering others */
                        layer.compositionType = HWC_OVERLAY;
                        layer.flags = HWC_SKIP_HDMI_RENDERING;
                    }
                    continue;
                }
#endif
                /* if there is a blocking_layer, only display it, others ignore */
                if (pdev->use_blocking_layer) {
                    if (h->flags & GRALLOC_USAGE_EXTERNAL_BLOCK) {
                        if (h->flags & GRALLOC_USAGE_EXTERNAL_ONLY) {
                            struct v4l2_rect dest_rect;
                            hdmi_cal_dest_rect(WIDTH(layer.sourceCrop), HEIGHT(layer.sourceCrop),
                                    pdev->hdmi_w, pdev->hdmi_h, &dest_rect);
                            layer.displayFrame.left = dest_rect.left;
                            layer.displayFrame.top = dest_rect.top;
                            layer.displayFrame.right = dest_rect.width + dest_rect.left;
                            layer.displayFrame.bottom = dest_rect.height + dest_rect.top;
                            layer.compositionType = HWC_OVERLAY;
                            layer.flags = 0;
                        } else if(h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) {
                            /* if EXTERNL_FLEXIBLE surface, it use saved displayFrame geometry in prepare_fimd()*/
                            layer.displayFrame.left = pdev->saved_layer_for_external[used_layer_count].x;
                            layer.displayFrame.top = pdev->saved_layer_for_external[used_layer_count].y;
                            layer.displayFrame.right = pdev->saved_layer_for_external[used_layer_count].w
                                + pdev->saved_layer_for_external[used_layer_count].x;
                            layer.displayFrame.bottom = pdev->saved_layer_for_external[used_layer_count].h
                                + pdev->saved_layer_for_external[used_layer_count].y;
                            used_layer_count++;
                            layer.compositionType = HWC_OVERLAY;
                            layer.flags = 0;
                        }
                    } else {
                        /* SKIP HDMI rendering others */
                        layer.compositionType = HWC_OVERLAY;
                        layer.flags = HWC_SKIP_HDMI_RENDERING;
                    }
                    continue;
                } else {
                    if (h->flags & GRALLOC_USAGE_INTERNAL_ONLY) {
                        layer.compositionType = HWC_OVERLAY;
                        layer.flags = HWC_SKIP_HDMI_RENDERING;
                        continue;
                    }
#endif

#if defined(FIMC_VIDEO)
                    if (((exynos4_get_drmMode(h->flags) == SECURE_DRM) || (h->flags & GRALLOC_USAGE_EXTERNAL_DISP)) &&
                        exynos4_supports_fimcaler(layer, h->format, false)) {
#else
                    if (exynos4_get_drmMode(h->flags) == SECURE_DRM) {
#endif
#if !defined(FIMC_VIDEO)
                            if (!video_layer) {
#endif
                                video_layer = &layer;
                                layer.compositionType = HWC_OVERLAY;
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
                                layer.flags = 0;
#endif
#if defined(FIMC_VIDEO)
                                videoIndex = i;
                                numVideoLayers++;
#endif
                                ALOGI("\tlayer %u: video layer", i);
                                dump_layer(&layer);
                                continue;
#if !defined(FIMC_VIDEO)
                            }
#endif
                    }
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
#if defined(FIMC_VIDEO)
                    if (numVideoLayers <= 1) {
#endif
                        if (pdev->num_of_ext_only_layer || pdev->num_of_ext_flexible_layer) {
                            /* this surface will display external with scaling */
                            if (h->flags & GRALLOC_USAGE_EXTERNAL_ONLY) {
                                struct v4l2_rect dest_rect;
                                hdmi_cal_dest_rect(WIDTH(layer.sourceCrop), HEIGHT(layer.sourceCrop),
                                        pdev->hdmi_w, pdev->hdmi_h, &dest_rect);
                                layer.displayFrame.left = dest_rect.left;
                                layer.displayFrame.top = dest_rect.top;
                                layer.displayFrame.right = dest_rect.width + dest_rect.left;
                                layer.displayFrame.bottom = dest_rect.height + dest_rect.top;
                                layer.compositionType = HWC_OVERLAY;
                                layer.flags = 0;
                            /* if EXTERNAL_FLEXIBLE surface, it use saved displayFrame geometry in prepare_fimd()*/
                            } else if(h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) {
                                layer.compositionType = HWC_OVERLAY;
                                layer.flags = 0;
                                layer.displayFrame.left = pdev->saved_layer_for_external[used_layer_count].x;
                                layer.displayFrame.top = pdev->saved_layer_for_external[used_layer_count].y;
                                layer.displayFrame.right = pdev->saved_layer_for_external[used_layer_count].w
                                    + pdev->saved_layer_for_external[used_layer_count].x;
                                layer.displayFrame.bottom = pdev->saved_layer_for_external[used_layer_count].h
                                    + pdev->saved_layer_for_external[used_layer_count].y;
                                used_layer_count++;
                            /* others will be skipped */
                            } else {
                                layer.compositionType = HWC_OVERLAY;
                                layer.flags = HWC_SKIP_HDMI_RENDERING;
                            }
                        } else {
                            if (!pdev->mUseSubtitles) {
                                layer.compositionType = HWC_OVERLAY;
                                layer.flags = HWC_SKIP_HDMI_RENDERING;
                            } else {
                                layer.compositionType = HWC_FRAMEBUFFER;
                            }
                        }

#if defined(FIMC_VIDEO)
                    } else {
                        layer.compositionType = HWC_FRAMEBUFFER;
                    }
#endif
#endif

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
                }
            }
#else
                    layer.compositionType = HWC_FRAMEBUFFER;
                    dump_layer(&layer);
#endif
        }
    }
#if defined(FIMC_VIDEO)
    if (numVideoLayers == 1) {
        for (int i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
#if defined(HWC_SERVICES)
            if (!pdev->mUseSubtitles || i == videoIndex) {
#endif
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
                if (i == videoIndex) {
                    layer.compositionType = HWC_OVERLAY;
                    layer.flags = 0;
                }
#endif
#if defined(HWC_SERVICES)
            }
#endif

            if (i == videoIndex) {
                struct v4l2_rect dest_rect;
#if defined(S3D_SUPPORT)
                if (pdev->mS3DMode != S3D_MODE_DISABLED) {
                    layer.displayFrame.left = 0;
                    layer.displayFrame.top = 0;
                    layer.displayFrame.right = pdev->hdmi_w;
                    layer.displayFrame.bottom = pdev->hdmi_h;
                }
#endif
            }
        }

            hdmi_skip_static_layers(pdev, contents, videoIndex);

    } else if (numVideoLayers > 1) {
        for (int i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.compositionType == HWC_FRAMEBUFFER_TARGET ||
                layer.compositionType == HWC_BACKGROUND)
                continue;
            layer.compositionType = HWC_FRAMEBUFFER;
        }
    }
#endif
    return 0;
}

#ifdef USES_WFD
static int exynos4_prepare_wfd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    ALOGI("preparing %u layers for WFD", contents->numHwLayers);
    hwc_layer_1_t *video_layer = NULL;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGI("\tlayer %u: framebuffer target", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND) {
            ALOGI("\tlayer %u: background layer", i);
            dump_layer(&layer);
            continue;
        }

        if (layer.handle) {
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);

            if ((exynos4_get_drmMode(h->flags) != NO_DRM) && (h->flags & GRALLOC_USAGE_EXTERNAL_DISP) &&
#ifdef SUPPORT_FIMC_LOCAL_PATH
                (exynos4_supports_fimcaler(pdev, layer, h->format, false, 0))) {
#else
                (exynos4_supports_fimcaler(layer, h->format, false))) {
#endif

                if (!video_layer) {
                    video_layer = &layer;
                    layer.compositionType = HWC_OVERLAY;

                    struct v4l2_rect dest_rect;

                    hdmi_cal_dest_rect(WIDTH(layer.sourceCrop), HEIGHT(layer.sourceCrop),
                            pdev->wfd_disp_w, pdev->wfd_disp_h, &dest_rect);
                    layer.displayFrame.left = dest_rect.left;
                    layer.displayFrame.top = dest_rect.top;
                    layer.displayFrame.right = dest_rect.width + dest_rect.left;
                    layer.displayFrame.bottom = dest_rect.height + dest_rect.top;

                    ALOGI("\tlayer %u: video layer", i);
                    dump_layer(&layer);
                    continue;
                }
            }
        }
        layer.compositionType = HWC_FRAMEBUFFER;
        dump_layer(&layer);
    }

    return 0;
}
#endif

static int exynos4_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || !displays)
        return 0;

    exynos4_hwc_composer_device_1_t *pdev =
            (exynos4_hwc_composer_device_1_t *)dev;

#ifdef USES_WFD
    if (pdev->wfd_hpd)
         wfd_enable(pdev);
    else
        wfd_disable(pdev);
#endif

    hwc_display_contents_1_t *fimd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];

#ifdef HWC_DYNAMIC_RECOMPOSITION
    pdev->invalidateStatus = 0;
    pdev->totPixels = 0;
#endif

    if (pdev->hdmi_hpd) {
        hdmi_enable(pdev);
    } else {
        hdmi_disable(pdev);
    }

    if (fimd_contents) {
        int err = exynos4_prepare_fimd(pdev, fimd_contents);
        if (err)
            return err;
    }

    if (hdmi_contents) {
        int err = 0;
#ifdef USES_WFD
        if (pdev->wfd_enabled)
            err = exynos4_prepare_wfd(pdev, hdmi_contents);
        else
#endif
            err = exynos4_prepare_hdmi(pdev, hdmi_contents);
        if (err)
            return err;
    }

    return 0;
}

static int exynos4_config_fimc_m2m(hwc_layer_1_t &layer,
        exynos4_hwc_composer_device_1_t *pdev, exynos4_fimc_data_t *fimc_data,
        int fimc_idx, int dst_format, hwc_rect_t *sourceCrop)
{
    ALOGI("configuring fimcaler %u for memory-to-memory", AVAILABLE_FIMC_UNITS[fimc_idx]);

    alloc_device_t* alloc_device = pdev->alloc_device;
    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;
    int uoffset = (GRALLOC_ALIGN(src_handle->stride, 16) * src_handle->vstride);
    int voffset = uoffset + ((GRALLOC_ALIGN(src_handle->stride, 16) * src_handle->vstride) / 4);
#if USES_WFD
    int wfd_w = ALIGN(pdev->wfd_w, EXYNOS4_WFD_OUTPUT_ALIGNMENT);
    int wfd_disp_w = ALIGN(pdev->wfd_disp_w, 2);
    int wfd_disp_h = ALIGN(pdev->wfd_disp_h, 2);
#endif

    exynos_fimc_img src_cfg, dst_cfg;
    memset(&src_cfg, 0, sizeof(src_cfg));
    memset(&dst_cfg, 0, sizeof(dst_cfg));

    hwc_rect_t sourceCropTemp;
    if (!sourceCrop)
        sourceCrop = &sourceCropTemp;

    src_cfg.x = layer.sourceCrop.left;
    src_cfg.y = layer.sourceCrop.top;
    src_cfg.w = WIDTH(layer.sourceCrop);
    src_cfg.fw = src_handle->stride;
    src_cfg.h = HEIGHT(layer.sourceCrop);
    src_cfg.fh = src_handle->vstride;
    src_cfg.yaddr = src_handle->fd;
    if (fimc_idx == FIMD_FIMC_SBS_IDX || fimc_idx == HDMI_FIMC_SBS_IDX)
        src_cfg.w /= 2;
    if (fimc_idx == FIMD_FIMC_TB_IDX || fimc_idx == HDMI_FIMC_TB_IDX)
        src_cfg.h /= 2;
    if (exynos4_format_is_ycrcb(src_handle->format)) {
        src_cfg.uaddr = src_handle->base + voffset;
        src_cfg.vaddr = src_handle->base + uoffset;
    } else {
        src_cfg.uaddr = src_handle->base + uoffset;
        src_cfg.vaddr = src_handle->base + voffset;
    }
    src_cfg.format = exynos4_format_to_fimc_format(src_handle->format);
    src_cfg.drmMode = !!(exynos4_get_drmMode(src_handle->flags) == SECURE_DRM);
    src_cfg.acquireFenceFd = layer.acquireFenceFd;
 //   src_cfg.mem_type = FIMC_MEM_DMABUF;
    layer.acquireFenceFd = -1;

#if USES_WFD
    if (dst_format == EXYNOS4_WFD_FORMAT) {
        dst_cfg.x = (wfd_w - wfd_disp_w) / 2;
        dst_cfg.y = (pdev->wfd_h - wfd_disp_h) / 2;
        dst_cfg.w = wfd_disp_w;
        dst_cfg.h = wfd_disp_h;
    } else
#endif
    {
        dst_cfg.x = 0;
        dst_cfg.y = 0;
        dst_cfg.w = WIDTH(layer.displayFrame);
        dst_cfg.h = HEIGHT(layer.displayFrame);
    }
    dst_cfg.rot = layer.transform;
    dst_cfg.drmMode = src_cfg.drmMode;
    dst_cfg.format = dst_format;
 //   dst_cfg.mem_type = FIMC_MEM_DMABUF;
    dst_cfg.narrowRgb = !exynos4_format_is_rgb(src_handle->format);
    if (dst_cfg.drmMode)
        align_crop_and_center(dst_cfg.w, dst_cfg.h, sourceCrop,
                FIMC_DST_CROP_W_ALIGNMENT_RGB888);

    ALOGI("source configuration:");
    dump_fimc_img(src_cfg);

    bool reconfigure = fimc_src_cfg_changed(src_cfg, fimc_data->src_cfg) ||
            fimc_dst_cfg_changed(dst_cfg, fimc_data->dst_cfg);
    bool realloc = true;
#if USES_WFD
    if (dst_format == EXYNOS4_WFD_FORMAT && !fimc_dst_cfg_changed(dst_cfg, fimc_data->dst_cfg))
        realloc = false;
#endif

    if (reconfigure && realloc) {
        int dst_stride;
        int usage = GRALLOC_USAGE_SW_READ_NEVER |
                GRALLOC_USAGE_SW_WRITE_NEVER |
#ifdef USE_FB_PHY_LINEAR
                ((fimc_idx == FIMD_FIMC_IDX) ? GRALLOC_USAGE_HW_FB_PHY_LINEAR : 0) |
#endif
                GRALLOC_USAGE_HW_COMPOSER;

        if (exynos4_get_drmMode(src_handle->flags) == SECURE_DRM) {
            usage |= GRALLOC_USAGE_PROTECTED;
#ifdef USE_NORMAL_DRM
            usage &= ~GRALLOC_USAGE_PRIVATE_NONSECURE;
        } else if (exynos4_get_drmMode(src_handle->flags) == NORMAL_DRM) {
            usage |= GRALLOC_USAGE_PROTECTED;
            usage |= GRALLOC_USAGE_PRIVATE_NONSECURE;
#endif
        }

        int w, h;
#if USES_WFD
        if (dst_format == EXYNOS4_WFD_FORMAT) {
            w = wfd_w;
            h = pdev->wfd_h;
        } else
#endif
        if (fimc_idx == HDMI_FIMC_IDX) {
            w = pdev->hdmi_w;
            h = pdev->hdmi_h;
        } else {
            w = ALIGN(dst_cfg.w, FIMC_DST_W_ALIGNMENT_RGB888);
            h = ALIGN(dst_cfg.h, FIMC_DST_H_ALIGNMENT_RGB888);
        }

        for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
            if (fimc_data->dst_buf[i]) {
                alloc_device->free(alloc_device, fimc_data->dst_buf[i]);
                fimc_data->dst_buf[i] = NULL;
            }

            if (fimc_data->dst_buf_fence[i] >= 0) {
                close(fimc_data->dst_buf_fence[i]);
                fimc_data->dst_buf_fence[i] = -1;
            }

            int format = HAL_PIXEL_FORMAT_RGBX_8888;
#ifdef USES_WFD
            format = (dst_format == EXYNOS4_WFD_FORMAT ? dst_format : format);
#endif
            int ret = alloc_device->alloc(alloc_device, w, h,
                    format, usage, &fimc_data->dst_buf[i],
                    &dst_stride);
#ifdef USES_WFD
             if (dst_format == EXYNOS4_WFD_FORMAT && !src_cfg.drmMode) {
                 /* Default color will be black */
                 char * uv_addr;
                 dst_handle = private_handle_t::dynamicCast(fimc_data->dst_buf[i]);
                 uv_addr = (char *)ion_map(dst_handle->fd1, w * h / 2, 0);
                 memset(uv_addr, 0x7f, w * h / 2);
             }
#endif

            if (ret < 0) {
                ALOGE("failed to allocate destination buffer: %s",
                        strerror(-ret));
                goto err_alloc;
            }
        }

        fimc_data->current_buf = 0;
#ifdef FIMC_SKIP_DUPLICATE_FRAME_PROCESSING
        fimc_data->last_fimc_lay_hnd = 0;
#endif
    }

#ifdef FIMC_SKIP_DUPLICATE_FRAME_PROCESSING
    if (fimc_data->last_fimc_lay_hnd == (uint32_t)layer.handle) {
        if (layer.acquireFenceFd)
            close(layer.acquireFenceFd);

        layer.releaseFenceFd = -1;
        fimc_data->dst_cfg.releaseFenceFd = -1;

        fimc_data->current_buf = (fimc_data->current_buf + NUM_FIMC_DST_BUFS - 1) % NUM_FIMC_DST_BUFS;
        if (fimc_data->dst_buf_fence[fimc_data->current_buf]) {
            close (fimc_data->dst_buf_fence[fimc_data->current_buf]);
            fimc_data->dst_buf_fence[fimc_data->current_buf] = -1;
        }
        return 0;
    } else {
        fimc_data->last_fimc_lay_hnd = (uint32_t)layer.handle;
    }
#endif

    dst_buf = fimc_data->dst_buf[fimc_data->current_buf];
    dst_handle = private_handle_t::dynamicCast(dst_buf);

    if(fimc_idx == HDMI_FIMC_IDX) {
        dst_cfg.fw = pdev->hdmi_w;
        dst_cfg.fh = pdev->hdmi_h;
    } else {
        dst_cfg.fw = dst_handle->stride;
        dst_cfg.fh = dst_handle->vstride;
    }
    dst_cfg.yaddr = dst_handle->fd;
#ifdef USES_WFD
    if (dst_format == EXYNOS4_WFD_FORMAT)
        dst_cfg.uaddr = dst_handle->fd1;
#endif
    dst_cfg.acquireFenceFd = fimc_data->dst_buf_fence[fimc_data->current_buf];
    fimc_data->dst_buf_fence[fimc_data->current_buf] = -1;

    ALOGI("destination configuration:");
    dump_fimc_img(dst_cfg);

    if ((int)dst_cfg.w != WIDTH(layer.displayFrame))
        ALOGI("padding %u x %u output to %u x %u and cropping to {%u,%u,%u,%u}",
                WIDTH(layer.displayFrame), HEIGHT(layer.displayFrame),
                dst_cfg.w, dst_cfg.h, sourceCrop->left, sourceCrop->top,
                sourceCrop->right, sourceCrop->bottom);

    if (fimc_data->fimc) {
        ALOGI("reusing open fimcaler %u", AVAILABLE_FIMC_UNITS[fimc_idx]);
    } else {
        ALOGI("opening fimcaler %u", AVAILABLE_FIMC_UNITS[fimc_idx]);
        fimc_data->fimc = exynos_fimc_create_exclusive(
                AVAILABLE_FIMC_UNITS[fimc_idx], FIMC_M2M_MODE, FIMC_DUMMY, true);
        if (!fimc_data->fimc) {
            ALOGE("failed to create fimcaler handle");
            ret = -1;
            goto err_alloc;
        }
    }

    if (reconfigure) {
        ret = exynos_fimc_stop_exclusive(fimc_data->fimc);
        if (ret < 0) {
            ALOGE("failed to stop fimcaler %u", fimc_idx);
            goto err_fimc_config;
        }

        ret = exynos_fimc_config_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
        if (ret < 0) {
            ALOGE("failed to configure fimcaler %u", fimc_idx);
            goto err_fimc_config;
        }
    }

    ret = exynos_fimc_run_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
    if (ret < 0) {
        ALOGE("failed to run fimcaler %u", fimc_idx);
        goto err_fimc_config;
    }

    fimc_data->src_cfg = src_cfg;
    fimc_data->dst_cfg = dst_cfg;

    layer.releaseFenceFd = src_cfg.releaseFenceFd;

    return 0;

err_fimc_config:
    exynos_fimc_destroy(fimc_data->fimc);
    fimc_data->fimc = NULL;
err_alloc:
    if (src_cfg.acquireFenceFd >= 0)
        close(src_cfg.acquireFenceFd);
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
       if (fimc_data->dst_buf[i]) {
           alloc_device->free(alloc_device, fimc_data->dst_buf[i]);
           fimc_data->dst_buf[i] = NULL;
       }
       if (fimc_data->dst_buf_fence[i] >= 0) {
           close(fimc_data->dst_buf_fence[i]);
           fimc_data->dst_buf_fence[i] = -1;
       }
    }
    memset(&fimc_data->src_cfg, 0, sizeof(fimc_data->src_cfg));
    memset(&fimc_data->dst_cfg, 0, sizeof(fimc_data->dst_cfg));
    return ret;
}


static void exynos4_cleanup_fimc_m2m(exynos4_hwc_composer_device_1_t *pdev,
        size_t fimc_idx)
{
    exynos4_fimc_data_t &fimc_data = pdev->fimc[fimc_idx];
    if (!fimc_data.fimc)
        return;

    ALOGI("closing fimcaler %u", AVAILABLE_FIMC_UNITS[fimc_idx]);

    exynos_fimc_stop_exclusive(fimc_data.fimc);
    exynos_fimc_destroy(fimc_data.fimc);
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
        if (fimc_data.dst_buf[i])
            pdev->alloc_device->free(pdev->alloc_device, fimc_data.dst_buf[i]);
        if (fimc_data.dst_buf_fence[i] >= 0)
            close(fimc_data.dst_buf_fence[i]);
    }

    memset(&fimc_data, 0, sizeof(fimc_data));
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++)
        fimc_data.dst_buf_fence[i] = -1;
}

static void exynos4_config_handle(private_handle_t *handle,
        hwc_rect_t &sourceCrop, hwc_rect_t &displayFrame,
        int32_t blending, int fence_fd, s3c_fb_win_config &cfg,
        exynos4_hwc_composer_device_1_t *pdev)
{
    uint32_t x, y;
    uint32_t w = WIDTH(displayFrame);
    uint32_t h = HEIGHT(displayFrame);
    uint8_t bpp = exynos4_format_to_bpp(handle->format);
    uint32_t offset = (sourceCrop.top * handle->stride + sourceCrop.left) * bpp / 8;

    if (displayFrame.left < 0) {
        unsigned int crop = -displayFrame.left;
        ALOGI("layer off left side of screen; cropping %u pixels from left edge",
                crop);
        x = 0;
        w -= crop;
        offset += crop * bpp / 8;
    } else {
        x = displayFrame.left;
    }

    if (displayFrame.right > pdev->xres) {
        unsigned int crop = displayFrame.right - pdev->xres;
        ALOGI("layer off right side of screen; cropping %u pixels from right edge",
                crop);
        w -= crop;
    }

    if (displayFrame.top < 0) {
        unsigned int crop = -displayFrame.top;
        ALOGI("layer off top side of screen; cropping %u pixels from top edge",
                crop);
        y = 0;
        h -= crop;
        offset += handle->stride * crop * bpp / 8;
    } else {
        y = displayFrame.top;
    }

    if (displayFrame.bottom > pdev->yres) {
        int crop = displayFrame.bottom - pdev->yres;
        ALOGI("layer off bottom side of screen; cropping %u pixels from bottom edge",
                crop);
        h -= crop;
    }

#ifdef DIRECT_FB_SRC_BUF_WA
    /*
     * This patch is required to solve the fb driver PAGE fault issue.
     * there is a corner case in FIMD direct FB mechanism.
     * FIMD dma_end address will be out of the mapped region for the following scenario.
     * When ((src_buf_left  != 0) || (disp_left < 0))  &&
     * ((src_buf_top + (disp_top < 0 ? disp_top : 0 ) + disp_h) == src_vstride)
     * This patch is a workaround to resolve the issue.
     */
    int stride_in_bytes = handle->stride * bpp / 8;
    int yoffset_in_lines;
    if (offset % stride_in_bytes) { // ((src_buf_left  != 0) || (disp_left != 0))
        yoffset_in_lines = offset / stride_in_bytes;
        if ((yoffset_in_lines + h + 1) > handle->vstride)
            h = handle->vstride - yoffset_in_lines - 1;
    }
#endif

    cfg.state = cfg.S3C_FB_WIN_STATE_BUFFER;
    cfg.fd = handle->fd;
    cfg.x = x;
    cfg.y = y;
    cfg.w = w;
    cfg.h = h;
    cfg.format = exynos4_format_to_s3c_format(handle->format);
    cfg.offset = offset;
    cfg.stride = handle->stride * bpp / 8;
    cfg.blending = exynos4_blending_to_s3c_blending(blending);
    cfg.fence_fd = fence_fd;
}

static void exynos4_config_overlay(hwc_layer_1_t *layer, s3c_fb_win_config &cfg,
        exynos4_hwc_composer_device_1_t *pdev)
{
    if (layer->compositionType == HWC_BACKGROUND) {
        hwc_color_t color = layer->backgroundColor;
        cfg.state = cfg.S3C_FB_WIN_STATE_COLOR;
        cfg.color = (color.r << 16) | (color.g << 8) | color.b;
        cfg.x = 0;
        cfg.y = 0;
        cfg.w = pdev->xres;
        cfg.h = pdev->yres;
        return;
    }

#ifdef FORCEFB_YUVLAYER
    if ((layer->acquireFenceFd >= 0) && pdev->forcefb_yuvlayer) {
        sync_wait(layer->acquireFenceFd, 1000);
        close(layer->acquireFenceFd);
        layer->acquireFenceFd = -1;
    }
#endif
    private_handle_t *handle = private_handle_t::dynamicCast(layer->handle);
    exynos4_config_handle(handle, layer->sourceCrop, layer->displayFrame,
            layer->blending, layer->acquireFenceFd, cfg, pdev);
}

static int exynos4_post_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    exynos4_hwc_post_data_t *pdata = &pdev->bufs;
    struct s3c_fb_win_config_data win_data;
    struct s3c_fb_win_config *config = win_data.config;

    memset(config, 0, sizeof(win_data.config));
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        config[i].fence_fd = -1;

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        int layer_idx = pdata->overlay_map[i];
        if (layer_idx != -1) {
            hwc_layer_1_t &layer = contents->hwLayers[layer_idx];
            private_handle_t *handle =
                    private_handle_t::dynamicCast(layer.handle);

            if (pdata->fimc_map[i].mode == exynos4_fimc_map_t::FIMC_M2M) {
                int fimc_idx = pdata->fimc_map[i].idx;
                exynos4_fimc_data_t &fimc = pdev->fimc[fimc_idx];

                // RGBX8888 surfaces are already in the right color order from the GPU,
                // RGB565 and YUV surfaces need the Gscaler to swap R & B
                int dst_format = HAL_PIXEL_FORMAT_BGRA_8888;
                if (exynos4_format_is_rgb(handle->format) &&
                                handle->format != HAL_PIXEL_FORMAT_RGB_565)
                    dst_format = HAL_PIXEL_FORMAT_RGBX_8888;

                hwc_rect_t sourceCrop = { 0, 0,
                        WIDTH(layer.displayFrame), HEIGHT(layer.displayFrame) };
#if defined(S3D_SUPPORT)
                if (pdev->mS3DMode == S3D_MODE_READY || pdev->mS3DMode == S3D_MODE_RUNNING) {
                    int S3DFormat = hdmi_S3D_format(pdev->mHdmiPreset);
                    if (S3DFormat == S3D_SBS)
                        fimc_idx = FIMD_FIMC_SBS_IDX;
                    else if (S3DFormat == S3D_TB)
                        fimc_idx = FIMD_FIMC_TB_IDX;
                }
#endif
                int err = exynos4_config_fimc_m2m(layer, pdev, &fimc,
                        fimc_idx, dst_format, &sourceCrop);
                if (err < 0) {
                    ALOGE("failed to configure fimcaler %u for layer %u",
                            fimc_idx, i);
                    pdata->fimc_map[i].mode = exynos4_fimc_map_t::FIMC_NONE;
                    continue;
                }

                buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
                private_handle_t *dst_handle =
                        private_handle_t::dynamicCast(dst_buf);
                int fence = fimc.dst_cfg.releaseFenceFd;
                exynos4_config_handle(dst_handle, sourceCrop,
                        layer.displayFrame, layer.blending, fence, config[i],
                        pdev);
#ifdef SUPPORT_FIMC_LOCAL_PATH
            } else if (pdata->fimc_map[i].mode == exynos4_fimc_map_t::FIMC_LOCAL) {
                int fimc_idx = pdata->fimc_map[i].idx;
                exynos4_fimc_data_t &fimc = pdev->fimc[fimc_idx];
                int err = exynos4_config_fimc_localout(pdev, layer, &fimc, fimc_idx);

                if (err < 0) {
                    ALOGE("failed to config_fimc_localout %u input for layer %u",
                            fimc_idx, i);
                    continue;
                }
#endif
            } else {
#ifdef WAIT_FOR_RENDER_FINISH
                ExynosWaitForRenderFinish(pdev->gralloc_module, &layer.handle, 1);
#endif
                exynos4_config_overlay(&layer, config[i], pdev);
            }
        }
        if (i == 0 && config[i].blending != S3C_FB_BLENDING_NONE) {
            ALOGI("blending not supported on window 0; forcing BLENDING_NONE");
            config[i].blending = S3C_FB_BLENDING_NONE;
        }

        ALOGI("window %u configuration:", i);
        dump_config(config[i]);
    }

#ifdef SUPPORT_FIMC_LOCAL_PATH
    if (!pdev->fimc_use) {
        if (pdev->fimc[FIMD_FIMC_IDX].fimc_mode == exynos4_fimc_map_t::FIMC_M2M) {
            exynos4_cleanup_fimc_m2m(pdev, FIMD_FIMC_IDX);
            pdev->fimc[FIMD_FIMC_IDX].fimc_mode = exynos4_fimc_map_t::FIMC_NONE;
        } else if (pdev->fimc[FIMD_FIMC_IDX].fimc_mode == exynos4_fimc_map_t::FIMC_LOCAL) {
#ifdef FIMC_OUT_WA
            exynos_fimc_stop_exclusive(pdev->fimc[0].fimc);
            pdev->need_reqbufs = true;
            pdev->count_sameconfig = 0;
            pdev->fimc[FIMD_FIMC_IDX].fimc_mode = exynos4_fimc_map_t::FIMC_NONE;
#else
            exynos_fimc_destroy(pdev->fimc[FIMD_FIMC_IDX].fimc);
            pdev->fimc[FIMD_FIMC_IDX].fimc = NULL;
            pdev->fimc[FIMD_FIMC_IDX].fimc_mode = exynos4_fimc_map_t::FIMC_NONE;
#endif
        }
    }
#endif

#ifdef SKIP_STATIC_LAYER_COMP
    if (pdev->virtual_ovly_flag) {
        memcpy(&win_data.config[pdev->last_ovly_win_idx + 1],
            &pdev->last_config[pdev->last_ovly_win_idx + 1], sizeof(struct s3c_fb_win_config));
        win_data.config[pdev->last_ovly_win_idx + 1].fence_fd = -1;
        for (size_t i = pdev->last_ovly_lay_idx + 1; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.compositionType == HWC_OVERLAY)
              layer.releaseFenceFd = layer.acquireFenceFd;
        }
    }
#endif

    int ret = ioctl(pdev->fd, S3CFB_WIN_CONFIG, &win_data);
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        if (config[i].fence_fd != -1)
            close(config[i].fence_fd);
    if (ret < 0) {
        ALOGE("ioctl S3CFB_WIN_CONFIG failed: %s", strerror(errno));
        return ret;
    }

    memcpy(pdev->last_config, &win_data.config, sizeof(win_data.config));
    memcpy(pdev->last_fimc_map, pdata->fimc_map, sizeof(pdata->fimc_map));
    pdev->last_fb_window = pdata->fb_window;
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        int layer_idx = pdata->overlay_map[i];
        if (layer_idx != -1) {
            hwc_layer_1_t &layer = contents->hwLayers[layer_idx];
            pdev->last_handles[i] = layer.handle;
        }
    }

    return win_data.fence;
}

static int exynos4_clear_fimd(exynos4_hwc_composer_device_1_t *pdev)
{
    struct s3c_fb_win_config_data win_data;
    memset(&win_data, 0, sizeof(win_data));

    int ret = ioctl(pdev->fd, S3CFB_WIN_CONFIG, &win_data);
    LOG_ALWAYS_FATAL_IF(ret < 0,
            "ioctl S3CFB_WIN_CONFIG failed to clear screen: %s",
            strerror(errno));
    // the causes of an empty config failing are all unrecoverable

    return win_data.fence;
}

static int exynos4_set_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    if (!contents->dpy || !contents->sur)
        return 0;

    hwc_layer_1_t *fb_layer = NULL;
    int err = 0;

    if (pdev->bufs.fb_window != NO_FB_NEEDED) {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            if (contents->hwLayers[i].compositionType ==
                    HWC_FRAMEBUFFER_TARGET) {
                pdev->bufs.overlay_map[pdev->bufs.fb_window] = i;
                fb_layer = &contents->hwLayers[i];
                break;
            }
        }

        if (CC_UNLIKELY(!fb_layer)) {
            ALOGE("framebuffer target expected, but not provided");
            err = -EINVAL;
        } else {
            ALOGI("framebuffer target buffer:");
            dump_layer(fb_layer);
        }
    }

    int fence;
    if (!err) {
        fence = exynos4_post_fimd(pdev, contents);
        if (fence < 0)
            err = fence;
    }

    if (err)
        fence = exynos4_clear_fimd(pdev);

#if defined(S3D_SUPPORT)
    bool FIMCLayer = false;
#endif
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        if (pdev->bufs.overlay_map[i] != -1) {
            hwc_layer_1_t &layer =
                    contents->hwLayers[pdev->bufs.overlay_map[i]];
            int dup_fd = dup(fence);
            if (dup_fd < 0)
                ALOGW("release fence dup failed: %s", strerror(errno));
            if (pdev->bufs.fimc_map[i].mode == exynos4_fimc_map_t::FIMC_M2M) {
                int fimc_idx = pdev->bufs.fimc_map[i].idx;
                exynos4_fimc_data_t &fimc = pdev->fimc[fimc_idx];
                fimc.dst_buf_fence[fimc.current_buf] = dup_fd;
                fimc.current_buf = (fimc.current_buf + 1) % NUM_FIMC_DST_BUFS;
#if defined(S3D_SUPPORT)
                FIMCLayer = true;
                if (!pdev->hdmi_hpd && pdev->mS3DMode == S3D_MODE_READY)
                    pdev->mS3DMode = S3D_MODE_RUNNING;
#endif
#ifdef SUPPORT_FIMC_LOCAL_PATH
            } else if (pdev->bufs.fimc_map[i].mode == exynos4_fimc_map_t::FIMC_LOCAL) {
                /* Only use close(dup_fd) case, working fine. */
                close(dup_fd);
                continue;
#endif
            } else {
                layer.releaseFenceFd = dup_fd;
            }
        }
    }
#if defined(S3D_SUPPORT)
    if (!pdev->hdmi_hpd && pdev->mS3DMode == S3D_MODE_RUNNING && !FIMCLayer)
        pdev->mS3DMode = S3D_MODE_DISABLED;
#endif
    close(fence);

    return err;
}

static int exynos4_set_hdmi(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    hwc_layer_1_t *fb_layer = NULL;
    hwc_layer_1_t *video_layer = NULL;

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    buffer_handle_t *dst_buf;
    bool use_composite_buffer_for_external = false;
    bool need_clear_composite_buffer = true;
#endif

    if (!pdev->hdmi_enabled) {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.acquireFenceFd != -1) {
                close(layer.acquireFenceFd);
                layer.acquireFenceFd = -1;
            }
        }
        return 0;
    }

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.flags & HWC_SKIP_LAYER) {
            ALOGI("HDMI skipping layer %d", i);
            continue;
        }

        if (layer.compositionType == HWC_OVERLAY) {
            if (!layer.handle)
                continue;


#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            if (layer.flags & HWC_SKIP_HDMI_RENDERING) {
                if (layer.acquireFenceFd != -1) {
                    close(layer.acquireFenceFd);
                    layer.acquireFenceFd = -1;
                }
                continue;
            } else {
#endif
#if defined(FIMC_VIDEO)
                private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);
                if ((int)get_yuv_planes(HAL_PIXEL_FORMAT_2_V4L2_PIX(handle->format)) < 0 &&
                        !(handle->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) &&
                        !(handle->flags & GRALLOC_USAGE_EXTERNAL_ONLY) &&
                        !(handle->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB) &&
                        !(handle->flags & GRALLOC_USAGE_EXTERNAL_DISP)) {
                    layer.releaseFenceFd = layer.acquireFenceFd;
                    continue;
                }
#endif
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            }
#endif

            ALOGI("HDMI video layer:");
            dump_layer(&layer);

            int fimc_idx = HDMI_FIMC_IDX;
#if defined(S3D_SUPPORT)
            bool changedPreset = false;
            if (pdev->mS3DMode != S3D_MODE_DISABLED && pdev->mHdmiResolutionChanged) {
                if (hdmi_is_preset_supported(pdev, pdev->mHdmiPreset)) {
                    pdev->mS3DMode = S3D_MODE_RUNNING;
                    hdmi_set_preset(pdev, pdev->mHdmiPreset);
                    changedPreset = true;
                } else {
                    pdev->mS3DMode = S3D_MODE_RUNNING;
                    pdev->mHdmiResolutionChanged = false;
                    pdev->mHdmiResolutionHandled = true;
                    int S3DFormat = hdmi_S3D_format(pdev->mHdmiPreset);
                    if (S3DFormat == S3D_SBS)
                        fimc_idx = HDMI_FIMC_SBS_IDX;
                    else if (S3DFormat == S3D_TB)
                        fimc_idx = HDMI_FIMC_TB_IDX;
                }
            }
#endif
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);

#if defined(FIMC_VIDEO)
            if ((exynos4_get_drmMode(h->flags) == SECURE_DRM) || (h->flags & GRALLOC_USAGE_EXTERNAL_DISP)) {
#else
            if (exynos4_get_drmMode(h->flags) == SECURE_DRM) {
#endif
                exynos4_fimc_data_t &fimc = pdev->fimc[HDMI_FIMC_IDX];
                int ret = exynos4_config_fimc_m2m(layer, pdev, &fimc,
                                                 fimc_idx,
                                                 HAL_PIXEL_FORMAT_RGBX_8888, NULL);
                if (ret < 0) {
                    ALOGE("failed to configure fimcaler for video layer");
                    continue;
                }

                buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
                private_handle_t *h = private_handle_t::dynamicCast(dst_buf);

                int acquireFenceFd = fimc.dst_cfg.releaseFenceFd;
                int releaseFenceFd = -1;

                video_layer = &layer;

                if (pdev->is_video_layer == false)
                    pdev->video_started = true;
                else
                    pdev->video_started = false;
                pdev->is_video_layer = true;

                hdmi_enable_layer(pdev, pdev->hdmi_layers[0]);

                hdmi_output(pdev, pdev->hdmi_layers[0], layer, h, acquireFenceFd,
                                                                 &releaseFenceFd);

                fimc.dst_buf_fence[fimc.current_buf] = releaseFenceFd;
                fimc.current_buf = (fimc.current_buf + 1) % NUM_FIMC_DST_BUFS;
            }

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            if (h->flags & GRALLOC_USAGE_EXTERNAL_VIRTUALFB) {
#ifdef USES_VFB
                if (pdev->prev_handle_vfb != h) {
#if 0
                    /* get the latest buffer by offset */
                    struct fb_var_screeninfo info;
                    if (ioctl(pdev->vfb_fd, FBIOGET_VSCREENINFO, &info) == -1)
                        ALOGE("FBIOGET_VSCREENINFO ioctl failed: %s", strerror(errno));
#endif

                    private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
                    hwc_layer_1_t dst_layer;
                    dst_layer.displayFrame.left = 0;
                    dst_layer.displayFrame.right = pdev->hdmi_w;
                    dst_layer.displayFrame.top = 0;
                    dst_layer.displayFrame.bottom = pdev->hdmi_h;
                    layer.releaseFenceFd = layer.acquireFenceFd;

#if 0
                    /* this code will be used to display the latest buffer by offset */
                    int index = info.yoffset / HEIGHT(dst_layer.displayFrame);
                    int surface_fd = pdev->surface_fd_for_vfb[index];
                    if ((index < NUM_BUFFER_U4A) && surface_fd != -1) {
                        h->fd = surface_fd;
                        hdmi_output(pdev, pdev->hdmi_layers[0], dst_layer, h, -1, NULL);
                    }
#else
                    hdmi_output(pdev, pdev->hdmi_layers[0], dst_layer, h, layer.acquireFenceFd,
                            &layer.releaseFenceFd);
#endif
                } else {
                    if (layer.acquireFenceFd)
                        close(layer.acquireFenceFd);
                    layer.releaseFenceFd = -1;
                }
                pdev->prev_handle_vfb = h;
                video_layer = &layer;
#endif
            } else if (h->flags & GRALLOC_USAGE_EXTERNAL_FLEXIBLE) {
                if (pdev->is_change_external_surface) {
                    if (video_layer) {
                        /* if subtitle , it use directyly use fb layer */
                        private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
                        hdmi_output(pdev, pdev->hdmi_layers[1], layer, h, layer.acquireFenceFd,
                                &layer.releaseFenceFd);
                        fb_layer = &layer;
                    } else {
                        bool clear = false;
                        if (need_clear_composite_buffer) {
                            clear = true;
                            need_clear_composite_buffer = false;
                        }
                        dst_buf = exynos4_external_layer_composite(pdev, layer, pdev->composite_buf_index, clear);
                        private_handle_t *dst_h = private_handle_t::dynamicCast(*dst_buf);

                        hwc_layer_1_t dst_layer;
                        dst_layer.displayFrame.left = 0;
                        dst_layer.displayFrame.right = pdev->hdmi_w;
                        dst_layer.displayFrame.top = 0;
                        dst_layer.displayFrame.bottom = pdev->hdmi_h;

                        hdmi_output(pdev, pdev->hdmi_layers[1], dst_layer, dst_h, layer.acquireFenceFd,
                                &layer.releaseFenceFd);
                        fb_layer = &layer;
                        use_composite_buffer_for_external = true;
                    }
                } else {
                    layer.releaseFenceFd = layer.acquireFenceFd;
                    fb_layer = &layer;
                }
            } else if (h->flags & GRALLOC_USAGE_EXTERNAL_ONLY) {
                if (pdev->is_change_external_surface) {
                    bool clear = false;
                    if (need_clear_composite_buffer) {
                        clear = true;
                        need_clear_composite_buffer = false;
                    }
                    dst_buf = exynos4_external_layer_composite(pdev, layer, pdev->composite_buf_index, clear);
                    private_handle_t *dst_h = private_handle_t::dynamicCast(*dst_buf);

                    hwc_layer_1_t dst_layer;
                    dst_layer.displayFrame.left = 0;
                    dst_layer.displayFrame.right = pdev->hdmi_w;
                    dst_layer.displayFrame.top = 0;
                    dst_layer.displayFrame.bottom = pdev->hdmi_h;

                    hdmi_output(pdev, pdev->hdmi_layers[1], dst_layer, dst_h, layer.acquireFenceFd,
                            &layer.releaseFenceFd);
                    fb_layer = &layer;
                    use_composite_buffer_for_external = true;
                } else {
                    layer.releaseFenceFd = layer.acquireFenceFd;
                    fb_layer = &layer;
                }
            }
#endif
        }

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            if (!layer.handle
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
                || !pdev->force_mirror_mode
#endif
               ) {
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
                if (pdev->force_mirror_mode || !pdev->mUseSubtitles)
#endif
                    continue;
            }

            ALOGI("HDMI FB layer:");
            dump_layer(&layer);

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            if (pdev->mHdmiCurrentPreset == V4L2_DV_1080P30) {
                /* in case of 1080P30,  memcpy to tempbuffer, and then render that */
                dst_buf = exynos4_external_layer_composite(pdev, layer, pdev->composite_buf_index, false);
                layer.releaseFenceFd = layer.acquireFenceFd;
                private_handle_t *dst_h = private_handle_t::dynamicCast(*dst_buf);
                hwc_layer_1_t dst_layer;
                dst_layer.displayFrame.left = 0;
                dst_layer.displayFrame.right = pdev->hdmi_w;
                dst_layer.displayFrame.top = 0;
                dst_layer.displayFrame.bottom = pdev->hdmi_h;

                hdmi_output(pdev, pdev->hdmi_layers[1], dst_layer, dst_h, -1, NULL);
                use_composite_buffer_for_external = true;
            } else {
#endif
                private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
                hdmi_output(pdev, pdev->hdmi_layers[1], layer, h, layer.acquireFenceFd,
                            &layer.releaseFenceFd);
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
            }
#endif
            fb_layer = &layer;

            if (pdev->is_fb_layer == false)
                pdev->fb_started = true;
            else
                pdev->fb_started = false;
            pdev->is_fb_layer = true;

            hdmi_enable_layer(pdev, pdev->hdmi_layers[1]);
        }
    }

#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    if (use_composite_buffer_for_external) {
        pdev->composite_buf_index++;
        if (pdev->composite_buf_index == NUM_COMPOSITE_BUFFER_FOR_EXTERNAL)
            pdev->composite_buf_index = 0;
    }
#endif

    if (!video_layer && !pdev->local_external_display_pause) {
        hdmi_disable_layer(pdev, pdev->hdmi_layers[0]);
        exynos4_cleanup_fimc_m2m(pdev, HDMI_FIMC_IDX);
#if defined(S3D_SUPPORT)
        if (pdev->mS3DMode == S3D_MODE_RUNNING && contents->numHwLayers > 1) {
            int preset = hdmi_3d_to_2d(pdev->mHdmiCurrentPreset);
            if (hdmi_is_preset_supported(pdev, preset)) {
                hdmi_set_preset(pdev, preset);
                pdev->mS3DMode = S3D_MODE_STOPPING;
                pdev->mHdmiPreset = preset;
                if (pdev->procs)
                    pdev->procs->invalidate(pdev->procs);
            } else {
                pdev->mS3DMode = S3D_MODE_DISABLED;
                pdev->mHdmiPreset = pdev->mHdmiCurrentPreset;
            }
        }
#endif
    }
    if (!fb_layer) {
        hdmi_disable_layer(pdev, pdev->hdmi_layers[1]);
        pdev->is_fb_layer = false;
    }
    if (!video_layer) {
        hdmi_disable_layer(pdev, pdev->hdmi_layers[0]);
        pdev->is_video_layer = false;
    }

#if defined(MIXER_UPDATE)
    if (exynos_v4l2_s_ctrl(pdev->hdmi_layers[1].fd, V4L2_CID_TV_UPDATE, 1) < 0) {
        ALOGE("%s: s_ctrl(CID_TV_UPDATE) failed %d", __func__, errno);
        return -1;
    }
#endif
    return 0;
}

#ifdef USES_WFD
static int exynos4_set_wfd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    hwc_layer_1_t *overlay_layer = NULL;
    hwc_layer_1_t *target_layer = NULL;

    if (!pdev->wfd_enabled) {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.acquireFenceFd != -1) {
                close(layer.acquireFenceFd);
                layer.acquireFenceFd = -1;
            }
        }
        return 0;
    }

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];
        private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);

        if (layer.flags & HWC_SKIP_LAYER) {
            ALOGI("WFD skipping layer %d", i);
            continue;
        }

        if (layer.compositionType == HWC_OVERLAY) {
             if (!layer.handle)
                 continue;

            ALOGI("WFD overlay layer:");
            dump_layer(&layer);

            overlay_layer = &layer;
        }

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            if (!layer.handle)
                continue;

            ALOGI("WFD FB target layer:");
            dump_layer(&layer);

            target_layer = &layer;
        }
    }

    if (overlay_layer || target_layer) {
        exynos4_fimc_data_t &fimc = pdev->fimc[HDMI_FIMC_IDX];
        overlay_layer = overlay_layer == NULL? target_layer : overlay_layer;
        if (pdev->wfd_sleepctrl && pdev->wfd_blanked) {
            buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
            private_handle_t *handle = private_handle_t::dynamicCast(dst_buf);

            pdev->wfd_enabled = false;
            runCompositor(pdev, *overlay_layer, handle, 0, 0xff, 0xff000000,
                             BLIT_OP_SOLID_FILL, true, 0, 0);
            wfd_output(dst_buf, pdev, &fimc, *overlay_layer);
            pdev->wfd_skipping = 1;
        } else if (pdev->wfd_blanked) {
            pdev->wfd_enabled = false;
            pdev->wfd_skipping = 1;
        } else {
            int ret = exynos4_config_fimc_m2m(*overlay_layer, pdev, &fimc,
                          HDMI_FIMC_IDX, EXYNOS4_WFD_FORMAT, NULL);
            if (ret < 0) {
                ALOGE("failed to configure fimcaler for WFD layer");
                return ret;
            }
            pdev->wfd_w = ALIGN(fimc.dst_cfg.w, EXYNOS4_WFD_OUTPUT_ALIGNMENT);
            pdev->wfd_h = ALIGN(fimc.dst_cfg.h, EXYNOS4_WFD_OUTPUT_ALIGNMENT);

            buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
            wfd_output(dst_buf, pdev, &fimc, *overlay_layer);
        }
    }

    return 0;
}
#endif

static int exynos4_set(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || !displays)
        return 0;

    exynos4_hwc_composer_device_1_t *pdev =
            (exynos4_hwc_composer_device_1_t *)dev;
    hwc_display_contents_1_t *fimd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];
    int fimd_err = 0, hdmi_err = 0;

#ifdef HWC_DYNAMIC_RECOMPOSITION
    pdev->setCallCnt++;
#endif

    if (fimd_contents)
        fimd_err = exynos4_set_fimd(pdev, fimd_contents);

    if (hdmi_contents) {
#ifdef USES_WFD
        if (pdev->wfd_enabled)
            hdmi_err = exynos4_set_wfd(pdev, hdmi_contents);
        else
#endif
        hdmi_err = exynos4_set_hdmi(pdev, hdmi_contents);
    }
#if defined(HWC_SERVICES)
#if defined(S3D_SUPPORT)
    if (pdev->mS3DMode != S3D_MODE_STOPPING && !pdev->mHdmiResolutionHandled) {
#else
    if (!pdev->mHdmiResolutionHandled) {
#endif
        pdev->mHdmiResolutionHandled = true;
        pdev->hdmi_hpd = true;
        hdmi_enable(pdev);
        if (pdev->procs) {
            pdev->procs->hotplug(pdev->procs, HWC_DISPLAY_EXTERNAL, true);
            pdev->procs->invalidate(pdev->procs);
        }
    }
    if (pdev->hdmi_hpd && pdev->mHdmiResolutionChanged) {
#if defined(S3D_SUPPORT)
        if (pdev->mS3DMode == S3D_MODE_DISABLED && hdmi_is_preset_supported(pdev, pdev->mHdmiPreset))
#else
        if (hdmi_is_preset_supported(pdev, pdev->mHdmiPreset))
#endif
            hdmi_set_preset(pdev, pdev->mHdmiPreset);
    }
#if defined(S3D_SUPPORT)
    if (pdev->mS3DMode == S3D_MODE_STOPPING)
        pdev->mS3DMode = S3D_MODE_DISABLED;
#endif
#endif

    if (fimd_err)
        return fimd_err;

    return hdmi_err;
}

#ifdef HWC_DYNAMIC_RECOMPOSITION
int exynos_getCompModeSwitch(struct exynos4_hwc_composer_device_1_t *pdev)
{
    unsigned int tot_win_size = 0;
    unsigned int lcd_size = pdev->xres * pdev->yres;

    /* initialize the Timestamps */
    if (!pdev->LastModeSwitchTimeStamp) {
        pdev->LastModeSwitchTimeStamp = pdev->LastVsyncTimeStamp;
        pdev->CompModeSwitch = NO_MODE_SWITCH;
        return 0;
    }

    /* If video layer is there, skip the mode switch */
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        if (pdev->last_fimc_map[i].mode != exynos4_fimc_map_t::FIMC_NONE) {
            if (pdev->CompModeSwitch != HWC_2_GLES) {
                return 0;
            } else {
                pdev->CompModeSwitch = GLES_2_HWC;
                return GLES_2_HWC;
            }
        }
    }

    /* Mode Switch is not required if total pixels are not more than the threshold */
    if (pdev->totPixels <= lcd_size * HWC_FIMD_BW_TH) {
        if (pdev->CompModeSwitch != HWC_2_GLES) {
            return 0;
        } else {
            pdev->CompModeSwitch = GLES_2_HWC;
            return GLES_2_HWC;
        }
    }

    /*
     * if VSYNC interrupt is disabled, there won't be any screen update in near future.
     * switch the mode to GLES
     */
    if (pdev->invalid_trigger) {
        if (!pdev->VsyncInterruptStatus) {
            if (pdev->CompModeSwitch != HWC_2_GLES) {
                pdev->CompModeSwitch = HWC_2_GLES;
                return HWC_2_GLES;
            }
        }
        return 0;
    }

    /*
     * There will be at least one composition call per one minute (because of time update)
     * To minimize the analysis overhead, just analyze it once in a second
     */
    if ((pdev->LastVsyncTimeStamp -pdev->LastModeSwitchTimeStamp) <  (VSYNC_INTERVAL * 60)) {
        return 0;
    }
    pdev->LastModeSwitchTimeStamp = pdev->LastVsyncTimeStamp;

    /*
     * FPS estimation.
     * If FPS is lower than HWC_FPS_TH, try to swiych the mode to GLES
     */
    if (pdev->setCallCnt < HWC_FPS_TH) {
        pdev->setCallCnt = 0;
        if (pdev->CompModeSwitch != HWC_2_GLES) {
            pdev->CompModeSwitch = HWC_2_GLES;
            return HWC_2_GLES;
        } else {
            return 0;
        }
    } else {
        pdev->setCallCnt = 0;
         if (pdev->CompModeSwitch == HWC_2_GLES) {
            pdev->CompModeSwitch = GLES_2_HWC;
            return GLES_2_HWC;
        } else {
            return 0;
        }
    }

    return 0;
}
#endif

static void exynos4_registerProcs(struct hwc_composer_device_1* dev,
        hwc_procs_t const* procs)
{
    struct exynos4_hwc_composer_device_1_t* pdev =
            (struct exynos4_hwc_composer_device_1_t*)dev;
    pdev->procs = procs;
}

static int exynos4_query(struct hwc_composer_device_1* dev, int what, int *value)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we support the background layer
        value[0] = 1;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = pdev->vsync_period;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int exynos4_eventControl(struct hwc_composer_device_1 *dev, int dpy,
        int event, int enabled)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        __u32 val = !!enabled;
#ifdef HWC_DYNAMIC_RECOMPOSITION
        pdev->VsyncInterruptStatus = val;
        pdev->vsyn_event_cnt++;
#endif
        int err = ioctl(pdev->fd, S3CFB_SET_VSYNC_INT, &val);
        if (err < 0) {
            ALOGE("vsync ioctl failed");
            return -errno;
        }

        return 0;
    }

    return -EINVAL;
}

static void handle_hdmi_uevent(struct exynos4_hwc_composer_device_1_t *pdev,
        const char *buff, int len)
{
#if USES_WFD
    if (pdev->wfd_hpd) {
        ALOGW("Hotplug is already used by Wifi Display");
        return;
    }
#endif

    const char *s = buff;
    s += strlen(s) + 1;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
            pdev->hdmi_hpd = atoi(s + strlen("SWITCH_STATE=")) == 1;

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    if (pdev->hdmi_hpd) {
        if (hdmi_get_config(pdev)) {
            ALOGE("Error reading HDMI configuration");
            pdev->hdmi_hpd = false;
            return;
        }

        pdev->hdmi_blanked = false;
#if defined(USES_CEC)
        start_cec(pdev);
    } else {
        CECClose();
        pdev->mCecFd = -1;
    }
#else
    }
#endif

    ALOGI("HDMI HPD changed to %s", pdev->hdmi_hpd ? "enabled" : "disabled");
    if (pdev->hdmi_hpd)
        ALOGI("HDMI Resolution changed to %dx%d", pdev->hdmi_h, pdev->hdmi_w);

    /* hwc_dev->procs is set right after the device is opened, but there is
     * still a race condition where a hotplug event might occur after the open
     * but before the procs are registered. */
    if (pdev->procs)
        pdev->procs->hotplug(pdev->procs, HWC_DISPLAY_EXTERNAL, pdev->hdmi_hpd);
}

static void handle_vsync_event(struct exynos4_hwc_composer_device_1_t *pdev)
{
    if (!pdev->procs)
        return;

    int err = lseek(pdev->vsync_fd, 0, SEEK_SET);
    if (err < 0) {
        ALOGE("error seeking to vsync timestamp: %s", strerror(errno));
        return;
    }

    char buf[4096];
    err = read(pdev->vsync_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

#ifdef FIMC_OUT_WA
    if (pdev->need_reqbufs) {
        if (pdev->wait_vsync_cnt > 0) {
            exynos_fimc_free_and_close(pdev->fimc[0].fimc);
            pdev->fimc[0].fimc = NULL;
            pdev->need_reqbufs = false;
            pdev->wait_vsync_cnt = 0;
        } else {
            pdev->wait_vsync_cnt++;
        }
    }
#endif

    errno = 0;
    uint64_t timestamp = strtoull(buf, NULL, 0);
    if (!errno)
        pdev->procs->vsync(pdev->procs, 0, timestamp);
#ifdef HWC_DYNAMIC_RECOMPOSITION
    if (!errno) {
        pdev->LastVsyncTimeStamp = timestamp;
        pdev->needInvalidate = exynos_getCompModeSwitch(pdev);
    }
#endif
}

#ifdef HWC_DYNAMIC_RECOMPOSITION
static void *hwc_vsync_stat_thread(void *data)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)data;
    int event_cnt = 0;

    while (true) {
        event_cnt = pdev->vsyn_event_cnt;
        /*
         * If VSYNC is diabled for more than 500ms, favor the 3D composition mode.
         * If all other conditions are met, mode will be switched to 3D composition.
         */
        usleep(500000);
        if ( (!pdev->VsyncInterruptStatus) && (event_cnt == pdev->vsyn_event_cnt)) {
            pdev->invalid_trigger = 1;
            if (exynos_getCompModeSwitch(pdev) == HWC_2_GLES) {
                pdev->invalid_trigger = 0;
                if ((pdev->procs) && (pdev->procs->invalidate))
                    pdev->procs->invalidate(pdev->procs);
            }
            pdev->invalid_trigger = 0;
        }
    }
    return NULL;
}
#endif

static void *hwc_vsync_thread(void *data)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)data;
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();

    char temp[4096];
    int err = read(pdev->vsync_fd, temp, sizeof(temp));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return NULL;
    }

#if defined(USES_CEC)
    struct pollfd fds[3];
#else
    struct pollfd fds[2];
#endif
    fds[0].fd = pdev->vsync_fd;
    fds[0].events = POLLPRI;
    fds[1].fd = uevent_get_fd();
    fds[1].events = POLLIN;
#if defined(USES_CEC)
    fds[2].fd = pdev->mCecFd;
    fds[2].events = POLLIN;
#endif

    while (true) {
#if defined(USES_CEC)
        int err;
        fds[2].fd = pdev->mCecFd;
        if (fds[2].fd > 0)
            err = poll(fds, 3, -1);
        else
            err = poll(fds, 2, -1);
#else
        int err = poll(fds, 2, -1);
#endif

        if (err > 0) {
            if (fds[0].revents & POLLPRI) {
                handle_vsync_event(pdev);
            }
            else if (fds[1].revents & POLLIN) {
                int len = uevent_next_event(uevent_desc,
                        sizeof(uevent_desc) - 2);

                bool hdmi = !strcmp(uevent_desc,
                        "change@/devices/virtual/switch/hdmi");
                if (hdmi)
                    handle_hdmi_uevent(pdev, uevent_desc, len);
#if defined(USES_CEC)
            } else if (pdev->hdmi_hpd && fds[2].revents & POLLIN) {
                handle_cec(pdev);
#endif
            }
        }
        else if (err == -1) {
            if (errno == EINTR)
                break;
            ALOGE("error in vsync thread: %s", strerror(errno));
        }
#ifdef HWC_DYNAMIC_RECOMPOSITION
    if (pdev->needInvalidate && (!pdev->invalidateStatus)) {
        pdev->needInvalidate = 0;
        pdev->invalidateStatus = 1;
        if ((pdev->procs) && (pdev->procs->invalidate))
            pdev->procs->invalidate(pdev->procs);
    }
#endif
    }

    return NULL;
}

static int exynos4_blank(struct hwc_composer_device_1 *dev, int disp, int blank)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (disp) {
    case HWC_DISPLAY_PRIMARY: {
        int fb_blank = blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK;
#ifdef SUPPORT_FIMC_LOCAL_PATH
        if (pdev->fimc_use && (fb_blank == FB_BLANK_POWERDOWN)) {
            if (pdev->fimc[FIMD_FIMC_IDX].fimc_mode == exynos4_fimc_map_t::FIMC_LOCAL) {
#ifdef FIMC_OUT_WA
                exynos_fimc_stop_exclusive(pdev->fimc[FIMD_FIMC_IDX].fimc);
                pdev->need_reqbufs = true;
                pdev->count_sameconfig = 0;
                pdev->fimc[FIMD_FIMC_IDX].fimc_mode = exynos4_fimc_map_t::FIMC_NONE;
#else
                exynos_fimc_destroy(pdev->fimc[FIMD_FIMC_IDX].fimc);
                pdev->fimc[FIMD_FIMC_IDX].fimc = NULL;
                pdev->fimc[FIMD_FIMC_IDX].fimc_mode = exynos4_fimc_map_t::FIMC_NONE;
#endif
            }
        }
#endif
        int err = ioctl(pdev->fd, FBIOBLANK, fb_blank);
        if (err < 0) {
            if (errno == EBUSY)
                ALOGI("%sblank ioctl failed (display already %sblanked)",
                        blank ? "" : "un", blank ? "" : "un");
            else
                ALOGE("%sblank ioctl failed: %s", blank ? "" : "un",
                        strerror(errno));
            return -errno;
        }
        break;
    }

    case HWC_DISPLAY_EXTERNAL:
#ifdef USES_WFD
        if (pdev->wfd_hpd) {
            pdev->wfd_blanked = !!blank;
            if (!blank)
                pdev->wfd_enabled = true;
             break;
        }
#endif
//////////////   COMMENTED OUT - sbrissen
/*        if (pdev->hdmi_hpd) {
            if (blank && !pdev->hdmi_blanked) {*/
                /*
                 * V4L2_CID_TV_BLANK becomes effective
                 * only if it is called before hdmi_disable() : STREAMOFF
                 */
                /*if (exynos_v4l2_s_ctrl(pdev->hdmi_layers[1].fd, V4L2_CID_TV_BLANK, 1) < 0) {
                    ALOGE("%s: s_ctrl(CID_TV_BLANK) failed %d", __func__, errno);
                    return -1;
                }
                hdmi_disable(pdev);
            }
            pdev->hdmi_blanked = !!blank;
        }
*/
        break;

    default:
        return -EINVAL;

    }

    return 0;
}

static void exynos4_dump(hwc_composer_device_1* dev, char *buff, int buff_len)
{
    if (buff_len <= 0)
        return;

    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    android::String8 result;

    result.appendFormat("  hdmi_enabled=%u\n", pdev->hdmi_enabled);
    if (pdev->hdmi_enabled)
        result.appendFormat("    w=%u, h=%u\n", pdev->hdmi_w, pdev->hdmi_h);
    result.append(
            "   type   |  handle  |  color   | blend | format |   position    |     size      | fimc \n"
            "----------+----------|----------+-------+--------+---------------+---------------------\n");
    //        8_______ | 8_______ | 8_______ | 5____ | 6_____ | [5____,5____] | [5____,5____] | 3__ \n"

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        struct s3c_fb_win_config &config = pdev->last_config[i];
        if (config.state == config.S3C_FB_WIN_STATE_DISABLED) {
            result.appendFormat(" %8s | %8s | %8s | %5s | %6s | %13s | %13s",
                    "OVERLAY", "-", "-", "-", "-", "-", "-");
        }
        else {
            if (config.state == config.S3C_FB_WIN_STATE_COLOR)
                result.appendFormat(" %8s | %8s | %8x | %5s | %6s", "COLOR",
                        "-", config.color, "-", "-");
            else
                result.appendFormat(" %8s | %8x | %8s | %5x | %6x",
                        pdev->last_fb_window == i ? "FB" : "OVERLAY",
                        intptr_t(pdev->last_handles[i]),
                        "-", config.blending, config.format);

            result.appendFormat(" | [%5d,%5d] | [%5u,%5u]", config.x, config.y,
                    config.w, config.h);
        }
        if (pdev->last_fimc_map[i].mode == exynos4_fimc_map_t::FIMC_NONE) {
            result.appendFormat(" | %3s", "-");
        } else {
            result.appendFormat(" | %3d",
                    AVAILABLE_FIMC_UNITS[pdev->last_fimc_map[i].idx]);
            if (pdev->last_fimc_map[i].mode == exynos4_fimc_map_t::FIMC_M2M)
                result.appendFormat(" | %10s","FIMC_M2M");
            else
                result.appendFormat(" | %10s","FIMC_LOCAL");
        }
        result.append("\n");
    }

    strlcpy(buff, result.string(), buff_len);
}

static int exynos4_getDisplayConfigs(struct hwc_composer_device_1 *dev,
        int disp, uint32_t *configs, size_t *numConfigs)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
               (struct exynos4_hwc_composer_device_1_t *)dev;

    if (*numConfigs == 0)
        return 0;

    if (disp == HWC_DISPLAY_PRIMARY) {
        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
#ifdef USES_WFD
        if (((!pdev->hdmi_hpd) && (!pdev->wfd_hpd)) ||
            ((pdev->hdmi_hpd) && (pdev->wfd_hpd)))
            return -EINVAL;

        if (pdev->hdmi_hpd) {
            int err = hdmi_get_config(pdev);
            if (err)
                return -EINVAL;
        }

        if (pdev->wfd_hpd)
            wfd_get_config(pdev);
#else
        if (!pdev->hdmi_hpd) {
            return -EINVAL;
        }

        int err = hdmi_get_config(pdev);
        if (err) {
            return -EINVAL;
        }
#endif
        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    }

    return -EINVAL;
}

static int32_t exynos4_fimd_attribute(struct exynos4_hwc_composer_device_1_t *pdev,
        const uint32_t attribute)
{
    switch(attribute) {
    case HWC_DISPLAY_VSYNC_PERIOD:
        return pdev->vsync_period;

    case HWC_DISPLAY_WIDTH:
        return pdev->xres;

    case HWC_DISPLAY_HEIGHT:
        return pdev->yres;

    case HWC_DISPLAY_DPI_X:
        return pdev->xdpi;

    case HWC_DISPLAY_DPI_Y:
        return pdev->ydpi;

    default:
        ALOGE("unknown display attribute %u", attribute);
        return -EINVAL;
    }
}

static int32_t exynos4_hdmi_attribute(struct exynos4_hwc_composer_device_1_t *pdev,
        const uint32_t attribute)
{
    switch(attribute) {
    case HWC_DISPLAY_VSYNC_PERIOD:
        return pdev->vsync_period;

    case HWC_DISPLAY_WIDTH:
        return pdev->hdmi_w;

    case HWC_DISPLAY_HEIGHT:
        return pdev->hdmi_h;

    case HWC_DISPLAY_DPI_X:
    case HWC_DISPLAY_DPI_Y:
        return 0; // unknown

    default:
        ALOGE("unknown display attribute %u", attribute);
        return -EINVAL;
    }
}

static int exynos4_getDisplayAttributes(struct hwc_composer_device_1 *dev,
        int disp, uint32_t config, const uint32_t *attributes, int32_t *values)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
                   (struct exynos4_hwc_composer_device_1_t *)dev;

    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        if (disp == HWC_DISPLAY_PRIMARY)
            values[i] = exynos4_fimd_attribute(pdev, attributes[i]);
        else if (disp == HWC_DISPLAY_EXTERNAL)
            values[i] = exynos4_hdmi_attribute(pdev, attributes[i]);
        else {
            ALOGE("unknown display type %u", disp);
            return -EINVAL;
        }
    }

    return 0;
}

static int exynos4_close(hw_device_t* device);

static int exynos4_open(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int ret;
    int refreshRate;
    int sw_fd;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return -EINVAL;
    }

    struct exynos4_hwc_composer_device_1_t *dev;
    dev = (struct exynos4_hwc_composer_device_1_t *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
            (const struct hw_module_t **)&dev->gralloc_module)) {
        ALOGE("failed to get gralloc hw module");
        ret = -EINVAL;
        goto err_get_module;
    }

    if (gralloc_open((const hw_module_t *)dev->gralloc_module,
            &dev->alloc_device)) {
        ALOGE("failed to open gralloc");
        ret = -EINVAL;
        goto err_get_module;
    }

    dev->fd = open("/dev/graphics/fb0", O_RDWR);
    if (dev->fd < 0) {
        ALOGE("failed to open framebuffer");
        ret = dev->fd;
        goto err_open_fb;
    }

    struct fb_var_screeninfo info;
    if (ioctl(dev->fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("FBIOGET_VSCREENINFO ioctl failed: %s", strerror(errno));
        ret = -errno;
        goto err_ioctl;
    }

    if (info.reserved[0] == 0 && info.reserved[1] == 0) {
        /* save physical lcd width, height to reserved[] */
        info.reserved[0] = info.xres;
        info.reserved[1] = info.yres;

        if (ioctl(dev->fd, FBIOPUT_VSCREENINFO, &info) == -1) {
            ALOGE("FBIOPUT_VSCREENINFO ioctl failed: %s", strerror(errno));
            ret = -errno;
            goto err_ioctl;
        }
    }

    /* restore physical lcd width, height from reserved[] */
    int lcd_xres, lcd_yres;
    lcd_xres = info.reserved[0];
    lcd_yres = info.reserved[1];

    refreshRate = 1000000000000LLU /
        (
         uint64_t( info.upper_margin + info.lower_margin + lcd_yres )
         * ( info.left_margin  + info.right_margin + lcd_xres )
         * info.pixclock
        );

    if (refreshRate == 0) {
        ALOGW("invalid refresh rate, assuming 60 Hz");
        refreshRate = 60;
    }

    dev->xres = lcd_xres;
    dev->yres = lcd_yres;
    dev->xdpi = 1000 * (lcd_xres * 25.4f) / info.width;
    dev->ydpi = 1000 * (lcd_yres * 25.4f) / info.height;
    dev->vsync_period  = 1000000000 / refreshRate;

    ALOGI("using\n"
          "xres         = %d px\n"
          "yres         = %d px\n"
          "width        = %d mm (%f dpi)\n"
          "height       = %d mm (%f dpi)\n"
          "refresh rate = %d Hz\n",
          dev->xres, dev->yres, info.width, dev->xdpi / 1000.0,
          info.height, dev->ydpi / 1000.0, refreshRate);

    for (size_t i = 0; i < NUM_FIMC_UNITS; i++)
        for (size_t j = 0; j < NUM_FIMC_DST_BUFS; j++)
            dev->fimc[i].dst_buf_fence[j] = -1;

#if !defined(HDMI_INCAPABLE)
    dev->hdmi_mixer0 = exynos_subdev_open_devname("s5p-mixer0", O_RDWR);
    if (dev->hdmi_mixer0 < 0) {
        ALOGE("failed to open hdmi mixer0 subdev");
        ret = dev->hdmi_mixer0;
        goto err_ioctl;
    }

    dev->hdmi_layers[0].id = 0;
    dev->hdmi_layers[0].fd = open("/dev/video16", O_RDWR);
    if (dev->hdmi_layers[0].fd < 0) {
        ALOGE("failed to open hdmi layer0 device");
        ret = dev->hdmi_layers[0].fd;
        goto err_mixer0;
    }

    dev->hdmi_layers[1].id = 1;
    dev->hdmi_layers[1].fd = open("/dev/video17", O_RDWR);
    if (dev->hdmi_layers[1].fd < 0) {
        ALOGE("failed to open hdmi layer1 device");
        ret = dev->hdmi_layers[1].fd;
        goto err_hdmi0;
    }
#endif
    dev->vsync_fd = open(VSYNC_DEV_NAME, O_RDONLY);
    if (dev->vsync_fd < 0) {
        ALOGE("failed to open vsync attribute");
        ret = dev->vsync_fd;
        goto err_hdmi1;
    }
#if !defined(HDMI_INCAPABLE)
    sw_fd = open("/sys/class/switch/hdmi/state", O_RDONLY);
    if (sw_fd) {
        char val;
        if (read(sw_fd, &val, 1) == 1 && val == '1') {
            dev->hdmi_hpd = true;
            if (hdmi_get_config(dev)) {
                ALOGE("Error reading HDMI configuration");
                dev->hdmi_hpd = false;
            }
        }
    }
#endif
    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = HWC_DEVICE_API_VERSION_1_1;
    dev->base.common.module = const_cast<hw_module_t *>(module);
    dev->base.common.close = exynos4_close;

    dev->base.prepare = exynos4_prepare;
    dev->base.set = exynos4_set;
    dev->base.eventControl = exynos4_eventControl;
    dev->base.blank = exynos4_blank;
    dev->base.query = exynos4_query;
    dev->base.registerProcs = exynos4_registerProcs;
    dev->base.dump = exynos4_dump;
    dev->base.getDisplayConfigs = exynos4_getDisplayConfigs;
    dev->base.getDisplayAttributes = exynos4_getDisplayAttributes;

    *device = &dev->base.common;

#ifdef HWC_SERVICES
    dev->mHWCService = android::ExynosHWCService::getExynosHWCService();
    dev->mHWCService->setExynosHWCCtx(dev);
    dev->mHdmiResolutionChanged = false;
    dev->mHdmiResolutionHandled = true;
#if defined(S3D_SUPPORT)
    dev->mS3DMode = S3D_MODE_DISABLED;
#endif
    dev->mHdmiPreset = HDMI_PRESET_DEFAULT;
    dev->mHdmiCurrentPreset = HDMI_PRESET_DEFAULT;
    dev->mUseSubtitles = false;
#endif

#if defined(USES_CEC)
    if (dev->hdmi_hpd)
        start_cec(dev);
    else
        dev->mCecFd = -1;
#endif

    ret = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (ret) {
        ALOGE("failed to start vsync thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.force_gpu", value, "0");
    dev->force_gpu = atoi(value);

    dev->force_mirror_mode = false;
    dev->hdmi_video_rotation = 0;
    dev->external_display_pause = false;
    dev->local_external_display_pause = false;
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    dev->use_blocking_layer = false;
    dev->composite_buf_index = 0;

    for (size_t i = 0; i < NUM_COMPOSITE_BUFFER_FOR_EXTERNAL; i++) {
        dev->composite_buffer_for_external[i] = NULL;
        dev->va_composite_buffer_for_external[i] = NULL;
    }
    dev->composite_buf_width  = 0;
    dev->composite_buf_height = 0;
    dev->already_mapped_vfb = false;

#ifdef USES_VFB
    struct fb_var_screeninfo var_info;
    struct s3cfb_user_window window;
    int vfb_fd;
    vfb_fd = open(EXYNOS4_U4A_FB_DEV, O_RDWR);

    if (vfb_fd <= 0) {
        ALOGD("%s::Failed to open window device (%s) : %s",
                __func__, strerror(errno), name);
        goto err_vsync;
    }

    if (ioctl(vfb_fd, FBIOGET_VSCREENINFO, &var_info) < 0) {
        ALOGD("FBIOGET_VSCREENINFO failed : %s",
                strerror(errno));
        goto err_vsync;
    }

    var_info.xres_virtual = 1920;
    var_info.yres_virtual = 1080 * 3;
    var_info.xres = 1920;
    var_info.yres = 1080;
    var_info.bits_per_pixel = 32;
    var_info.xoffset = 0;
    var_info.yoffset = 0;
    var_info.transp.length = 8;
    var_info.activate &= ~FB_ACTIVATE_MASK;
    var_info.activate |= FB_ACTIVATE_FORCE;

    if (ioctl(vfb_fd, FBIOPUT_VSCREENINFO, &(var_info)) < 0) {
        ALOGD("FBIOPUT_VSCREENINFO failed : %s",
                strerror(errno));
        goto err_vsync;
    }

    window.x = 0;
    window.y = 0;

    if (ioctl(vfb_fd, S3CFB_WIN_POSITION, &window) < 0) {
        ALOGD("%s::S3CFB_WIN_POSITION(%d, %d) fail",
                __func__, window.x, window.y);
        goto err_vsync;
    }

    dev->vfb_fd = vfb_fd;
    for (int i = 0; i < NUM_BUFFER_U4A; i++)
        dev->surface_fd_for_vfb[i] = -1;
#endif

    for (int i = 0; i < NUM_FB_TARGET; i++) {
        dev->fb_target_info[i].fd = -1;
        dev->fb_target_info[i].mapped_addr = NULL;
        dev->fb_target_info[i].map_size = 0;
    }
#endif

#ifdef HWC_DYNAMIC_RECOMPOSITION
    ret = pthread_create(&dev->vsync_stat_thread, NULL, hwc_vsync_stat_thread, dev);
    if (ret) {
        ALOGE("failed to start vsync_stat thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }
#endif

#ifdef FIMC_OUT_WA
    dev->need_reqbufs = false;
    dev->wait_vsync_cnt = 0;
#endif
    return 0;

err_vsync:
    close(dev->vsync_fd);
err_mixer0:
    close(dev->hdmi_mixer0);
err_hdmi1:
    close(dev->hdmi_layers[0].fd);
err_hdmi0:
    close(dev->hdmi_layers[1].fd);
err_ioctl:
    close(dev->fd);
err_open_fb:
    gralloc_close(dev->alloc_device);
err_get_module:
    free(dev);
    return ret;
}

static int exynos4_close(hw_device_t *device)
{
    struct exynos4_hwc_composer_device_1_t *dev =
            (struct exynos4_hwc_composer_device_1_t *)device;
    pthread_kill(dev->vsync_thread, SIGTERM);
    pthread_join(dev->vsync_thread, NULL);
#ifdef HWC_DYNAMIC_RECOMPOSITION
    pthread_kill(dev->vsync_stat_thread, SIGTERM);
    pthread_join(dev->vsync_stat_thread, NULL);
#endif
    for (size_t i = 0; i < NUM_FIMC_UNITS; i++)
        exynos4_cleanup_fimc_m2m(dev, i);
    gralloc_close(dev->alloc_device);
    close(dev->vsync_fd);
    close(dev->hdmi_mixer0);
    close(dev->hdmi_layers[0].fd);
    close(dev->hdmi_layers[1].fd);
    close(dev->fd);
#ifdef USES_WFD
    wfd_disable(dev);
#endif
#ifdef USE_GRALLOC_FLAG_FOR_HDMI
    for (size_t i = 0; i < NUM_COMPOSITE_BUFFER_FOR_EXTERNAL; i++) {
        ion_unmap((void *)dev->va_composite_buffer_for_external[i],
                dev->composite_buf_width * dev->composite_buf_height * 4);
        dev->va_composite_buffer_for_external[i] = NULL;
        dev->alloc_device->free(dev->alloc_device, dev->composite_buffer_for_external[i]);
        dev->composite_buffer_for_external[i] = NULL;
    }

    for (int i = 0; i < NUM_FB_TARGET; i++) {
        if (dev->fb_target_info[i].fd != -1) {
            ion_unmap((void *)dev->fb_target_info[i].mapped_addr, dev->fb_target_info[i].map_size);
            dev->fb_target_info[i].fd = -1;
            dev->fb_target_info[i].mapped_addr = NULL;
            dev->fb_target_info[i].map_size = 0;
        }
    }
#endif
    return 0;
}

static struct hw_module_methods_t exynos4_hwc_module_methods = {
    open: exynos4_open,
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: HWC_MODULE_API_VERSION_0_1,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Samsung exynos4 hwcomposer module",
        author: "Samsung LSI",
        methods: &exynos4_hwc_module_methods,
    }
};
