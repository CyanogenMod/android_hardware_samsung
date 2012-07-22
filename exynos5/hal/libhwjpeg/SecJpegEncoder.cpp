/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/Log.h>

#include "SecJpegEncoder.h"

static const char ExifAsciiPrefix[] = { 0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0 };

#define JPEG_ERROR_ALOG(fmt,...)

#define JPEG_MAIN_DUMP  (0)
#define JPEG_THUMB_DUMP (0)

SecJpegEncoder::SecJpegEncoder()
{
    m_flagCreate = false;
    m_jpegMain = NULL;
    m_jpegThumb = NULL;
    m_thumbnailW = 0;
    m_thumbnailH = 0;
    m_pThumbInputBuffer = NULL;
    m_pThumbOutputBuffer = NULL;
#ifdef JPEG_WA_FOR_PAGEFAULT
    m_pJpegInputBuffer = 0;
    m_pExtInBuf = NULL;
    m_iInBufSize = 0;
#endif // JPEG_WA_FOR_PAGEFAULT
    m_ionJpegClient = 0;;
    m_ionThumbInBuffer = 0;
    m_ionThumbOutBuffer = 0;
#ifdef JPEG_WA_FOR_PAGEFAULT
    m_ionJpegInBuffer = 0;;
#endif // JPEG_WA_FOR_PAGEFAULT
}

SecJpegEncoder::~SecJpegEncoder()
{
    if (m_flagCreate == true) {
        this->destroy();
    }
}

bool SecJpegEncoder::flagCreate(void)
{
    return m_flagCreate;
}

int SecJpegEncoder::create(void)
{
    int ret = ERROR_NONE;
    if (m_flagCreate == true) {
        return ERROR_ALREADY_CREATE;
    }

    if (m_jpegMain == NULL) {
        m_jpegMain = new SecJpegEncoderHal;

        if (m_jpegMain == NULL) {
            JPEG_ERROR_ALOG("ERR(%s):Cannot create SecJpegEncoderHal class\n", __func__);
            return ERROR_CANNOT_CREATE_SEC_JPEG_ENC_HAL;
        }

        ret = m_jpegMain->create();
        if (ret) {
            return ret;
        }

        ret = m_jpegMain->setCache(JPEG_CACHE_ON);

        if (ret) {
            m_jpegMain->destroy();
            return ret;
        }
    }

    m_flagCreate = true;

    return ERROR_NONE;
}

int SecJpegEncoder::destroy(void)
{
    if (m_flagCreate == false) {
        return ERROR_ALREADY_DESTROY;
    }

    if (m_jpegMain != NULL) {
        m_jpegMain->destroy();
        delete m_jpegMain;
        m_jpegMain = NULL;
    }

    if (m_jpegThumb != NULL) {
        int iSize = sizeof(char)*m_thumbnailW*m_thumbnailH*4;

#ifdef JPEG_WA_FOR_PAGEFAULT
        iSize += JPEG_WA_BUFFER_SIZE;

        freeJpegIonMemory(m_ionJpegClient, &m_ionJpegInBuffer, &m_pJpegInputBuffer, m_iInBufSize);
#endif //JPEG_WA_FOR_PAGEFAULT

        freeJpegIonMemory(m_ionJpegClient, &m_ionThumbInBuffer, &m_pThumbInputBuffer, iSize);
        freeJpegIonMemory(m_ionJpegClient, &m_ionThumbOutBuffer, &m_pThumbOutputBuffer, iSize);

        if (m_ionJpegClient != 0) {
            ion_client_destroy(m_ionJpegClient);
            m_ionJpegClient = 0;
        }

        m_jpegThumb->destroy();
        delete m_jpegThumb;
        m_jpegThumb = NULL;
    }

    m_flagCreate = false;
    m_thumbnailW = 0;
    m_thumbnailH = 0;
    return ERROR_NONE;
}

int SecJpegEncoder::setSize(int w, int h)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    return m_jpegMain->setSize(w, h);
}


int SecJpegEncoder::setQuality(int quality)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    return m_jpegMain->setQuality(quality);
}

