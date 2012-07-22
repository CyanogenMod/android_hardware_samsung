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

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <EGL/egl.h>

#include "SecHWCUtils.h"

#include "gralloc_priv.h"
#ifdef HWC_HWOVERLAY
#include <GLES/gl.h>
#endif
#if defined(BOARD_USES_HDMI)
#include "SecHdmiClient.h"
#include "SecTVOutService.h"

#include "SecHdmi.h"

//#define CHECK_EGL_FPS
#ifdef CHECK_EGL_FPS
extern void check_fps();
#endif

static int lcd_width, lcd_height;
static int prev_usage = 0;

#define CHECK_TIME_DEBUG 0
#define SUPPORT_AUTO_UI_ROTATE
#endif
int testRenderNum =0;

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Samsung S5PC21X hwcomposer module",
        author: "SAMSUNG",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_t const* l) {
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

void calculate_rect(struct hwc_win_info_t *win, hwc_layer_t *cur,
        sec_rect *rect)
{
    rect->x = cur->displayFrame.left;
    rect->y = cur->displayFrame.top;
    rect->w = cur->displayFrame.right - cur->displayFrame.left;
    rect->h = cur->displayFrame.bottom - cur->displayFrame.top;

    if (rect->x < 0) {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres;
        else
            rect->w = rect->w + rect->x;
        rect->x = 0;
    } else {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres - rect->x;
    }
    if (rect->y < 0) {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres;
        else
            rect->h = rect->h + rect->y;
        rect->y = 0;
    } else {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres - rect->y;
    }
}

static int set_src_dst_img_rect(hwc_layer_t *cur,
        struct hwc_win_info_t *win,
        struct sec_img *src_img,
        struct sec_img *dst_img,
        struct sec_rect *src_rect,
        struct sec_rect *dst_rect,
        int win_idx)
{
    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    sec_rect rect;

    /* 1. Set src_img from prev_handle */
    src_img->f_w     = prev_handle->width;
    src_img->f_h     = prev_handle->height;
    src_img->w       = prev_handle->width;
    src_img->h       = prev_handle->height;
    src_img->format  = prev_handle->format;
    src_img->base    = (uint32_t)prev_handle->base;
    src_img->offset  = prev_handle->offset;
    src_img->mem_id  = prev_handle->fd;
    src_img->paddr  = prev_handle->paddr;
    src_img->usage  = prev_handle->usage;
    src_img->uoffset  = prev_handle->uoffset;
    src_img->voffset  = prev_handle->voffset;

    src_img->mem_type = HWC_VIRT_MEM_TYPE;

    switch (src_img->format) {
    case HAL_PIXEL_FORMAT_YV12:             /* To support video editor */
    case HAL_PIXEL_FORMAT_YCbCr_420_P:      /* To support SW codec     */
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        src_img->f_w = (src_img->f_w + 15) & ~15;
        src_img->f_h = (src_img->f_h + 1) & ~1;
        break;
    default:
        src_img->f_w = src_img->w;
        src_img->f_h = src_img->h;
        break;
    }

    /* 2. Set dst_img from window(lcd) */
    calculate_rect(win, cur, &rect);
    dst_img->f_w = win->lcd_info.xres;
    dst_img->f_h = win->lcd_info.yres;
    dst_img->w = rect.w;
    dst_img->h = rect.h;

    switch (win->lcd_info.bits_per_pixel) {
    case 32:
        dst_img->format = HAL_PIXEL_FORMAT_RGBX_8888;
        break;
    default:
        dst_img->format = HAL_PIXEL_FORMAT_RGB_565;
        break;
    }

    dst_img->base     = win->addr[win->buf_index];
    dst_img->offset   = 0;
    dst_img->mem_id   = 0;
    dst_img->mem_type = HWC_PHYS_MEM_TYPE;

    /* 3. Set src_rect(crop rect) */
    if (cur->displayFrame.left < 0) {
        src_rect->x =
            (0 - cur->displayFrame.left)
            *(src_img->w)
            /(cur->displayFrame.right - cur->displayFrame.left + 1);
        if (cur->displayFrame.right + 1 > win->lcd_info.xres) {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left + 1) -
                src_rect->x -
                (cur->displayFrame.right - win->lcd_info.xres)
                *(src_img->w)
                /(cur->displayFrame.right - cur->displayFrame.left + 1);
        } else {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left + 1) -
                src_rect->x;
        }
    } else {
        src_rect->x = cur->sourceCrop.left;
        if (cur->displayFrame.right + 1 > win->lcd_info.xres) {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left + 1) -
                src_rect->x -
                (cur->displayFrame.right - win->lcd_info.xres)
                *(src_img->w)
                /(cur->displayFrame.right - cur->displayFrame.left + 1);
        } else {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left + 1);
        }
    }
    if (cur->displayFrame.top < 0) {
        src_rect->y =
            (0 - cur->displayFrame.top)
            *(src_img->h)
            /(cur->displayFrame.bottom - cur->displayFrame.top + 1);
        if (cur->displayFrame.bottom + 1 > win->lcd_info.yres) {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top + 1) -
                src_rect->y -
                (cur->displayFrame.bottom - win->lcd_info.yres)
                *(src_img->h)
                /(cur->displayFrame.bottom - cur->displayFrame.top + 1);
        } else {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top + 1) -
                src_rect->y;
        }
    } else {
        src_rect->y = cur->sourceCrop.top;
        if (cur->displayFrame.bottom + 1 > win->lcd_info.yres) {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top + 1) -
                src_rect->y -
                (cur->displayFrame.bottom - win->lcd_info.yres)
                *(src_img->h)
                /(cur->displayFrame.bottom - cur->displayFrame.top + 1);
        } else {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top + 1);
        }
    }

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "crop information()::"
            "sourceCrop left(%d),top(%d),right(%d),bottom(%d),"
            "src_rect x(%d),y(%d),w(%d),h(%d),"
            "prev_handle w(%d),h(%d)",
            cur->sourceCrop.left,
            cur->sourceCrop.top,
            cur->sourceCrop.right,
            cur->sourceCrop.bottom,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            prev_handle->width, prev_handle->height);

    src_rect->x = SEC_MAX(src_rect->x, 0);
    src_rect->y = SEC_MAX(src_rect->y, 0);
    src_rect->w = SEC_MAX(src_rect->w, 0);
    src_rect->w = SEC_MIN(src_rect->w, prev_handle->width);
    src_rect->h = SEC_MAX(src_rect->h, 0);
    src_rect->h = SEC_MIN(src_rect->h, prev_handle->height);

    /* 4. Set dst_rect(fb or lcd)
     *    fimc dst image will be stored from left top corner
     */
    dst_rect->x = 0;
    dst_rect->y = 0;
    dst_rect->w = win->rect_info.w;
    dst_rect->h = win->rect_info.h;

    /* Summery */
    SEC_HWC_Log(HWC_LOG_DEBUG,
            "set_src_dst_img_rect()::"
            "SRC w(%d),h(%d),f_w(%d),f_h(%d),fmt(0x%x),"
            "base(0x%x),offset(%d),paddr(0x%X),mem_id(%d),mem_type(%d)=>\r\n"
            "   DST w(%d),h(%d),f(0x%x),base(0x%x),"
            "offset(%d),mem_id(%d),mem_type(%d),"
            "rot(%d),win_idx(%d)"
            "   SRC_RECT x(%d),y(%d),w(%d),h(%d)=>"
            "DST_RECT x(%d),y(%d),w(%d),h(%d)",
            src_img->w, src_img->h, src_img->f_w, src_img->f_h, src_img->format,
            src_img->base, src_img->offset, src_img->paddr, src_img->mem_id, src_img->mem_type,
            dst_img->w, dst_img->h,  dst_img->format, dst_img->base,
            dst_img->offset, dst_img->mem_id, dst_img->mem_type,
            cur->transform, win_idx,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);

    return 0;
}

