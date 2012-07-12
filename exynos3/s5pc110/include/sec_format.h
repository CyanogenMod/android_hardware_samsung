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

#ifndef _SEC_FORMAT_H_
#define _SEC_FORMAT_H_

/* enum related to pixel format */

enum {
    SAMSUNG_HAL_PIXEL_FORMAT_YCbCr_420_SP        = 0x100,
    SAMSUNG_HAL_PIXEL_FORMAT_YCbCr_420_P         = 0x101,
    SAMSUNG_HAL_PIXEL_FORMAT_YCbCr_420_I         = 0x102,
    SAMSUNG_HAL_PIXEL_FORMAT_CbYCrY_422_I        = 0x103,
    SAMSUNG_HAL_PIXEL_FORMAT_CbYCrY_420_I        = 0x104,
    SAMSUNG_HAL_PIXEL_FORMAT_YCbCr_422_P         = 0x105,
    SAMSUNG_HAL_PIXEL_FORMAT_YCrCb_422_SP        = 0x106,
    // support custom format for zero copy
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP = 0x110,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP = 0x111,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED = 0x112,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP = 0x113,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP = 0x114,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I  = 0x115,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I  = 0x116,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I = 0x117,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I = 0x118,
    SAMSUNG_HAL_PIXEL_FORMAT_CUSTOM_MAX
};

#endif