int SecJpegEncoder::setColorFormat(int colorFormat)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    return m_jpegMain->setColorFormat(colorFormat);
}

int SecJpegEncoder::setJpegFormat(int jpegFormat)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    return m_jpegMain->setJpegFormat(jpegFormat);
}

int SecJpegEncoder::updateConfig(void)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    return m_jpegMain->updateConfig();
}

char *SecJpegEncoder::getInBuf(int *input_size)
{
    if (m_flagCreate == false) {
        return NULL;
    }

    int inSize = 0;
    char *inBuf = *(m_jpegMain->getInBuf(&inSize));

    if (inBuf == NULL) {
        JPEG_ERROR_ALOG("%s::Fail to JPEG input buffer!!\n", __func__);
        return NULL;
    }

    *input_size = inSize;

    return inBuf;
}

char *SecJpegEncoder::getOutBuf(int *output_size)
{
    if (m_flagCreate == false) {
        return NULL;
    }

    int outSize = 0;
    char *outBuf = m_jpegMain->getOutBuf(&outSize);

    if (outBuf == NULL) {
        JPEG_ERROR_ALOG("%s::Fail to JPEG input buffer!!\n", __func__);
        return NULL;
    }

    *output_size = outSize;

    return outBuf;
}

int  SecJpegEncoder::setInBuf(char *buf, int size)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    if (buf == NULL) {
        return ERROR_BUFFR_IS_NULL;
    }

    if (size<=0) {
        return ERROR_BUFFER_TOO_SMALL;
    }

    int ret = ERROR_NONE;

#ifdef JPEG_WA_FOR_PAGEFAULT
    size += JPEG_WA_BUFFER_SIZE;

    freeJpegIonMemory(m_ionJpegClient, &m_ionJpegInBuffer, &m_pJpegInputBuffer, size);

    if (m_ionJpegClient == 0) {
        m_ionJpegClient = ion_client_create();
        if (m_ionJpegClient < 0) {
            JPEG_ERROR_ALOG("[%s]src ion client create failed, value = %d\n", __func__, size);
            m_ionJpegClient = 0;
            return ret;
        }
    }

    ret = allocJpegIonMemory(m_ionJpegClient, &m_ionJpegInBuffer, &m_pJpegInputBuffer, size);
    if (ret != ERROR_NONE) {
        return ret;
    }

    ret = m_jpegMain->setInBuf(&m_pJpegInputBuffer, &size);
    if (ret) {
        JPEG_ERROR_ALOG("%s::Fail to JPEG input buffer!!\n", __func__);
        return ret;
    }
    m_iInBufSize = size;

    m_pExtInBuf = buf;

#else // NO JPEG_WA_FOR_PAGEFAULT
    ret = m_jpegMain->setInBuf(&buf, &size);
    if (ret) {
        JPEG_ERROR_ALOG("%s::Fail to JPEG input buffer!!\n", __func__);
        return ret;
    }
#endif // JPEG_WA_FOR_PAGEFAULT

    return ERROR_NONE;
}

int  SecJpegEncoder::setOutBuf(char *buf, int size)
{
    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

    if (buf == NULL) {
        return ERROR_BUFFR_IS_NULL;
    }

    if (size<=0) {
        return ERROR_BUFFER_TOO_SMALL;
    }

    int ret = ERROR_NONE;
    ret = m_jpegMain->setOutBuf(buf, size);
    if (ret) {
        JPEG_ERROR_ALOG("%s::Fail to JPEG output buffer!!\n", __func__);
        return ret;
    }

    return ERROR_NONE;
}