static int get_hwc_compos_decision(hwc_layer_t* cur, int iter, int win_cnt)
{
    if(cur->flags & HWC_SKIP_LAYER  || !cur->handle) {
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::is_skip_layer  %d  cur->handle %x ",
                __func__, cur->flags & HWC_SKIP_LAYER, cur->handle);

        return HWC_FRAMEBUFFER;
    }

    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    int compositionType = HWC_FRAMEBUFFER;

    if (iter == 0) {
    /* check here....if we have any resolution constraints */
        if (((cur->sourceCrop.right - cur->sourceCrop.left + 1) < 16) ||
            ((cur->sourceCrop.bottom - cur->sourceCrop.top + 1) < 8))
            return compositionType;

        if ((cur->transform == HAL_TRANSFORM_ROT_90) ||
            (cur->transform == HAL_TRANSFORM_ROT_270)) {
            if (((cur->displayFrame.right - cur->displayFrame.left + 1) < 4) ||
                ((cur->displayFrame.bottom - cur->displayFrame.top + 1) < 8))
                return compositionType;
        } else if (((cur->displayFrame.right - cur->displayFrame.left + 1) < 8) ||
                   ((cur->displayFrame.bottom - cur->displayFrame.top + 1) < 4)) {
            return compositionType;
        }

        switch (prev_handle->format) {
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
            compositionType = HWC_OVERLAY;
            break;
        case HAL_PIXEL_FORMAT_YV12:                 /* YCrCb_420_P */
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            if ((prev_handle->usage & GRALLOC_USAGE_HWC_HWOVERLAY) &&
                 (cur->blending == HWC_BLENDING_NONE))
                compositionType = HWC_OVERLAY;
            else
                compositionType = HWC_FRAMEBUFFER;
            break;
        default:
            compositionType = HWC_FRAMEBUFFER;
            break;
        }
    }

