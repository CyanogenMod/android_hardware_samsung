/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef __SEC_JPG_CODEC_HAL_H__
#define __SEC_JPG_CODEC_HAL_H__

#include "videodev2.h"

#define JPEG_DEC_NODE        "/dev/video11"
#define JPEG_ENC_NODE        "/dev/video12"

#define JPEG_MAX_PLANE_CNT          (3)
#define JPEG_BYTE_ALIGN     (32)

#define MAX_JPG_WIDTH 8192
#define MAX_JPG_HEIGHT 8192

#define JPEG_CACHE_OFF (0)
#define JPEG_CACHE_ON (1)

class SecJpegCodecHal {
public:
    ;
    SecJpegCodecHal();
    virtual ~SecJpegCodecHal();

    enum ERROR_JPEG_HAL {
        ERROR_JPEG_DEVICE_ALREADY_CREATE = -0x100,
        ERROR_CANNOT_OPEN_JPEG_DEVICE,
        ERROR_JPEG_DEVICE_ALREADY_CLOSED,
        ERROR_JPEG_DEVICE_ALREADY_DESTROY,
        ERROR_JPEG_DEVICE_NOT_CREATE_YET,
        ERROR_INVALID_COLOR_FORMAT,
        ERROR_INVALID_JPEG_FORMAT,
        ERROR_INVALID_IMAGE_SIZE,
        ERROR_JPEG_CONFIG_POINTER_NULL,
        ERROR_INVALID_JPEG_CONFIG,
        ERROR_IN_BUFFER_CREATE_FAIL,
        ERROR_OUT_BUFFER_CREATE_FAIL,
        ERROR_EXCUTE_FAIL,
        ERROR_JPEG_SIZE_TOO_SMALL,
        ERROR_CANNOT_CHANGE_CACHE_SETTING,
        ERROR_SIZE_NOT_SET_YET,
        ERROR_BUFFR_IS_NULL,
        ERROR_BUFFER_TOO_SMALL,
        ERROR_GET_SIZE_FAIL,
        ERROR_REQBUF_FAIL,
        ERROR_INVALID_V4l2_BUF_TYPE = -0x80,
        ERROR_MMAP_FAILED,
        ERROR_FAIL,
        ERROR_NONE = 0
    };

    enum MODE {
        MODE_ENCODE = 0,
        MODE_DECODE
    };

    struct BUFFER{
        int     numOfPlanes;
        char *addr[JPEG_MAX_PLANE_CNT];
        int     size[JPEG_MAX_PLANE_CNT];
    };

    struct BUF_INFO{
        int                 numOfPlanes;
        enum v4l2_memory    memory;
        enum v4l2_buf_type  buf_type;
        int                 reserved[4];
    };

    struct PIX_FMT{
        int in_fmt;
        int out_fmt;
        int reserved[4];
    };

    struct CONFIG{
        int               mode;
        int               enc_qual;

        int               width;
        int               height;
        int               scaled_width;
        int               scaled_height;

        int               numOfPlanes;

        int               sizeJpeg;

        union {
            PIX_FMT enc_fmt;
            PIX_FMT dec_fmt;
        } pix;

        int              reserved[8];
    };

protected:
    // variables
    bool t_bFlagCreate;
    bool t_bFlagCreateInBuf;
    bool t_bFlagCreateOutBuf;
    bool t_bFlagExcute;

    int t_iPlaneNum;

    int t_iJpegFd;
    struct CONFIG t_stJpegConfig;
    struct BUFFER t_stJpegInbuf;
    struct BUFFER t_stJpegOutbuf;

    //functions
    int t_v4l2Querycap(int iFd);
    int t_v4l2SetJpegcomp(int iFd, int iQuality);
    int t_v4l2SetFmt(int iFd, enum v4l2_buf_type eType, struct CONFIG *pstConfig);
    int t_v4l2GetFmt(int iFd, enum v4l2_buf_type eType, struct CONFIG *pstConfig);
    int t_v4l2Reqbufs(int iFd, int iBufCount, struct BUF_INFO *pstBufInfo);
    int t_v4l2Querybuf(int iFd, struct BUF_INFO *pstBufInfo, struct BUFFER *pstBuf);
    int t_v4l2Qbuf(int iFd, struct BUF_INFO *pstBufInfo, struct BUFFER *pstBuf);
    int t_v4l2Dqbuf(int iFd, enum v4l2_buf_type eType, enum v4l2_memory eMemory);
    int t_v4l2StreamOn(int iFd, enum v4l2_buf_type eType);
    int t_v4l2StreamOff(int iFd, enum v4l2_buf_type eType);
    int t_v4l2SetCtrl(int iFd, int iCid, int iValue);
    int t_v4l2GetCtrl(int iFd, int iCid);
};

class SecJpegEncoderHal : SecJpegCodecHal {
public:
    ;
    SecJpegEncoderHal();
    virtual ~SecJpegEncoderHal();

    enum QUALITY {
        QUALITY_LEVEL_1 = 0,    /* high */
        QUALITY_LEVEL_2,
        QUALITY_LEVEL_3,
        QUALITY_LEVEL_4,        /* low */
    };

    int     create(void);
    int     destroy(void);

    int     setSize(int iW, int iH);

    int     setJpegConfig(void* pConfig);
    void *getJpegConfig(void);


    char **getInBuf(int *piInputSize);
    char *getOutBuf(int *piOutputSize);

    int     setCache(int iValue);

    int     setInBuf(char **pcBuf, int *iSize);
    int     setOutBuf(char *pcBuf, int iSize);

    int     getSize(int *piWidth, int *piHeight);
    int     setColorFormat(int iV4l2ColorFormat);
    int     setJpegFormat(int iV4l2JpegFormat);
    int     updateConfig(void);

    int     setQuality(int iQuality);
    int     getJpegSize(void);

    int     encode(void);
};

class SecJpegDecoderHal : SecJpegCodecHal {
public:
    ;
    SecJpegDecoderHal();
    virtual ~SecJpegDecoderHal();

    int     create(void);
    int     destroy(void);

    int     setSize(int iW, int iH);

    int     setJpegConfig(void* pConfig);
    void *getJpegConfig(void);


    char *getInBuf(int *piInputSize);
    char **getOutBuf(int *piOutputSize);

    int     setCache(int iValue);

    int     setInBuf(char *pcBuf, int iSize);
    int     setOutBuf(char **pcBuf, int *iSize);

    int     getSize(int *piWidth, int *piHeight);
    int     setColorFormat(int iV4l2ColorFormat);
    int     setJpegFormat(int iV4l2JpegFormat);
    int     updateConfig(void);

    int setScaledSize(int iW, int iH);
    int setJpegSize(int iJpegSize);

    int  decode(void);
};

#endif /* __SEC_JPG_CODEC_HAL_H__ */