int SecJpegEncoder::encode(int *size, exif_attribute_t *exifInfo)
{
    int ret = ERROR_NONE;
    unsigned char *exifOut = NULL;

    if (m_flagCreate == false) {
        return ERROR_NOT_YET_CREATED;
    }

#ifdef JPEG_WA_FOR_PAGEFAULT
    memcpy(m_pJpegInputBuffer, m_pExtInBuf, m_iInBufSize-JPEG_WA_BUFFER_SIZE);
#endif // JPEG_WA_FOR_PAGEFAULT

    ret = m_jpegMain->encode();
    if (ret) {
        JPEG_ERROR_ALOG("encode failed\n");
        return ret;
    }

    int iJpegSize = m_jpegMain->getJpegSize();

    if (iJpegSize<=0) {
        JPEG_ERROR_ALOG("%s:: output_size is too small(%d)!!\n", __func__, iJpegSize);
        return ERROR_OUT_BUFFER_SIZE_TOO_SMALL;
    }

    int iOutputSize = 0;
    char *pcJpegBuffer = m_jpegMain->getOutBuf(&iOutputSize);

    if (pcJpegBuffer == NULL) {
        JPEG_ERROR_ALOG("%s::buffer is null!!\n", __func__);
        return ERROR_OUT_BUFFER_CREATE_FAIL;
    }

    if (exifInfo != NULL) {
        unsigned int thumbLen, exifLen;

        unsigned int bufSize = 0;
        if (exifInfo->enableThumb) {
            if (encodeThumbnail(&thumbLen)) {
                bufSize = EXIF_FILE_SIZE;
                exifInfo->enableThumb = false;
            } else {
                if (thumbLen > EXIF_LIMIT_SIZE) {
                    bufSize = EXIF_FILE_SIZE;
                    exifInfo->enableThumb = false;
                }
                else {
                    bufSize = EXIF_FILE_SIZE + thumbLen;
                }
            }
        } else {
            bufSize = EXIF_FILE_SIZE;
            exifInfo->enableThumb = false;
        }

        exifOut = new unsigned char[bufSize];
        if (exifOut == NULL) {
            JPEG_ERROR_ALOG("%s::Failed to allocate for exifOut", __func__);
            delete[] exifOut;
            return ERROR_EXIFOUT_ALLOC_FAIL;
        }
        memset(exifOut, 0, bufSize);

        if (makeExif (exifOut, exifInfo, &exifLen)) {
            JPEG_ERROR_ALOG("%s::Failed to make EXIF", __func__);
            delete[] exifOut;
            return ERROR_MAKE_EXIF_FAIL;
        }

        if (exifLen <= EXIF_LIMIT_SIZE) {
            memmove(pcJpegBuffer+exifLen+2, pcJpegBuffer+2, iJpegSize - 2);
            memcpy(pcJpegBuffer+2, exifOut, exifLen);
            iJpegSize += exifLen;
        }

        delete[] exifOut;
    }

    *size = iJpegSize;

    return ERROR_NONE;
}