#ifdef SUB_TITLES_HWC
    else if ((win_cnt > 0) &&
            (prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP)) {
        switch (prev_handle->format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_RGB_888:
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_RGBA_5551:
        case HAL_PIXEL_FORMAT_RGBA_4444:
            compositionType = HWC_OVERLAY;
            break;
        default:
            compositionType = HWC_FRAMEBUFFER;
            break;
        }

        SEC_HWC_Log(HWC_LOG_DEBUG, "2nd iter###%s:: compositionType %d bpp %d"
                " format %x src[%d %d %d %d] dst[%d %d %d %d] srcImg[%d %d]",
                __func__, compositionType, prev_handle->bpp,
                prev_handle->format,
                cur->sourceCrop.left, cur->sourceCrop.right,
                cur->sourceCrop.top, cur->sourceCrop.bottom,
                cur->displayFrame.left, cur->displayFrame.right,
                cur->displayFrame.top, cur->displayFrame.bottom,
                prev_handle->width, prev_handle->height);
    }
#endif

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s::compositionType(%d)=>0:FB,1:OVERLAY \r\n"
            "   format(0x%x),magic(0x%x),flags(%d),size(%d),offset(%d)"
            "b_addr(0x%x),usage(%d),w(%d),h(%d),bpp(%d)",
            "get_hwc_compos_decision()", compositionType,
            prev_handle->format, prev_handle->magic, prev_handle->flags,
            prev_handle->size, prev_handle->offset, prev_handle->base,
            prev_handle->usage, prev_handle->width, prev_handle->height,
            prev_handle->bpp);

    return  compositionType;
}

static void reset_win_rect_info(hwc_win_info_t *win)
{
    win->rect_info.x = 0;
    win->rect_info.y = 0;
    win->rect_info.w = 0;
    win->rect_info.h = 0;
    return;
}


