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

/*
 * @file        exynos4_format_v4l2.c
 *
 * @brief       exynos format convertion for "exynos4 v4l2"
 *
 * @author      shinwon.lee(shinwon.lee@samsung.com)
 *
 * Revision History:
 * - 2012/03/06 : shinwon lee(shinwon.lee@samsung.com)
 *
 */


#ifndef __EXYNOS4_FORMAT_V4L2_H__
#define __EXYNOS4_FORMAT_V4L2_H__

//---------------------------------------------------------//
// Include
//---------------------------------------------------------//
#include <hardware/hardware.h>
#include "exynos_format.h"
#include <utils/Log.h>
#include <videodev2.h>
#include <videodev2_exynos_media.h>

int HAL_PIXEL_FORMAT_2_V4L2_PIX(
    int hal_pixel_format)
{
    int v4l2_pixel_format = -1;

    switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB32;
        break;

    case HAL_PIXEL_FORMAT_RGB_888:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB24;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB565;
        break;

    case HAL_PIXEL_FORMAT_BGRA_8888:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB32;
        break;

    case HAL_PIXEL_FORMAT_RGBA_5551:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB555X;
        break;

    case HAL_PIXEL_FORMAT_RGBA_4444:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB444;
        break;

    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        v4l2_pixel_format = V4L2_PIX_FMT_YUV420M;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV61;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV12M;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_YUYV;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_P:
        v4l2_pixel_format = V4L2_PIX_FMT_YUV422P;
        break;

    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_UYVY;
        break;

    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV16;
        break;

    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV21;
        break;

    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        v4l2_pixel_format = V4L2_PIX_FMT_NV12MT;
        break;

   case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_YVYU;
        break;

   case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_VYUY;
        break;

    default:
        ALOGE("%s::unmatched HAL_PIXEL_FORMAT color_space(0x%x)\n",
                __func__, hal_pixel_format);
        break;
    }

    return v4l2_pixel_format;
}

int V4L2_PIX_2_HAL_PIXEL_FORMAT(
    int v4l2_pixel_format)
{
    int hal_pixel_format = -1;

    switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_RGB32:
        hal_pixel_format = HAL_PIXEL_FORMAT_RGBA_8888;
        break;

    case V4L2_PIX_FMT_RGB24:
        hal_pixel_format = HAL_PIXEL_FORMAT_RGB_888;
        break;

    case V4L2_PIX_FMT_RGB565:
        hal_pixel_format = HAL_PIXEL_FORMAT_RGB_565;
        break;

    case V4L2_PIX_FMT_BGR32:
        hal_pixel_format = HAL_PIXEL_FORMAT_BGRA_8888;
        break;

    case V4L2_PIX_FMT_RGB555X:
        hal_pixel_format = HAL_PIXEL_FORMAT_RGBA_5551;
        break;

    case V4L2_PIX_FMT_RGB444:
        hal_pixel_format = HAL_PIXEL_FORMAT_RGBA_4444;
        break;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
        hal_pixel_format = HAL_PIXEL_FORMAT_YCbCr_420_P;
        break;

    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YVU420M:
         hal_pixel_format = HAL_PIXEL_FORMAT_YV12;
         break;

    case V4L2_PIX_FMT_NV16:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP;
        break;

    case V4L2_PIX_FMT_NV12:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP;
        break;

    case V4L2_PIX_FMT_YUYV:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I;
        break;

    case V4L2_PIX_FMT_YUV422P:
        hal_pixel_format = HAL_PIXEL_FORMAT_YCbCr_422_P;
        break;

    case V4L2_PIX_FMT_UYVY:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I;
        break;

    case V4L2_PIX_FMT_NV21:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP;
        break;

    case V4L2_PIX_FMT_NV12MT:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED;
        break;

    case V4L2_PIX_FMT_NV61:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP;
        break;

    case V4L2_PIX_FMT_YVYU:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I;
        break;

    case V4L2_PIX_FMT_VYUY:
        hal_pixel_format = HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I;
        break;

    default:
        ALOGE("%s::unmatched V4L2_PIX color_space(%d)\n",
                __func__, v4l2_pixel_format);
        break;
    }

    return hal_pixel_format;
}

unsigned int FRAME_SIZE(
    int hal_pixel_format,
    int width,
    int height)
{
    unsigned int frame_size = 0;
    unsigned int size       = 0;

    switch (hal_pixel_format) {
    // 16bpp
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
        frame_size = GET_16BPP_FRAME_SIZE(width, height);
        break;

    // 24bpp
    case HAL_PIXEL_FORMAT_RGB_888:
        frame_size = GET_24BPP_FRAME_SIZE(width, height);
        break;

    // 32bpp
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
        frame_size = GET_32BPP_FRAME_SIZE(width, height);
        break;

    // 12bpp
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_I:
    case HAL_PIXEL_FORMAT_CbYCrY_420_I:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        size = width * height;
        frame_size = size + ((size >> 2) << 1);
        break;

    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        frame_size =   ALIGN_TO_8KB(ALIGN_TO_128B(width) * ALIGN_TO_32B(height))
                     + ALIGN_TO_8KB(ALIGN_TO_128B(width) * ALIGN_TO_32B(height >> 1));
        break;

    // 16bpp
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_422_P:
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        frame_size = GET_16BPP_FRAME_SIZE(width, height);
        break;

    default:
        ALOGD("%s::no matching source colorformat(0x%x), w(%d), h(%d) fail\n",
                __func__, hal_pixel_format, width, height);
        break;
    }

    return frame_size;
}

#endif
