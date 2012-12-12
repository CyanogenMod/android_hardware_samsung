/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
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
 * @file    swscaler.h
 * @brief
 * @author  MinGu Jeon (mingu85.jeon@samsung.com)
 * @version 1.0
 * @history
 *   2012.05.09 : Create
 */
#ifndef _LIB_SWSCALE_H
#define _LIB_SWSCALE_H

#include <stdlib.h>
#include <stdio.h>
#include <utils/Log.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif
void SW_Scale_up(unsigned int srcImageWidth, unsigned int srcImageHeight, unsigned int dstImageWidth, unsigned int dstImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned char *dstY, unsigned char *dstCbCr);
void SW_Scale_up_crop(unsigned int srcImageWidth, unsigned int
        srcImageHeight, unsigned int dstImageWidth, unsigned int
        dstImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned
        char *dstY, unsigned char *dstCbCr);
void SW_Memcpy_NEON(unsigned int cropImageWidth, unsigned int  cropImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned char *dstY, unsigned char *dstCbCr);
#ifdef __cplusplus
}
#endif

#endif