static int assign_overlay_window(struct hwc_context_t *ctx, hwc_layer_t *cur,
        int win_idx, int layer_idx)
{
    struct hwc_win_info_t   *win;
    sec_rect   rect;
    int ret = 0;

    if (NUM_OF_WIN <= win_idx)
        return -1;

    win = &ctx->win[win_idx];

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: left(%d),top(%d),right(%d),bottom(%d),transform(%d)"
            "lcd_info.xres(%d),lcd_info.yres(%d)",
            "++assign_overlay_window()",
            cur->displayFrame.left, cur->displayFrame.top,
            cur->displayFrame.right, cur->displayFrame.bottom, cur->transform,
            win->lcd_info.xres, win->lcd_info.yres);

    calculate_rect(win, cur, &rect);

    if ((rect.x != win->rect_info.x) || (rect.y != win->rect_info.y) ||
        (rect.w != win->rect_info.w) || (rect.h != win->rect_info.h)){
        win->rect_info.x = rect.x;
        win->rect_info.y = rect.y;
        win->rect_info.w = rect.w;
        win->rect_info.h = rect.h;
            //turnoff the window and set the window position with new conf...
        if (window_set_pos(win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_set_pos is failed : %s",
                    __func__, strerror(errno));
            ret = -1;
        }
        ctx->layer_prev_buf[win_idx] = 0;
    }

    win->layer_index = layer_idx;
    win->status = HWC_WIN_RESERVED;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: win_x %d win_y %d win_w %d win_h %d  lay_idx %d win_idx %d\n",
            "--assign_overlay_window()",
            win->rect_info.x, win->rect_info.y, win->rect_info.w,
            win->rect_info.h, win->layer_index, win_idx );

    return 0;
}

static int hwc_prepare(hwc_composer_device_t *dev, hwc_layer_list_t* list)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int overlay_win_cnt = 0;
    int compositionType = 0;
    int ret;

    //if geometry is not changed, there is no need to do any work here
    if (!list || (!(list->flags & HWC_GEOMETRY_CHANGED)))
        return 0;

    //all the windows are free here....
    for (int i = 0 ; i < NUM_OF_WIN; i++) {
        ctx->win[i].status = HWC_WIN_FREE;
        ctx->win[i].buf_index = 0;
    }

    ctx->num_of_hwc_layer = 0;
    ctx->num_of_fb_layer = 0;
    ctx->num_2d_blit_layer = 0;

    for (int i = 0; i < list->numHwLayers ; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];

        if (overlay_win_cnt < NUM_OF_WIN) {
            compositionType = get_hwc_compos_decision(cur, 0, overlay_win_cnt);

            if (compositionType == HWC_FRAMEBUFFER) {
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->num_of_fb_layer++;
            } else {
                ret = assign_overlay_window(ctx, cur, overlay_win_cnt, i);
                if (ret != 0) {
                    ALOGE("assign_overlay_window fail, change to frambuffer");
                    cur->compositionType = HWC_FRAMEBUFFER;
                    ctx->num_of_fb_layer++;
                    continue;
                }

                cur->compositionType = HWC_OVERLAY;
                cur->hints = HWC_HINT_CLEAR_FB;
                overlay_win_cnt++;
                ctx->num_of_hwc_layer++;
            }
        } else {
            cur->compositionType = HWC_FRAMEBUFFER;
            ctx->num_of_fb_layer++;
        }
    }

#ifdef SUB_TITLES_HWC
    for (int i = 0; i < list->numHwLayers ; i++) {
        if (overlay_win_cnt < NUM_OF_WIN) {
            hwc_layer_t* cur = &list->hwLayers[i];
            if (get_hwc_compos_decision(cur, 1, overlay_win_cnt) == HWC_OVERLAY) {
                ret = assign_overlay_window(ctx, cur, overlay_win_cnt, i);
                if (ret == 0) {
                    cur->compositionType = HWC_OVERLAY;
                    cur->hints = HWC_HINT_CLEAR_FB;
                    overlay_win_cnt++;
                    ctx->num_of_hwc_layer++;
                    ctx->num_of_fb_layer--;
                    ctx->num_2d_blit_layer = 1;
                }
            }
        }
        else
            break;
    }
#endif

#if defined(BOARD_USES_HDMI)
    android::SecHdmiClient *mHdmiClient = android::SecHdmiClient::getInstance();
    mHdmiClient->setHdmiHwcLayer(ctx->num_of_hwc_layer);
#endif

    if (list->numHwLayers != (ctx->num_of_fb_layer + ctx->num_of_hwc_layer))
        SEC_HWC_Log(HWC_LOG_DEBUG,
                "%s:: numHwLayers %d num_of_fb_layer %d num_of_hwc_layer %d ",
                __func__, list->numHwLayers, ctx->num_of_fb_layer,
                ctx->num_of_hwc_layer);

    if (overlay_win_cnt < NUM_OF_WIN) {
        //turn off the free windows
        for (int i = overlay_win_cnt; i < NUM_OF_WIN; i++) {
            window_hide(&ctx->win[i]);
            reset_win_rect_info(&ctx->win[i]);
        }
    }

    return 0;
}

