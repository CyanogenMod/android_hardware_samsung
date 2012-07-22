/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * @file    csc_fimc.cpp
 *
 * @brief   csc_fimc use fimc1 to color space convertion
 *
 * @author  ShinWon Lee (shinwon.lee@samsung.com)
 *
 * @version 1.0
 *
 * @history
 *   2011.11.01 : Create
 */

#include <utils/Log.h>
#include <dlfcn.h>

#include "SecFimc.h"
#include "csc_fimc.h"

#define ALIGN(value, base) (((value) + (base) - 1) & ~((base) - 1))

unsigned int OMXtoHarPixelFomrat(OMX_COLOR_FORMATTYPE ColorFormat)
{
    unsigned int v4l2_format = 0;
    switch (ColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
        v4l2_format = HAL_PIXEL_FORMAT_YCbCr_420_P;
        break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
        v4l2_format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
        break;
    default:
        v4l2_format = HAL_PIXEL_FORMAT_YCbCr_420_P;
        break;
    }
    return v4l2_format;
}

/*
 * create and open fimc handle
 *
 * @return
 *   fimc handle
 */
void *csc_fimc_open()
{
    SecFimc *(*create_instance)();
    void (*destroy_instance)(void *);
    SecFimc *handle_fimc = NULL;
    void* hdl = NULL;

    hdl = dlopen("libfimc.so", RTLD_NOW);
    if (hdl == NULL) {
        ALOGE("%s:: load libfimc.so failed", __func__);
        return NULL;
    }

    create_instance = (SecFimc *(*)())dlsym(hdl, "create_instance");
    handle_fimc = (SecFimc *)create_instance();
    if (handle_fimc == NULL) {
        ALOGE("%s:: create handle_fimc failed", __func__);
        return NULL;
    }

    if (!handle_fimc->create(SecFimc::DEV_1, SecFimc::MODE_MULTI_BUF, 1)) {
        destroy_instance = (void (*)(void *))dlsym(hdl, "destroy_instance");
        destroy_instance(handle_fimc);
        ALOGE("%s:: create() failed", __func__);
        return NULL;
    }
    return (void *)handle_fimc;
}

/*
 * close and destroy fimc handle
 *
 * @param handle
 *   fimc handle[in]
 *
 * @return
 *   pass or fail
 */
CSC_FIMC_ERROR_CODE csc_fimc_close(void *handle)
{
    void (*destroy_instance)(void *);
    SecFimc *handle_fimc = (SecFimc *)handle;
    void* hdl = NULL;

    if (!handle_fimc->destroy()) {
        ALOGE("%s:: destroy() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    hdl = dlopen("libfimc.so", RTLD_NOW);
    if (hdl == NULL) {
        ALOGE("%s:: load libfimc.so failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    destroy_instance = (void (*)(void *))dlsym(hdl, "destroy_instance");
    destroy_instance(handle);

    return CSC_FIMC_RET_OK;
}

/*
 * convert color space nv12t to omxformat
 *
 * @param handle
 *   fimc handle[in]
 *
 * @param dst_addr
 *   y,u,v address of dst_addr[out]
 *
 * @param src_addr
 *   y,uv address of src_addr.Format is nv12t[in]
 *
 * @param width
 *   width of dst image[in]
 *
 * @param height
 *   height of dst image[in]
 *
 * @param omxformat
 *   omxformat of dst image[in]
 *
 * @return
 *   pass or fail
 */
CSC_FIMC_ERROR_CODE csc_fimc_convert_nv12t(
    void *handle,
    void **dst_addr,
    void **src_addr,
    unsigned int width,
    unsigned int height,
    OMX_COLOR_FORMATTYPE omxformat)
{
    int rotate_value = 0;

    SecFimc *handle_fimc = (SecFimc *)handle;

    unsigned int src_crop_x = 0;
    unsigned int src_crop_y = 0;
    unsigned int src_crop_width = width;
    unsigned int src_crop_height = height;

    unsigned int dst_crop_x = 0;
    unsigned int dst_crop_y = 0;
    unsigned int dst_crop_width = width;
    unsigned int dst_crop_height = height;

    unsigned int HarPixelformat = 0;
    HarPixelformat = OMXtoHarPixelFomrat(omxformat);

    // set post processor configuration
    if (!handle_fimc->setSrcParams(width, height, src_crop_x, src_crop_y,
                                   &src_crop_width, &src_crop_height,
                                   HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED)) {
        ALOGE("%s:: setSrcParms() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    if (!handle_fimc->setSrcAddr((unsigned int)src_addr[0],
                                 (unsigned int)src_addr[1],
                                 (unsigned int)src_addr[1],
                                 HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED)) {
        ALOGE("%s:: setSrcPhyAddr() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    if (!handle_fimc->setRotVal(rotate_value)) {
        ALOGE("%s:: setRotVal() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    if (!handle_fimc->setDstParams(width, height, dst_crop_x, dst_crop_y,
                                   &dst_crop_width, &dst_crop_height,
                                   HarPixelformat)) {
        ALOGE("%s:: setDstParams() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    switch (omxformat) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
        if (!handle_fimc->setDstAddr((unsigned int)(dst_addr[0]),
                                     (unsigned int)(dst_addr[1]),
                                     (unsigned int)(dst_addr[1]))) {
            ALOGE("%s:: setDstPhyAddr() failed", __func__);
            return CSC_FIMC_RET_FAIL;
        }
        break;
    case OMX_COLOR_FormatYUV420Planar:
    default:
        if (!handle_fimc->setDstAddr((unsigned int)(dst_addr[0]),
                                     (unsigned int)(dst_addr[1]),
                                     (unsigned int)(dst_addr[2]))) {
            ALOGE("%s:: setDstPhyAddr() failed", __func__);
            return CSC_FIMC_RET_FAIL;
        }
        break;
    }

    if (!handle_fimc->draw(0, 0)) {
        ALOGE("%s:: handleOneShot() failed", __func__);
        return CSC_FIMC_RET_FAIL;
    }

    return CSC_FIMC_RET_OK;
}