int SecJpegEncoder::makeExif (unsigned char *exifOut,
                              exif_attribute_t *exifInfo,
                              unsigned int *size,
                              bool useMainbufForThumb)
{
    unsigned char *pCur, *pApp1Start, *pIfdStart, *pGpsIfdPtr, *pNextIfdOffset;
    unsigned int tmp, LongerTagOffest = 0, exifSizeExceptThumb;
    pApp1Start = pCur = exifOut;

    //2 Exif Identifier Code & TIFF Header
    pCur += 4;  // Skip 4 Byte for APP1 marker and length
    unsigned char ExifIdentifierCode[6] = { 0x45, 0x78, 0x69, 0x66, 0x00, 0x00 };
    memcpy(pCur, ExifIdentifierCode, 6);
    pCur += 6;

    /* Byte Order - little endian, Offset of IFD - 0x00000008.H */
    unsigned char TiffHeader[8] = { 0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00 };
    memcpy(pCur, TiffHeader, 8);
    pIfdStart = pCur;
    pCur += 8;

    //2 0th IFD TIFF Tags
    if (exifInfo->enableGps)
        tmp = NUM_0TH_IFD_TIFF;
    else
        tmp = NUM_0TH_IFD_TIFF - 1;

    memcpy(pCur, &tmp, NUM_SIZE);
    pCur += NUM_SIZE;

    LongerTagOffest += 8 + NUM_SIZE + tmp*IFD_SIZE + OFFSET_SIZE;

    writeExifIfd(&pCur, EXIF_TAG_IMAGE_WIDTH, EXIF_TYPE_LONG,
                 1, exifInfo->width);
    writeExifIfd(&pCur, EXIF_TAG_IMAGE_HEIGHT, EXIF_TYPE_LONG,
                 1, exifInfo->height);
    writeExifIfd(&pCur, EXIF_TAG_MAKE, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->maker) + 1, exifInfo->maker, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_MODEL, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->model) + 1, exifInfo->model, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_ORIENTATION, EXIF_TYPE_SHORT,
                 1, exifInfo->orientation);
    writeExifIfd(&pCur, EXIF_TAG_SOFTWARE, EXIF_TYPE_ASCII,
                 strlen((char *)exifInfo->software) + 1, exifInfo->software, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_YCBCR_POSITIONING, EXIF_TYPE_SHORT,
                 1, exifInfo->ycbcr_positioning);
    writeExifIfd(&pCur, EXIF_TAG_EXIF_IFD_POINTER, EXIF_TYPE_LONG,
                 1, LongerTagOffest);
    if (exifInfo->enableGps) {
        pGpsIfdPtr = pCur;
        pCur += IFD_SIZE;   // Skip a ifd size for gps IFD pointer
    }

    pNextIfdOffset = pCur;  // Skip a offset size for next IFD offset
    pCur += OFFSET_SIZE;

    //2 0th IFD Exif Private Tags
    pCur = pIfdStart + LongerTagOffest;

    tmp = NUM_0TH_IFD_EXIF;
    memcpy(pCur, &tmp , NUM_SIZE);
    pCur += NUM_SIZE;

    LongerTagOffest += NUM_SIZE + NUM_0TH_IFD_EXIF*IFD_SIZE + OFFSET_SIZE;

    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_TIME, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->exposure_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_FNUMBER, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->fnumber, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_PROGRAM, EXIF_TYPE_SHORT,
                 1, exifInfo->exposure_program);
    writeExifIfd(&pCur, EXIF_TAG_ISO_SPEED_RATING, EXIF_TYPE_SHORT,
                 1, exifInfo->iso_speed_rating);
    writeExifIfd(&pCur, EXIF_TAG_EXIF_VERSION, EXIF_TYPE_UNDEFINED,
                 4, exifInfo->exif_version);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME_ORG, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_DATE_TIME_DIGITIZE, EXIF_TYPE_ASCII,
                 20, exifInfo->date_time, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_SHUTTER_SPEED, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->shutter_speed, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_APERTURE, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->aperture, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_BRIGHTNESS, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->brightness, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_BIAS, EXIF_TYPE_SRATIONAL,
                 1, (rational_t *)&exifInfo->exposure_bias, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_MAX_APERTURE, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->max_aperture, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_METERING_MODE, EXIF_TYPE_SHORT,
                 1, exifInfo->metering_mode);
    writeExifIfd(&pCur, EXIF_TAG_FLASH, EXIF_TYPE_SHORT,
                 1, exifInfo->flash);
    writeExifIfd(&pCur, EXIF_TAG_FOCAL_LENGTH, EXIF_TYPE_RATIONAL,
                 1, &exifInfo->focal_length, &LongerTagOffest, pIfdStart);
    char code[8] = { 0x00, 0x00, 0x00, 0x49, 0x49, 0x43, 0x53, 0x41 };
    int commentsLen = strlen((char *)exifInfo->user_comment) + 1;
    memmove(exifInfo->user_comment + sizeof(code), exifInfo->user_comment, commentsLen);
    memcpy(exifInfo->user_comment, code, sizeof(code));
    writeExifIfd(&pCur, EXIF_TAG_USER_COMMENT, EXIF_TYPE_UNDEFINED,
                 commentsLen + sizeof(code), exifInfo->user_comment, &LongerTagOffest, pIfdStart);
    writeExifIfd(&pCur, EXIF_TAG_COLOR_SPACE, EXIF_TYPE_SHORT,
                 1, exifInfo->color_space);
    writeExifIfd(&pCur, EXIF_TAG_PIXEL_X_DIMENSION, EXIF_TYPE_LONG,
                 1, exifInfo->width);
    writeExifIfd(&pCur, EXIF_TAG_PIXEL_Y_DIMENSION, EXIF_TYPE_LONG,
                 1, exifInfo->height);
    writeExifIfd(&pCur, EXIF_TAG_EXPOSURE_MODE, EXIF_TYPE_LONG,
                 1, exifInfo->exposure_mode);
    writeExifIfd(&pCur, EXIF_TAG_WHITE_BALANCE, EXIF_TYPE_LONG,
                 1, exifInfo->white_balance);
    writeExifIfd(&pCur, EXIF_TAG_SCENCE_CAPTURE_TYPE, EXIF_TYPE_LONG,
                 1, exifInfo->scene_capture_type);
    tmp = 0;
    memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
    pCur += OFFSET_SIZE;

    //2 0th IFD GPS Info Tags
    if (exifInfo->enableGps) {
        writeExifIfd(&pGpsIfdPtr, EXIF_TAG_GPS_IFD_POINTER, EXIF_TYPE_LONG,
                     1, LongerTagOffest); // GPS IFD pointer skipped on 0th IFD

        pCur = pIfdStart + LongerTagOffest;

        if (exifInfo->gps_processing_method[0] == 0) {
            // don't create GPS_PROCESSING_METHOD tag if there isn't any
            tmp = NUM_0TH_IFD_GPS - 1;
        } else {
            tmp = NUM_0TH_IFD_GPS;
        }
        memcpy(pCur, &tmp, NUM_SIZE);
        pCur += NUM_SIZE;

        LongerTagOffest += NUM_SIZE + tmp*IFD_SIZE + OFFSET_SIZE;

        writeExifIfd(&pCur, EXIF_TAG_GPS_VERSION_ID, EXIF_TYPE_BYTE,
                     4, exifInfo->gps_version_id);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LATITUDE_REF, EXIF_TYPE_ASCII,
                     2, exifInfo->gps_latitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LATITUDE, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_latitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LONGITUDE_REF, EXIF_TYPE_ASCII,
                     2, exifInfo->gps_longitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_LONGITUDE, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_longitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_ALTITUDE_REF, EXIF_TYPE_BYTE,
                     1, exifInfo->gps_altitude_ref);
        writeExifIfd(&pCur, EXIF_TAG_GPS_ALTITUDE, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->gps_altitude, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_GPS_TIMESTAMP, EXIF_TYPE_RATIONAL,
                     3, exifInfo->gps_timestamp, &LongerTagOffest, pIfdStart);
        tmp = strlen((char*)exifInfo->gps_processing_method);
        if (tmp > 0) {
            if (tmp > 100) {
                tmp = 100;
            }
            unsigned char tmp_buf[100+sizeof(ExifAsciiPrefix)];
            memcpy(tmp_buf, ExifAsciiPrefix, sizeof(ExifAsciiPrefix));
            memcpy(&tmp_buf[sizeof(ExifAsciiPrefix)], exifInfo->gps_processing_method, tmp);
            writeExifIfd(&pCur, EXIF_TAG_GPS_PROCESSING_METHOD, EXIF_TYPE_UNDEFINED,
                         tmp+sizeof(ExifAsciiPrefix), tmp_buf, &LongerTagOffest, pIfdStart);
        }
        writeExifIfd(&pCur, EXIF_TAG_GPS_DATESTAMP, EXIF_TYPE_ASCII,
                     11, exifInfo->gps_datestamp, &LongerTagOffest, pIfdStart);
        tmp = 0;
        memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
        pCur += OFFSET_SIZE;
    }

    //2 1th IFD TIFF Tags
    char *thumbBuf = NULL;
    unsigned int thumbSize = 0;

    if (useMainbufForThumb) {
        if (m_jpegMain) {
            thumbBuf = m_jpegMain->getOutBuf((int *)&thumbSize);
            thumbSize = m_jpegMain->getJpegSize();
        }
    } else {
        if (m_jpegThumb) {
            thumbBuf = m_jpegThumb->getOutBuf((int *)&thumbSize);
            thumbSize = m_jpegThumb->getJpegSize();
        }
    }

    if (exifInfo->enableThumb && (thumbBuf != NULL) && (thumbSize != 0)) {
        exifSizeExceptThumb = tmp = LongerTagOffest;
        memcpy(pNextIfdOffset, &tmp, OFFSET_SIZE);  // NEXT IFD offset skipped on 0th IFD

        pCur = pIfdStart + LongerTagOffest;

        tmp = NUM_1TH_IFD_TIFF;
        memcpy(pCur, &tmp, NUM_SIZE);
        pCur += NUM_SIZE;

        LongerTagOffest += NUM_SIZE + NUM_1TH_IFD_TIFF*IFD_SIZE + OFFSET_SIZE;

        writeExifIfd(&pCur, EXIF_TAG_IMAGE_WIDTH, EXIF_TYPE_LONG,
                     1, exifInfo->widthThumb);
        writeExifIfd(&pCur, EXIF_TAG_IMAGE_HEIGHT, EXIF_TYPE_LONG,
                     1, exifInfo->heightThumb);
        writeExifIfd(&pCur, EXIF_TAG_COMPRESSION_SCHEME, EXIF_TYPE_SHORT,
                     1, exifInfo->compression_scheme);
        writeExifIfd(&pCur, EXIF_TAG_ORIENTATION, EXIF_TYPE_SHORT,
                     1, exifInfo->orientation);
        writeExifIfd(&pCur, EXIF_TAG_X_RESOLUTION, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->x_resolution, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_Y_RESOLUTION, EXIF_TYPE_RATIONAL,
                     1, &exifInfo->y_resolution, &LongerTagOffest, pIfdStart);
        writeExifIfd(&pCur, EXIF_TAG_RESOLUTION_UNIT, EXIF_TYPE_SHORT,
                     1, exifInfo->resolution_unit);
        writeExifIfd(&pCur, EXIF_TAG_JPEG_INTERCHANGE_FORMAT, EXIF_TYPE_LONG,
                     1, LongerTagOffest);
        writeExifIfd(&pCur, EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LEN, EXIF_TYPE_LONG,
                     1, thumbSize);

        tmp = 0;
        memcpy(pCur, &tmp, OFFSET_SIZE); // next IFD offset
        pCur += OFFSET_SIZE;

        memcpy(pIfdStart + LongerTagOffest,
               thumbBuf, thumbSize);
        LongerTagOffest += thumbSize;
        if (LongerTagOffest > EXIF_LIMIT_SIZE) {
            LongerTagOffest = exifSizeExceptThumb;
            tmp = 0;
            memcpy(pNextIfdOffset, &tmp, OFFSET_SIZE);  // NEXT IFD offset skipped on 0th IFD
        }
    } else {
        tmp = 0;
        memcpy(pNextIfdOffset, &tmp, OFFSET_SIZE);  // NEXT IFD offset skipped on 0th IFD
    }

    unsigned char App1Marker[2] = { 0xff, 0xe1 };
    memcpy(pApp1Start, App1Marker, 2);
    pApp1Start += 2;

    *size = 10 + LongerTagOffest;
    tmp = *size - 2;    // APP1 Maker isn't counted
    unsigned char size_mm[2] = {(tmp >> 8) & 0xFF, tmp & 0xFF};
    memcpy(pApp1Start, size_mm, 2);

    return ERROR_NONE;
}