static int hwc_set(hwc_composer_device_t *dev,
                   hwc_display_t dpy,
                   hwc_surface_t sur,
                   hwc_layer_list_t* list)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    int skipped_window_mask = 0;
    hwc_layer_t* cur;
    struct hwc_win_info_t   *win;
    int ret;
    int pmem_phyaddr;
    static int egl_check;
    int egl_run = 0;
    struct sec_img src_img;
    struct sec_img dst_img;
    struct sec_rect src_work_rect;
    struct sec_rect dst_work_rect;

    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(dst_img));
    memset(&src_work_rect, 0, sizeof(src_work_rect));
    memset(&dst_work_rect, 0, sizeof(dst_work_rect));

#if defined(BOARD_USES_HDMI)
    int skip_hdmi_rendering = 0;
    int rotVal = 0;
#endif

    if (!list) {
        //turn off the all windows
        for (int i = 0; i < NUM_OF_WIN; i++) {
            window_hide(&ctx->win[i]);
            reset_win_rect_info(&ctx->win[i]);
            ctx->win[i].status = HWC_WIN_FREE;
        }
        ctx->num_of_hwc_layer = 0;

        if (sur == NULL && dpy == NULL)
            return HWC_EGL_ERROR;
    }

    if(ctx->num_of_hwc_layer > NUM_OF_WIN)
        ctx->num_of_hwc_layer = NUM_OF_WIN;

    //compose hardware layers here
    for (int i = 0; i < ctx->num_of_hwc_layer - ctx->num_2d_blit_layer; i++) {
        win = &ctx->win[i];
        if (win->status == HWC_WIN_RESERVED) {
            cur = &list->hwLayers[win->layer_index];

            if (cur->compositionType == HWC_OVERLAY) {
                if (ctx->layer_prev_buf[i] == (uint32_t)cur->handle) {
                    /*
                     * In android platform, all the graphic buffer are at least
                     * double buffered (2 or more) this buffer is already rendered.
                     * It is the redundant src buffer for FIMC rendering.
                     */
                    ALOGD("SKIP FIMC rendering for Layer%d", win->layer_index);
#if defined(BOARD_USES_HDMI)
                    skip_hdmi_rendering = 1;
#endif
                    continue;
                }
                ctx->layer_prev_buf[i] = (uint32_t)cur->handle;
                // initialize the src & dist context for fimc
                set_src_dst_img_rect(cur, win, &src_img, &dst_img,
                                &src_work_rect, &dst_work_rect, i);

                ret = runFimc(ctx,
                            &src_img, &src_work_rect,
                            &dst_img, &dst_work_rect,
                            cur->transform);

                if (ret < 0) {
                    SEC_HWC_Log(HWC_LOG_ERROR, "%s::runFimc fail : ret=%d\n",
                                __func__, ret);
                    skipped_window_mask |= (1 << i);
                    continue;
                }

                window_pan_display(win);

                win->buf_index = (win->buf_index + 1) % NUM_OF_WIN_BUF;
                if (win->power_state == 0)
                    window_show(win);
            } else {
                SEC_HWC_Log(HWC_LOG_ERROR,
                        "%s:: error : layer %d compositionType should have been"
                        " HWC_OVERLAY ", __func__, win->layer_index);
                skipped_window_mask |= (1 << i);
                continue;
            }
        } else {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : window status should have "
                    "been HWC_WIN_RESERVED by now... ", __func__);
             skipped_window_mask |= (1 << i);
             continue;
        }
    }

