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
 * @file    swscaler.c
 * @brief
 * @author  MinGu Jeon (mingu85.jeon@samsung.com)
 * @version 1.0
 * @history
 *   2012.05.09 : Create
 */
#include <stdlib.h>
#include <stdio.h>
#include "swscaler.h"

long get_result_time(struct timeval *start, struct timeval *end)
{
    long sec, time, usec;
    sec = end->tv_sec - start->tv_sec;
    if (end->tv_usec >= start->tv_usec) {
        usec = end->tv_usec - start->tv_usec;
    } else {
        usec = end->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }
    time = sec * 1000000 + usec;

    return time;
}

void SW_Scale_up_Y(unsigned int srcImageWidth, unsigned int srcImageHeight, unsigned int dstImageWidth, unsigned int dstImageHeight, unsigned int MainHorRatio, unsigned int MainVerRatio, unsigned char *total, unsigned char *total2)
{
    SW_Scale_up_Y_NEON(srcImageWidth, srcImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, total, total2);
}

void SW_Scale_up_CbCr(unsigned int srcImageWidth, unsigned int srcImageHeight, unsigned int dstImageWidth, unsigned int dstImageHeight, unsigned int MainHorRatio, unsigned int MainVerRatio, unsigned char *total, unsigned char *total2)
{
    SW_Scale_up_CbCr_NEON(srcImageWidth, srcImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, total, total2);
}

void SW_Scale_up(unsigned int srcImageWidth, unsigned int srcImageHeight, unsigned int dstImageWidth, unsigned int dstImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned char *dstY, unsigned char *dstCbCr)
{
    unsigned int MainHorRatio, MainVerRatio;
    MainHorRatio = (srcImageWidth << 14) / (dstImageWidth);
    MainVerRatio = (srcImageHeight << 14) / (dstImageHeight);

    if ((srcImageWidth == dstImageWidth) && (srcImageHeight == dstImageHeight)) {
        SW_Memcpy_NEON(srcImageWidth, srcImageHeight, srcY, srcCbCr, dstY, dstCbCr);
    } else {
        SW_Scale_up_Y(srcImageWidth, srcImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, srcY, dstY);
        SW_Scale_up_CbCr(srcImageWidth, srcImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, srcCbCr, dstCbCr);
    }
}
/*
 *  SW_Scale_up_crop(cropImageWidth, cropImageHeight, dstImageWidth, dstImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned char *dstY, unsigned char *dstCbCr)
 *  Crop the image based on the middle of image.
 *  Calculation address are needed before calling this function.
 *  @param cropImageWidth, cropImageHeight
 *      crop size of Width and Height
 *
 *  @param dstImageWidth, dstImageHeight
 *      Scaling size of result image
 *
 *  @param srcY, srcCbCr
 *      Address of Y and CbCr fileds in source image
 *
 *  @param dstY, dstCbCr
 *      Address of Y and CbCr fileds in result image
 */

void SW_Scale_up_crop(unsigned int cropImageWidth, unsigned int cropImageHeight, unsigned int dstImageWidth, unsigned int dstImageHeight, unsigned char *srcY, unsigned char *srcCbCr, unsigned char *dstY, unsigned char *dstCbCr)
{
    unsigned int MainHorRatio, MainVerRatio;
    MainHorRatio = (cropImageWidth << 14) / (dstImageWidth);
    MainVerRatio = (cropImageHeight << 14) / (dstImageHeight);
    if ((cropImageWidth == dstImageWidth) && (cropImageHeight == dstImageHeight)) {
            SW_Memcpy_NEON(cropImageWidth, cropImageHeight, srcY, srcCbCr, dstY, dstCbCr);
    } else {
        SW_Scale_up_Y(dstImageWidth, dstImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, srcY, dstY);
        SW_Scale_up_CbCr(dstImageWidth, dstImageHeight, dstImageWidth, dstImageHeight, MainHorRatio, MainVerRatio, srcCbCr, dstCbCr);
    }
}