/*
 * private member functions
*/
inline void SecJpegEncoder::writeExifIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned int value)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, &value, 4);
    *pCur += 4;
}

inline void SecJpegEncoder::writeExifIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned char *pValue)
{
    char buf[4] = { 0,};

    memcpy(buf, pValue, count);
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, buf, 4);
    *pCur += 4;
}

inline void SecJpegEncoder::writeExifIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             unsigned char *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, count);
    *offset += count;
}

inline void SecJpegEncoder::writeExifIfd(unsigned char **pCur,
                                             unsigned short tag,
                                             unsigned short type,
                                             unsigned int count,
                                             rational_t *pValue,
                                             unsigned int *offset,
                                             unsigned char *start)
{
    memcpy(*pCur, &tag, 2);
    *pCur += 2;
    memcpy(*pCur, &type, 2);
    *pCur += 2;
    memcpy(*pCur, &count, 4);
    *pCur += 4;
    memcpy(*pCur, offset, 4);
    *pCur += 4;
    memcpy(start + *offset, pValue, 8 * count);
    *offset += 8 * count;
}

int SecJpegEncoder::m_scaleDownYuv422(char *srcBuf, unsigned int srcW, unsigned int srcH,
                                           char *dstBuf, unsigned int dstW, unsigned int dstH)
{
    int step_x, step_y;
    int iXsrc, iXdst;
    int x, y, src_y_start_pos, dst_pos, src_pos;

    if (dstW & 0x01 || dstH & 0x01) {
        return ERROR_INVALID_SCALING_WIDTH_HEIGHT;
    }

    step_x = srcW / dstW;
    step_y = srcH / dstH;

    unsigned int srcWStride = srcW * 2;
    unsigned int stepXStride = step_x * 2;

    dst_pos = 0;
    for (unsigned int y = 0; y < dstH; y++) {
        src_y_start_pos = srcWStride * step_y * y;

        for (unsigned int x = 0; x < dstW; x += 2) {
            src_pos = src_y_start_pos + (stepXStride * x);

            dstBuf[dst_pos++] = srcBuf[src_pos    ];
            dstBuf[dst_pos++] = srcBuf[src_pos + 1];
            dstBuf[dst_pos++] = srcBuf[src_pos + 2];
            dstBuf[dst_pos++] = srcBuf[src_pos + 3];
        }
    }

    return ERROR_NONE;
}

