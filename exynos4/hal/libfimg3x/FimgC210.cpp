/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
**
*/

///////////////////////////////////////////////////
// include
///////////////////////////////////////////////////
#define LOG_NDEBUG 0
#define LOG_TAG "FimgC210"
#include <utils/Log.h>

#include "FimgC210.h"

namespace android
{

//---------------------------------------------------------------------------//
// FimgC210
//---------------------------------------------------------------------------//

Mutex      FimgC210::m_instanceLock;
int        FimgC210::m_curFimgC210Index = 0;
int        FimgC210::m_numOfInstance    = 0;
FimgApi *  FimgC210::m_ptrFimgApiList[NUMBER_FIMG_LIST] = {NULL, };

//---------------------------------------------------------------------------//

FimgC210::FimgC210()
         : m_g2dFd(0),
           m_g2dVirtAddr(NULL),
           m_g2dSize(0),
           m_g2dSrcVirtAddr(NULL),
           m_g2dSrcSize(0),
           m_g2dDstVirtAddr(NULL),
           m_g2dDstSize(0)
{
    m_lock = new Mutex(Mutex::SHARED, "FimgC210");
}

FimgC210::~FimgC210()
{
    delete m_lock;
}

FimgApi * FimgC210::CreateInstance()
{
    Mutex::Autolock autolock(m_instanceLock);

    FimgApi * ptrFimg = NULL;

    // Using List like RingBuffer...
    for(int i = m_curFimgC210Index; i < NUMBER_FIMG_LIST; i++) {
        if(m_ptrFimgApiList[i] == NULL)
            m_ptrFimgApiList[i] = new FimgC210;

        if(m_ptrFimgApiList[i]->FlagCreate() == false) {
            if(m_ptrFimgApiList[i]->Create() == false) {
                PRINT("%s::Create(%d) fail\n", __func__, i);
                goto CreateInstance_End;
            }
            else
                m_numOfInstance++;
        }

        if(i < NUMBER_FIMG_LIST - 1)
            m_curFimgC210Index = i + 1;
        else
            m_curFimgC210Index = 0;

        ptrFimg = m_ptrFimgApiList[i];
        goto CreateInstance_End;
    }

CreateInstance_End :

    return ptrFimg;
}

void FimgC210::DestroyInstance(FimgApi * ptrFimgApi)
{
    Mutex::Autolock autolock(m_instanceLock);

    for(int i = 0; i < NUMBER_FIMG_LIST; i++) {
        if(m_ptrFimgApiList[i] != NULL
           && m_ptrFimgApiList[i] == ptrFimgApi) {
            if(m_ptrFimgApiList[i]->FlagCreate() == true
               && m_ptrFimgApiList[i]->Destroy() == false) {
                PRINT("%s::Destroy() fail\n", __func__);
            } else {
                FimgC210 * tempFimgC210 =  (FimgC210 *)m_ptrFimgApiList[i];
                delete tempFimgC210;
                m_ptrFimgApiList[i] = NULL;

                m_numOfInstance--;
            }

            break;
        }
    }
}

void FimgC210::DestroyAllInstance(void)
{
    Mutex::Autolock autolock(m_instanceLock);

    for(int i = 0; i < NUMBER_FIMG_LIST; i++) {
        if(m_ptrFimgApiList[i] != NULL) {
            if(m_ptrFimgApiList[i]->FlagCreate() == true
               && m_ptrFimgApiList[i]->Destroy() == false) {
                    PRINT("%s::Destroy() fail\n", __func__);
            } else {
                FimgC210 * tempFimgC210 =  (FimgC210 *)m_ptrFimgApiList[i];
                delete tempFimgC210;
                m_ptrFimgApiList[i] = NULL;
            }
        }
    }
}


bool FimgC210::t_Create(void)
{
    bool ret = true;

    if(m_CreateG2D() == false) {
        PRINT("%s::m_CreateG2D() fail \n", __func__);

        if(m_DestroyG2D() == false)
            PRINT("%s::m_DestroyG2D() fail \n", __func__);

        ret = false;
    }

    return ret;
}

bool FimgC210::t_Destroy(void)
{
    bool ret = true;

    if(m_DestroyG2D() == false) {
        PRINT("%s::m_DestroyG2D() fail \n", __func__);
        ret = false;
    }

    return ret;
}

bool FimgC210::t_Stretch(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag)
{
    #ifdef CHECK_FIMGC210_PERFORMANCE
        #define   NUM_OF_STEP (10)
        StopWatch    stopWatch("CHECK_FIMGC210_PERFORMANCE");
        const char * stopWatchName[NUM_OF_STEP];
        nsecs_t      stopWatchTime[NUM_OF_STEP];
        int          stopWatchIndex = 0;
    #endif // CHECK_FIMGC210_PERFORMANCE

    if(m_DoG2D(src, dst, clip, flag) == false) {
        goto STRETCH_FAIL;
    }

#ifdef G2D_NONE_BLOCKING_MODE
    if(m_PollG2D(&m_g2dPoll) == false)
    {
        PRINT("%s::m_PollG2D() fail\n", __func__);
        goto STRETCH_FAIL;
    }
#endif

    #ifdef CHECK_FIMGC210_PERFORMANCE
        m_PrintFimgC210Performance(src, dst, stopWatchIndex, stopWatchName, stopWatchTime);
    #endif // CHECK_FIMGC210_PERFORMANCE

    return true;

STRETCH_FAIL:
    return false;

}

bool FimgC210::t_Sync(void)
{
#if 0
    if(ioctl(m_g2dFd, G2D_SYNC) < 0) {
        PRINT("%s::G2D_Sync fail\n", __func__);
        goto SYNC_FAIL;
    }
#else
    if(m_PollG2D(&m_g2dPoll) == false)
    {
        PRINT("%s::m_PollG2D() fail\n", __func__);
        goto SYNC_FAIL;
    }
#endif
    return true;

SYNC_FAIL:
    return false;

}

bool FimgC210::t_Lock(void)
{
    m_lock->lock();
    return true;
}

bool FimgC210::t_UnLock(void)
{
    m_lock->unlock();
    return true;
}

bool FimgC210::m_CreateG2D(void)
{
    void * mmap_base;

    if(m_g2dFd != 0) {
        PRINT("%s::m_g2dFd(%d) is not 0 fail\n", __func__, m_g2dFd);
        return false;
    }

#ifdef G2D_NONE_BLOCKING_MODE
    m_g2dFd = open(SEC_G2D_DEV_NAME, O_RDWR | O_NONBLOCK);
#else
    m_g2dFd = open(SEC_G2D_DEV_NAME, O_RDWR);
#endif
    if(m_g2dFd < 0) {
        PRINT("%s::open(%s) fail(%s)\n", __func__, SEC_G2D_DEV_NAME, strerror(errno));
        m_g2dFd = 0;
        return false;
    }

    memset(&m_g2dPoll, 0, sizeof(m_g2dPoll));
    m_g2dPoll.fd     = m_g2dFd;
    m_g2dPoll.events = POLLOUT | POLLERR;

    return true;
}

bool FimgC210::m_DestroyG2D(void)
{
    if(m_g2dVirtAddr != NULL) {
        munmap(m_g2dVirtAddr, m_g2dSize);
        m_g2dVirtAddr = NULL;
        m_g2dSize = 0;
    }

    if(0 < m_g2dFd) {
        close(m_g2dFd);
    }
    m_g2dFd = 0;

    return true;
}

//bool FimgC210::m_DoG2D(FimgRect * src, FimgRect * dst, int rotateValue, int alphaValue, int colorKey)
bool FimgC210::m_DoG2D(FimgRect * src, FimgRect * dst, FimgClip * clip, FimgFlag * flag)
{
    g2d_params params;

    memcpy(&params.src_rect, src, sizeof(FimgRect));
    memcpy(&params.dst_rect, dst, sizeof(FimgRect));
    memcpy(&params.clip, clip, sizeof(FimgClip));
    memcpy(&params.flag, flag, sizeof(FimgFlag));

    if(ioctl(m_g2dFd, G2D_BLIT, &params) < 0) {
        #if 0
        {
            PRINT("---------------------------------------\n");
            PRINT("src.color_format : %d \n", src->color_format);
            PRINT("src.full_w       : %d \n", src->full_w);
            PRINT("src.full_h       : %d \n", src->full_h);
            PRINT("src.x            : %d \n", src->x);
            PRINT("src.y            : %d \n", src->y);
            PRINT("src.w            : %d \n", src->w);
            PRINT("src.h            : %d \n", src->h);

            PRINT("dst.color_format : %d \n", dst->color_format);
            PRINT("dst.full_w       : %d \n", dst->full_w);
            PRINT("dst.full_h       : %d \n", dst->full_h);
            PRINT("dst.x            : %d \n", dst->x);
            PRINT("dst.y            : %d \n", dst->y);
            PRINT("dst.w            : %d \n", dst->w);
            PRINT("dst.h            : %d \n", dst->h);

            PRINT("flag.rotate_val  : %d \n", flag->rotate_val);
            PRINT("flag.alpha_val   : %d(%d) \n", flag->alpha_val);
            PRINT("flag.color_key_mode  : %d(%d) \n", flag->color_key_mode, flag->color_key_val);
            PRINT("---------------------------------------\n");
        }
        #endif

        return false;
    }

    return true;
}

inline bool FimgC210::m_PollG2D(struct pollfd * events)
{
    #define G2D_POLL_TIME (1000)

    int ret;

    ret = poll(events, 1, G2D_POLL_TIME);

    if (ret < 0) {
        if(ioctl(m_g2dFd, G2D_RESET) < 0) {
	     PRINT("%s::G2D_RESET fail\n", __func__);
        }
        PRINT("%s::poll fail \n", __func__);
        return false;
    }
    else if (ret == 0) {
        if(ioctl(m_g2dFd, G2D_RESET) < 0) {
	     PRINT("%s::G2D_RESET fail\n", __func__);
        }
        PRINT("%s::No data in %d milli secs..\n", __func__, G2D_POLL_TIME);
        return false;
    }

    return true;
}

inline bool FimgC210::m_CleanG2D(unsigned int virtAddr, unsigned int size)
{
    g2d_dma_info dma_info = { virtAddr, size };

    if(ioctl(m_g2dFd, G2D_DMA_CACHE_CLEAN, &dma_info) < 0) {
        PRINT("%s::G2D_DMA_CACHE_CLEAN(%d, %d) fail\n", __func__, virtAddr, size);
        return false;
    }
    return true;
}

inline bool FimgC210::m_FlushG2D  (unsigned int virtAddr, unsigned int size)
{
    g2d_dma_info dma_info = { virtAddr, size };

    if(ioctl(m_g2dFd, G2D_DMA_CACHE_FLUSH, &dma_info) < 0) {
        PRINT("%s::G2D_DMA_CACHE_FLUSH(%d, %d) fail\n", __func__, virtAddr, size);
        return false;
    }
    return true;
}

inline int FimgC210::m_RotateValueFimgApi2FimgHw(int rotateValue)
{
    switch (rotateValue) {
    case ROTATE_0:      return G2D_ROT_0;
    case ROTATE_90:     return G2D_ROT_90;
    case ROTATE_180:    return G2D_ROT_180;
    case ROTATE_270:    return G2D_ROT_270;
    case ROTATE_X_FLIP: return G2D_ROT_X_FLIP;
    case ROTATE_Y_FLIP: return G2D_ROT_Y_FLIP;
    }

    return -1;
}


#ifdef CHECK_FIMGC210_PERFORMANCE
void FimgC210::m_PrintFimgC210Performance(FimgRect *   src,
                                          FimgRect *   dst,
                                          int          stopWatchIndex,
                                          const char * stopWatchName[],
                                          nsecs_t      stopWatchTime[])
{
    char * srcColorFormat = "UNKNOW_COLOR_FORMAT";
    char * dstColorFormat = "UNKNOW_COLOR_FORMAT";

    switch(src->color_format)
    {
    case COLOR_FORMAT_RGB_565   :
        srcColorFormat = "RGB_565";
        break;
    case COLOR_FORMAT_RGBA_8888 :
        srcColorFormat = "RGBA_8888";
        break;
    case COLOR_FORMAT_RGBX_8888 :
        srcColorFormat = "RGBX_8888";
        break;
    default :
        srcColorFormat = "UNKNOW_COLOR_FORMAT";
        break;
    }

    switch(dst->color_format)
    {
    case COLOR_FORMAT_RGB_565   :
        dstColorFormat = "RGB_565";
        break;
    case COLOR_FORMAT_RGBA_8888 :
        dstColorFormat = "RGBA_8888";
        break;
    case COLOR_FORMAT_RGBX_8888 :
        dstColorFormat = "RGBX_8888";
        break;
    default :
        dstColorFormat = "UNKNOW_COLOR_FORMAT";
        break;
    }


#ifdef CHECK_FIMGC210_CRITICAL_PERFORMANCE
#else
    PRINT("===============================================\n");
    PRINT("src[%3d, %3d | %10s] -> dst[%3d, %3d | %10s]\n",
          src->w, src->h, srcColorFormat,
          dst->w, dst->h, dstColorFormat);
#endif

    nsecs_t totalTime = stopWatchTime[stopWatchIndex - 1];

    for(int i = 0 ; i < stopWatchIndex; i++) {
        nsecs_t sectionTime;

        if(i != 0)
            sectionTime = stopWatchTime[i] - stopWatchTime[i-1];
        else
            sectionTime = stopWatchTime[i];

#ifdef CHECK_FIMGC210_CRITICAL_PERFORMANCE
        if(1500 < (sectionTime / 1000)) // check 1.5 mille second..
#endif
        {
            PRINT("===============================================\n");
            PRINT("src[%3d, %3d | %10s] -> dst[%3d, %3d | %10s]\n",
                   src->w, src->h, srcColorFormat,
                   dst->w, dst->h, dstColorFormat);

            PRINT("%20s : %5lld msec(%5.2f %%)\n",
              stopWatchName[i],
              sectionTime / 1000,
              ((float)sectionTime / (float)totalTime) * 100.0f);
        }
    }

}
#endif // CHECK_FIMGC210_PERFORMANCE

//---------------------------------------------------------------------------//
// extern function
//---------------------------------------------------------------------------//
extern "C" struct FimgApi * createFimgApi()
{
    if (fimgApiAutoFreeThread == 0)
        fimgApiAutoFreeThread = new FimgApiAutoFreeThread();
    else
        fimgApiAutoFreeThread->SetOneMoreSleep();

    return FimgC210::CreateInstance();
}

extern "C" void destroyFimgApi(FimgApi * ptrFimgApi)
{
    // Dont' call DestrotInstance..
    // return FimgC210::DestroyInstance(ptrFimgApi);
}

}; // namespace android