#ifdef SUB_TITLES_HWC
    if (ctx->num_2d_blit_layer) {
        g2d_rect srcRect;
        g2d_rect dstRect;

        win = &ctx->win[ctx->num_of_hwc_layer - 1];
        cur = &list->hwLayers[win->layer_index];
        set_src_dst_g2d_rect(cur, win, &srcRect, &dstRect);
        ret = runG2d(ctx, &srcRect,  &dstRect,
                        cur->transform);
         if (ret < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::runG2d fail : ret=%d\n",
                    __func__, ret);
                   skipped_window_mask |= (1 << (ctx->num_of_hwc_layer - 1));
                   goto g2d_error;
         }

         window_pan_display(win);

         win->buf_index = (win->buf_index + 1) % NUM_OF_WIN_BUF;
         if (win->power_state == 0)
             window_show(win);
    }

g2d_error:
#endif

    if (skipped_window_mask) {
        //turn off the free windows
        for (int i = 0; i < NUM_OF_WIN; i++) {
            if (skipped_window_mask & (1 << i)) {
                window_hide(&ctx->win[i]);
                reset_win_rect_info(&ctx->win[i]);
            }
        }
    }

    if (0 < ctx->num_of_fb_layer) {
#ifdef CHECK_EGL_FPS
        check_fps();
#endif
#ifdef HWC_HWOVERLAY
        unsigned char pixels[4];
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#endif
        egl_check = 1;
        egl_run = 1;
    } else {
        if (egl_check == 1) {
            egl_check = 0;
            egl_run = 1;
        }
    }

    if (egl_run == 1) {
        EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
        if (!sucess)
            return HWC_EGL_ERROR;
    }

#if defined(BOARD_USES_HDMI)
    android::SecHdmiClient *mHdmiClient = android::SecHdmiClient::getInstance();

    if (skip_hdmi_rendering == 1)
        return 0;

    if (list == NULL) {
        // Don't display unnecessary image
        mHdmiClient->setHdmiEnable(0);
        return 0;
    } else {
        mHdmiClient->setHdmiEnable(1);
    }

#ifdef SUPPORT_AUTO_UI_ROTATE
    cur = &list->hwLayers[0];

    if (cur->transform == HAL_TRANSFORM_ROT_90 || cur->transform == HAL_TRANSFORM_ROT_270)
        mHdmiClient->setHdmiRotate(270, ctx->num_of_hwc_layer);
    else
        mHdmiClient->setHdmiRotate(0, ctx->num_of_hwc_layer);
#endif

    // To support S3D video playback (automatic TV mode change to 3D mode)
    if (ctx->num_of_hwc_layer == 1) {
        if (src_img.usage != prev_usage)
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE);    // V4L2_STD_1080P_60

        if ((src_img.usage & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
            (src_img.usage & GRALLOC_USAGE_PRIVATE_SBS_RL))
            mHdmiClient->setHdmiResolution(7209601);    // V4L2_STD_TVOUT_720P_60_SBS_HALF
        else if ((src_img.usage & GRALLOC_USAGE_PRIVATE_TB_LR) ||
            (src_img.usage & GRALLOC_USAGE_PRIVATE_TB_RL))
            mHdmiClient->setHdmiResolution(1080924);    // V4L2_STD_TVOUT_1080P_24_TB

        prev_usage = src_img.usage;
    } else {
        if ((prev_usage & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_SBS_RL) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_TB_LR) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_TB_RL))
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE);    // V4L2_STD_1080P_60

        prev_usage = 0;
    }

    if (ctx->num_of_hwc_layer == 1) {
        if ((src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED)||
                (src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP)) {
            ADDRS * addr = (ADDRS *)(src_img.base);

            mHdmiClient->blit2Hdmi(src_img.w, src_img.h,
                                    src_img.format,
                                    (unsigned int)addr->addr_y, (unsigned int)addr->addr_cbcr, (unsigned int)addr->addr_cbcr,
                                    0, 0,
                                    android::SecHdmiClient::HDMI_MODE_VIDEO,
                                    ctx->num_of_hwc_layer);
        } else if ((src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_P) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YV12)) {
            mHdmiClient->blit2Hdmi(src_img.w, src_img.h,
                                    src_img.format,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_rgb_y,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_cb,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_cr,
                                    0, 0,
                                    android::SecHdmiClient::HDMI_MODE_VIDEO,
                                    ctx->num_of_hwc_layer);
        } else {
            ALOGE("%s: Unsupported format = %d", __func__, src_img.format);
        }
    }