// thumbnail
int SecJpegEncoder::setThumbnailSize(int w, int h)
{
    if (m_flagCreate == false) {
        return ERROR_CANNOT_CREATE_SEC_JPEG_ENC_HAL;
    }

    if (w < 0 || MAX_JPG_WIDTH < w) {
        return false;
    }

    if (h < 0 || MAX_JPG_HEIGHT < h) {
        return false;
    }

    m_thumbnailW = w;
    m_thumbnailH = h;
    return ERROR_NONE;
}


int SecJpegEncoder::encodeThumbnail(unsigned int *size, bool useMain)
{
    int ret = ERROR_NONE;

    if (m_flagCreate == false) {
        return ERROR_CANNOT_CREATE_SEC_JPEG_ENC_HAL;
    }

    // create jpeg thumbnail class
    if (m_jpegThumb == NULL) {
        m_jpegThumb = new SecJpegEncoderHal;

        if (m_jpegThumb == NULL) {
            JPEG_ERROR_ALOG("ERR(%s):Cannot open a jpeg device file\n", __func__);
            return ERROR_CANNOT_CREATE_SEC_THUMB;
        }
    }

    ret = m_jpegThumb->create();
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail create\n", __func__);
        return ret;
    }

        ret = m_jpegThumb->setCache(JPEG_CACHE_ON);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail cache set\n", __func__);
        return ret;
    }

    void *pConfig = m_jpegMain->getJpegConfig();
    if (pConfig == NULL) {
        JPEG_ERROR_ALOG("ERR(%s):Fail getJpegConfig\n", __func__);
        return ERROR_BUFFR_IS_NULL;
    }

    ret = m_jpegThumb->setJpegConfig(pConfig);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail setJpegConfig\n", __func__);
        return ret;
    }

    ret = m_jpegThumb->setQuality(JPEG_THUMBNAIL_QUALITY);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail setQuality\n", __func__);
        return ret;
    }

    ret = m_jpegThumb->setSize(m_thumbnailW, m_thumbnailH);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail setSize\n", __func__);
        return ret;
    }

    int iThumbSize = sizeof(char)*m_thumbnailW*m_thumbnailH*4;
#ifdef JPEG_WA_FOR_PAGEFAULT
    iThumbSize += JPEG_WA_BUFFER_SIZE;
#endif //JPEG_WA_FOR_PAGEFAULT

    freeJpegIonMemory(m_ionJpegClient, &m_ionThumbInBuffer, &m_pThumbInputBuffer, iThumbSize);
    freeJpegIonMemory(m_ionJpegClient, &m_ionThumbOutBuffer, &m_pThumbOutputBuffer, iThumbSize);

    if (m_ionJpegClient == 0) {
        m_ionJpegClient = ion_client_create();
        if (m_ionJpegClient < 0) {
            JPEG_ERROR_ALOG("[%s]src ion client create failed, value = %d\n", __func__, m_ionJpegClient);
            m_ionJpegClient = 0;
            return ret;
        }
    }

    ret = allocJpegIonMemory(m_ionJpegClient, &m_ionThumbInBuffer, &m_pThumbInputBuffer, iThumbSize);
    if (ret != ERROR_NONE) {
        return ret;
    }

    ret = m_jpegThumb->setInBuf(&m_pThumbInputBuffer, &iThumbSize);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail setInBuf\n", __func__);
        return ret;
    }

    ret = allocJpegIonMemory(m_ionJpegClient, &m_ionThumbOutBuffer, &m_pThumbOutputBuffer, iThumbSize);
    if (ret != ERROR_NONE) {
        return ret;
    }

    ret = m_jpegThumb->setOutBuf((char *)m_pThumbOutputBuffer, iThumbSize);
    if (ret) {
        JPEG_ERROR_ALOG("ERR(%s):Fail setOutBuf\n", __func__);
        return ret;
    }

    ret = m_jpegThumb->updateConfig();
    if (ret) {
        JPEG_ERROR_ALOG("update config failed\n");
        return ret;
    }

    if (useMain) {

        int iW=0, iH=0;
        int input_sizeMain=0, input_sizeThumb=0;

        ret = m_jpegMain->getSize(&iW, &iH);
        if (ret) {
            JPEG_ERROR_ALOG("ERR(%s):Fail setJpegConfig\n", __func__);
            return ret;
        }

        ret = m_scaleDownYuv422(*(m_jpegMain->getInBuf(&input_sizeMain)),
                              iW,
                              iH,
                              *(m_jpegThumb->getInBuf(&input_sizeThumb)),
                              m_thumbnailW,
                              m_thumbnailH);
        if (ret) {
            JPEG_ERROR_ALOG("%s::m_scaleDownYuv422(%d, %d, %d, %d) fail", __func__, iW, iH, m_thumbnailW, m_thumbnailH);
            return ret;
        }
    }
    else {
        return ERROR_IMPLEMENT_NOT_YET;
    }

    int outSizeThumb;

    ret = m_jpegThumb->encode();
    if (ret) {
        JPEG_ERROR_ALOG("encode failed\n");
        return ret;
    }

    outSizeThumb = m_jpegThumb->getJpegSize();
    if (outSizeThumb<=0) {
        JPEG_ERROR_ALOG("jpeg size is too small\n");
        return ERROR_THUMB_JPEG_SIZE_TOO_SMALL;
    }

    *size = (unsigned int)outSizeThumb;

    return ERROR_NONE;

}