#endif
    return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int ret = 0;
    int i;
    if (ctx) {
        if (destroyFimc(&ctx->fimc) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyFimc fail", __func__);
            ret = -1;
        }
#ifdef SUB_TITLES_HWC
        if (destroyG2d(&ctx->g2d) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyG2d() fail", __func__);
            ret = -1;
        }
#endif
        if (destroyMem(&ctx->s3c_mem) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyMem fail", __func__);
            ret = -1;
        }

#ifdef USE_HW_PMEM
        if (destroyPmem(&ctx->sec_pmem) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyPmem fail", __func__);
            ret = -1;
        }
#endif
        for (i = 0; i < NUM_OF_WIN; i++) {
            if (window_close(&ctx->win[i]) < 0)
                SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
        }

        free(ctx);
    }
    return ret;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;
    struct hwc_win_info_t   *win;

    if (strcmp(name, HWC_HARDWARE_COMPOSER))
        return  -EINVAL;

    struct hwc_context_t *dev;
    dev = (hwc_context_t*)malloc(sizeof(*dev));

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version = 0;
    dev->device.common.module = const_cast<hw_module_t*>(module);
    dev->device.common.close = hwc_device_close;

    dev->device.prepare = hwc_prepare;
    dev->device.set = hwc_set;

    *device = &dev->device.common;

    //initializing
    memset(&(dev->fimc),    0, sizeof(s5p_fimc_t));
    memset(&(dev->s3c_mem), 0, sizeof(struct s3c_mem_t));
#ifdef USE_HW_PMEM
    memset(&(dev->sec_pmem),    0, sizeof(sec_pmem_t));
#endif
     /* open WIN0 & WIN1 here */
     for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_open(&(dev->win[i]), i)  < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "%s:: Failed to open window %d device ", __func__, i);
             status = -EINVAL;
             goto err;
        }
     }

    if (window_get_global_lcd_info(dev->win[0].fd, &dev->lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::window_get_global_lcd_info is failed : %s",
                __func__, strerror(errno));
        status = -EINVAL;
        goto err;
    }

#if defined(BOARD_USES_HDMI)
    lcd_width   = dev->lcd_info.xres;
    lcd_height  = dev->lcd_info.yres;
#endif

    /* initialize the window context */
    for (int i = 0; i < NUM_OF_WIN; i++) {
        win = &dev->win[i];
        memcpy(&win->lcd_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));
        memcpy(&win->var_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));

        win->rect_info.x = 0;
        win->rect_info.y = 0;
        win->rect_info.w = win->var_info.xres;
        win->rect_info.h = win->var_info.yres;

       if (window_set_pos(win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_set_pos is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }

        if (window_get_info(win, i) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_get_info is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }

    }

#ifdef USE_HW_PMEM
    if (createPmem(&dev->sec_pmem, PMEM_SIZE) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::initPmem(%d) fail", __func__, PMEM_SIZE);
    }
#endif

    if (createMem(&dev->s3c_mem, 0, 0) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::createMem() fail (size=0)", __func__);
        status = -EINVAL;
        goto err;
    }

    //create PP
    if (createFimc(&dev->fimc) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::creatFimc() fail", __func__);
        status = -EINVAL;
        goto err;
    }

#ifdef SUB_TITLES_HWC
   if (createG2d(&dev->g2d) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::createG2d() fail", __func__);
        status = -EINVAL;
        goto err;
    }
#endif

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: hwc_device_open: SUCCESS", __func__);

    return 0;

err:
    if (destroyFimc(&dev->fimc) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyFimc() fail", __func__);
#ifdef SUB_TITLES_HWC
     if (destroyG2d(&dev->g2d) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyG2d() fail", __func__);
#endif
    if (destroyMem(&dev->s3c_mem) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyMem() fail", __func__);

#ifdef USE_HW_PMEM
    if (destroyPmem(&dev->sec_pmem) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyPmem() fail", __func__);
#endif

    for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_close(&dev->win[i]) < 0)
            SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
    }

    return status;
}