int SecJpegEncoder::allocJpegIonMemory(ion_client ionClient, ion_buffer *ionBuffer, char **buffer, int size)
{
    int ret = ERROR_NONE;

    if (ionClient == 0) {
        JPEG_ERROR_ALOG("[%s]ionClient is zero (%d)\n", __func__, ionClient);
        return ERROR_BUFFR_IS_NULL;
    }

    *ionBuffer = ion_alloc(ionClient, size, 0, ION_HEAP_SYSTEM_MASK);
    if (*ionBuffer == -1) {
        JPEG_ERROR_ALOG("[%s]ion_alloc(%d) failed\n", __func__, size);
        *ionBuffer = 0;
        return ret;
    }

    *buffer = (char *)ion_map(*ionBuffer, size, 0);
    if (*buffer == MAP_FAILED) {
        JPEG_ERROR_ALOG("[%s]src ion map failed(%d)\n", __func__, size);
        ion_free(*ionBuffer);
        *ionBuffer = 0;
        *buffer = NULL;
        return ret;
    }

    return ret;
}

void SecJpegEncoder::freeJpegIonMemory(ion_client ionClient, ion_buffer *ionBuffer, char **buffer, int size)
{
    if (ionClient == 0) {
        return;
    }

    if (*buffer != NULL) {
        ion_unmap(*buffer, size);
        *buffer = NULL;
    }

    if (*ionBuffer != 0) {
        ion_free(*ionBuffer);
        *ionBuffer = 0;
    }
}

