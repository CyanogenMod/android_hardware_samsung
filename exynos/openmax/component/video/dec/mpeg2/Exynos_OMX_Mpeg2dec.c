/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
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
 * @file        Exynos_OMX_Mpeg2dec.c
 * @brief
 * @author      Satish Kumar Reddy (palli.satish@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.07.10 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_Mpeg2dec.h"
#include "ExynosVideoApi.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"

#ifdef USE_ANB
#include "Exynos_OSAL_Android.h"
#endif

/* To use CSC_METHOD_HW in EXYNOS OMX, gralloc should allocate physical memory using FIMC */
/* It means GRALLOC_USAGE_HW_FIMC1 should be set on Native Window usage */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_MPEG2_DEC"
#define EXYNOS_LOG_OFF
//#define EXYNOS_TRACE_ON
#include "Exynos_OSAL_Log.h"

#define MPEG2_DEC_NUM_OF_EXTRA_BUFFERS 7

//#define FULL_FRAME_SEARCH /* Full frame search not support*/

static OMX_ERRORTYPE GetCodecInputPrivateData(OMX_PTR codecBuffer, void *pVirtAddr, OMX_U32 *dataSize)
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetCodecOutputPrivateData(OMX_PTR codecBuffer, void *addr[], int size[])
{
    OMX_ERRORTYPE       ret = OMX_ErrorNone;
    ExynosVideoBuffer  *pCodecBuffer;

    if (codecBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pCodecBuffer = (ExynosVideoBuffer *)codecBuffer;

    if (addr != NULL) {
        addr[0] = pCodecBuffer->planes[0].addr;
        addr[1] = pCodecBuffer->planes[1].addr;
        addr[2] = pCodecBuffer->planes[2].addr;
    }

    if (size != NULL) {
        size[0] = pCodecBuffer->planes[0].allocSize;
        size[1] = pCodecBuffer->planes[1].allocSize;
        size[2] = pCodecBuffer->planes[2].allocSize;
    }

EXIT:
    return ret;
}

int Check_Mpeg2_Frame(
    OMX_U8   *pInputStream,
    int       buffSize,
    OMX_U32   flag,
    OMX_BOOL  bPreviousFrameEOF,
    OMX_BOOL *pbEndOfFrame)
{
    FunctionIn();

    *pbEndOfFrame = OMX_TRUE;

    /* Frame  Start code*/
    if (pInputStream[0] != 0x00 || pInputStream[1] != 0x00 || pInputStream[2]!=0x01) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mpeg2 Frame Start Code not Found");
        *pbEndOfFrame = OMX_FALSE;
    }

    FunctionOut();
    return buffSize;
}

static OMX_BOOL Check_Mpeg2_StartCode(
    OMX_U8     *pInputStream,
    OMX_U32     streamSize)
{
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "streamSize: %d",streamSize);

    if (streamSize < 3) {
        return OMX_FALSE;
    }

    /* Frame  Start code*/
    if (pInputStream[0] != 0x00 || pInputStream[1] != 0x00 || pInputStream[2]!=0x01) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mpeg2 Frame Start Code not Found");
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

OMX_ERRORTYPE Mpeg2CodecOpen(EXYNOS_MPEG2DEC_HANDLE *pMpeg2Dec)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pMpeg2Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }

    /* alloc ops structure */
    pDecOps    = (ExynosVideoDecOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecOps));
    pInbufOps  = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));
    pOutbufOps = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate decoder ops buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pMpeg2Dec->hMFCMpeg2Handle.pDecOps    = pDecOps;
    pMpeg2Dec->hMFCMpeg2Handle.pInbufOps  = pInbufOps;
    pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pDecOps->nSize    = sizeof(ExynosVideoDecOps);
    pInbufOps->nSize  = sizeof(ExynosVideoDecBufferOps);
    pOutbufOps->nSize = sizeof(ExynosVideoDecBufferOps);

    Exynos_Video_Register_Decoder(pDecOps, pInbufOps, pOutbufOps);

    /* check mandatory functions for decoder ops */
    if ((pDecOps->Init == NULL) || (pDecOps->Finalize == NULL) ||
        (pDecOps->Get_ActualBufferCount == NULL) || (pDecOps->Set_FrameTag == NULL) ||
        (pDecOps->Get_FrameTag == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for buffer ops */
    if ((pInbufOps->Setup == NULL) || (pOutbufOps->Setup == NULL) ||
        (pInbufOps->Run == NULL) || (pOutbufOps->Run == NULL) ||
        (pInbufOps->Stop == NULL) || (pOutbufOps->Stop == NULL) ||
        (pInbufOps->Enqueue == NULL) || (pOutbufOps->Enqueue == NULL) ||
        (pInbufOps->Dequeue == NULL) || (pOutbufOps->Dequeue == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* alloc context, open, querycap */
#ifdef USE_DMA_BUF
    pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.pDecOps->Init(V4L2_MEMORY_DMABUF);
#else
    pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.pDecOps->Init(V4L2_MEMORY_USERPTR);
#endif
    if (pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate context buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pDecOps != NULL) {
            Exynos_OSAL_Free(pDecOps);
            pMpeg2Dec->hMFCMpeg2Handle.pDecOps = NULL;
        }
        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pMpeg2Dec->hMFCMpeg2Handle.pInbufOps = NULL;
        }
        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecClose(EXYNOS_MPEG2DEC_HANDLE *pMpeg2Dec)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pMpeg2Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pDecOps->Finalize(hMFCHandle);
        pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle = NULL;
    }
    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps = NULL;
    }
    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pMpeg2Dec->hMFCMpeg2Handle.pInbufOps = NULL;
    }
    if (pDecOps != NULL) {
        Exynos_OSAL_Free(pDecOps);
        pMpeg2Dec->hMFCMpeg2Handle.pDecOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecStart(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_MPEG2DEC_HANDLE   *pMpeg2Dec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pMpeg2Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecStop(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_MPEG2DEC_HANDLE   *pMpeg2Dec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pMpeg2Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL))
        pInbufOps->Stop(hMFCHandle);
    else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL))
        pOutbufOps->Stop(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecOutputBufferProcessRun(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE            ret = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_MPEG2DEC_HANDLE   *pMpeg2Dec = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pMpeg2Dec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pMpeg2Dec->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pMpeg2Dec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pMpeg2Dec->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pMpeg2Dec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecEnQueueAllBuffer(OMX_COMPONENTTYPE *pOMXComponent, OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    int i, nOutbufs;

    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) && (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pMpeg2Dec->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]->pVirAddr[0]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pMpeg2Dec->bDestinationStart == OMX_TRUE)) {
        OMX_U32 dataLen[MFC_OUTPUT_BUFFER_PLANE] = {0, 0};
        ExynosVideoBuffer *pBuffer = NULL;

        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        nOutbufs = pDecOps->Get_ActualBufferCount(hMFCHandle);
        nOutbufs += EXTRA_DPB_NUM;
        for (i = 0; i < nOutbufs; i++) {
            pOutbufOps->Get_Buffer(hMFCHandle, i, &pBuffer);
            Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, (OMX_PTR)pBuffer);
        }
        pOutbufOps->Clear_Queue(hMFCHandle);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecSrcSetup(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE        *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize = pSrcInputData->dataLen;

    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;
    OMX_U32                  inputBufferNumber = 0;
    int i;

    FunctionIn();

    if ((oneFrameSize <= 0) && (pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS)) {
        OMX_BUFFERHEADERTYPE *OMXBuffer = NULL;
        OMXBuffer = Exynos_OutputBufferGetQueue_Direct(pExynosComponent);
        if (OMXBuffer == NULL) {
            ret = OMX_ErrorUndefined;
            goto EXIT;
        }

        OMXBuffer->nTimeStamp = pSrcInputData->timeStamp;
        OMXBuffer->nFlags = pSrcInputData->nFlags;
        Exynos_OMX_OutputBufferReturn(pOMXComponent, OMXBuffer);

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pVideoDec->bThumbnailMode == OMX_TRUE)
        pDecOps->Set_DisplayDelay(hMFCHandle, 0);

    /* input buffer info */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    bufferConf.eCompressionFormat = VIDEO_CODING_MPEG2;
    pInbufOps->Set_Shareable(hMFCHandle);
    if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        bufferConf.nSizeImage = pExynosInputPort->portDefinition.format.video.nFrameWidth
                                * pExynosInputPort->portDefinition.format.video.nFrameHeight * 3 / 2;
        inputBufferNumber = MAX_VIDEO_INPUTBUFFER_NUM;
    } else if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        bufferConf.nSizeImage = DEFAULT_MFC_INPUT_BUFFER_SIZE;
        inputBufferNumber = MFC_INPUT_BUFFER_NUM_MAX;
    }

    /* should be done before prepare input buffer */
    if (pInbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set input buffer geometry */
    if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, inputBufferNumber) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        /* Register input buffer */
        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            ExynosVideoPlane plane;
            plane.addr = pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0];
            plane.allocSize = pVideoDec->pMFCDecInputBuffer[i]->bufferSize[0];
            plane.fd = pVideoDec->pMFCDecInputBuffer[i]->fd[0];
            if (pInbufOps->Register(hMFCHandle, &plane, MFC_INPUT_BUFFER_PLANE) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "BUFFER_COPY Failed to Register input buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
        }
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /* Register input buffer */
        for (i = 0; i < pExynosInputPort->portDefinition.nBufferCountActual; i++) {
            ExynosVideoPlane plane;
            plane.addr = pExynosInputPort->extendBufferHeader[i].OMXBufferHeader->pBuffer;
            plane.allocSize = pExynosInputPort->extendBufferHeader[i].OMXBufferHeader->nAllocLen;
            plane.fd = pExynosInputPort->extendBufferHeader[i].buf_fd[0];
            if (pInbufOps->Register(hMFCHandle, &plane, MFC_INPUT_BUFFER_PLANE) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "BUFFER_SHARE Failed to Register input buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
        }
    }

    /* set output geometry */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    pMpeg2Dec->hMFCMpeg2Handle.MFCOutputColorType = bufferConf.eColorFormat = VIDEO_COLORFORMAT_NV12_TILED;
    if (pOutbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for output buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* input buffer enqueue for header parsing */
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d", oneFrameSize);
    if (pInbufOps->Enqueue(hMFCHandle, (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.dataBuffer,
                        (unsigned int *)&oneFrameSize, MFC_INPUT_BUFFER_PLANE, pSrcInputData->bufferHeader) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to enqueue input buffer for header parsing");
//        ret = OMX_ErrorInsufficientResources;
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* start header parsing */
    if (pInbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run input buffer for header parsing");
        ret = OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* get geometry for output */
    Exynos_OSAL_Memset(&pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    if (pOutbufOps->Get_Geometry(hMFCHandle, &pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to get geometry for parsed header info");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* get dpb count */
    pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum += EXTRA_DPB_NUM;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "Mpeg2CodecSetup nOutbufs: %d", pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum);

    pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCSrc = OMX_TRUE;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pExynosInputPort->portDefinition.format.video.nFrameWidth != pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth) ||
            (pExynosInputPort->portDefinition.format.video.nFrameHeight != pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight)) {
            pExynosInputPort->portDefinition.format.video.nFrameWidth = pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth;
            pExynosInputPort->portDefinition.format.video.nFrameHeight = pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight;
            pExynosInputPort->portDefinition.format.video.nStride = ((pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pExynosInputPort->portDefinition.format.video.nSliceHeight = ((pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight + 15) & (~15));

            Exynos_UpdateFrameSize(pOMXComponent);
            pExynosOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pExynosInputPort->portDefinition.format.video.nFrameWidth != pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth) ||
            (pExynosInputPort->portDefinition.format.video.nFrameHeight != pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight) ||
            (pExynosOutputPort->portDefinition.nBufferCountActual != pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum)) {
            pExynosInputPort->portDefinition.format.video.nFrameWidth = pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth;
            pExynosInputPort->portDefinition.format.video.nFrameHeight = pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight;
            pExynosInputPort->portDefinition.format.video.nStride = ((pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pExynosInputPort->portDefinition.format.video.nSliceHeight = ((pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight + 15) & (~15));

            pExynosOutputPort->portDefinition.nBufferCountActual = pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum - 4;
            pExynosOutputPort->portDefinition.nBufferCountMin = pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum - 4;

            Exynos_UpdateFrameSize(pOMXComponent);
            pExynosOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    }
    Exynos_OSAL_SleepMillisec(0);
    ret = OMX_ErrorInputDataDecodeYet;
    Mpeg2CodecStop(pOMXComponent, INPUT_PORT_INDEX);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Mpeg2CodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    int i, nOutbufs;

    FunctionIn();

    /* get dpb count */
    nOutbufs = pMpeg2Dec->hMFCMpeg2Handle.maxDPBNum;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        /* should be done before prepare output buffer */
        if (pOutbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    }

    pOutbufOps->Set_Shareable(hMFCHandle);
    if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ExynosVideoPlane planes[MFC_OUTPUT_BUFFER_PLANE];
    OMX_U32 nAllocLen[MFC_OUTPUT_BUFFER_PLANE] = {0, 0};
    OMX_U32 dataLen[MFC_OUTPUT_BUFFER_PLANE] = {0, 0};
    int plane;

    nAllocLen[0] = calc_plane(pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth,
                        pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight);
    nAllocLen[1] = calc_plane(pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameWidth,
                        pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf.nFrameHeight >> 1);

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        /* Register output buffer */
        for (i = 0; i < nOutbufs; i++) {
            pVideoDec->pMFCDecOutputBuffer[i] = (CODEC_DEC_BUFFER *)Exynos_OSAL_Malloc(sizeof(CODEC_DEC_BUFFER));
            Exynos_OSAL_Memset(pVideoDec->pMFCDecOutputBuffer[i], 0, sizeof(CODEC_DEC_BUFFER));

            for (plane = 0; plane < MFC_OUTPUT_BUFFER_PLANE; plane++) {
                pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane] =
                    (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoDec->hSharedMemory, nAllocLen[plane], NORMAL_MEMORY);
                if (pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane] == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Alloc output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
                pVideoDec->pMFCDecOutputBuffer[i]->fd[plane] =
                    Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory,
                                                       pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane]);
                pVideoDec->pMFCDecOutputBuffer[i]->bufferSize[plane] = nAllocLen[plane];

                planes[plane].addr = pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane];
                planes[plane].fd = pVideoDec->pMFCDecOutputBuffer[i]->fd[plane];
                planes[plane].allocSize = pVideoDec->pMFCDecOutputBuffer[i]->bufferSize[plane];
            }

            if (pOutbufOps->Register(hMFCHandle, planes, MFC_OUTPUT_BUFFER_PLANE) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Register output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr,
                            (unsigned int *)dataLen, MFC_OUTPUT_BUFFER_PLANE, NULL);
        }
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /* Register output buffer */
        /*************/
        /*    TBD    */
        /*************/
#ifdef USE_ANB
        if (pExynosOutputPort->bIsANBEnabled == OMX_TRUE) {
            for (i = 0; i < pExynosOutputPort->assignedBufferNum; i++) {
                for (plane = 0; plane < MFC_OUTPUT_BUFFER_PLANE; plane++) {
                    planes[plane].fd = pExynosOutputPort->extendBufferHeader[i].buf_fd[plane];
                    planes[plane].addr = pExynosOutputPort->extendBufferHeader[i].pYUVBuf[plane];
                    planes[plane].allocSize = nAllocLen[plane];
                }

                if (pOutbufOps->Register(hMFCHandle, planes, MFC_OUTPUT_BUFFER_PLANE) != VIDEO_ERROR_NONE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Register output buffer");
                    ret = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pExynosOutputPort->extendBufferHeader[i].pYUVBuf,
                              (unsigned int *)dataLen, MFC_OUTPUT_BUFFER_PLANE, NULL);
            }
        } else {
            ret = OMX_ErrorNotImplemented;
            goto EXIT;
        }
#else
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
#endif
    }

    if (pOutbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run output buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        Mpeg2CodecStop (pOMXComponent, OUTPUT_PORT_INDEX);
    }
    pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCDst = OMX_TRUE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamVideoMpeg2:
    {
        OMX_VIDEO_PARAM_MPEG2TYPE *pDstMpeg2Param = (OMX_VIDEO_PARAM_MPEG2TYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_MPEG2TYPE *pSrcMpeg2Param = NULL;
        EXYNOS_MPEG2DEC_HANDLE       *pMpeg2Dec      = NULL;
        ret = Exynos_OMX_Check_SizeVersion(pDstMpeg2Param, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
        if (ret != OMX_ErrorNone) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "1");
            goto EXIT;
        }

        if (pDstMpeg2Param->nPortIndex > OUTPUT_PORT_INDEX) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "2");
            ret = OMX_ErrorBadPortIndex;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcMpeg2Param = &pMpeg2Dec->Mpeg2Component[pDstMpeg2Param->nPortIndex];

        Exynos_OSAL_Memcpy(pDstMpeg2Param, pSrcMpeg2Param, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_MPEG2_DEC_ROLE);
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE*)pComponentParameterStructure;
        OMX_VIDEO_PARAM_MPEG2TYPE *pSrcMpeg2Component = NULL;
        EXYNOS_MPEG2DEC_HANDLE      *pMpeg2Dec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcMpeg2Component = &pMpeg2Dec->Mpeg2Component[pDstProfileLevel->nPortIndex];

        pDstProfileLevel->eProfile = pSrcMpeg2Component->eProfile;
        pDstProfileLevel->eLevel = pSrcMpeg2Component->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = NULL;
        EXYNOS_MPEG2DEC_HANDLE                *pMpeg2Dec                 = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcErrorCorrectionType = &pMpeg2Dec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE           ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE     *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    case OMX_IndexParamVideoMpeg2:
    {
        OMX_VIDEO_PARAM_MPEG2TYPE *pDstMpeg2Param = NULL;
        OMX_VIDEO_PARAM_MPEG2TYPE *pSrcMpeg2Param = (OMX_VIDEO_PARAM_MPEG2TYPE *)pComponentParameterStructure;
        EXYNOS_MPEG2DEC_HANDLE       *pMpeg2Dec      = NULL;
        ret = Exynos_OMX_Check_SizeVersion(pSrcMpeg2Param, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcMpeg2Param->nPortIndex > OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstMpeg2Param = &pMpeg2Dec->Mpeg2Component[pSrcMpeg2Param->nPortIndex];

        Exynos_OSAL_Memcpy(pDstMpeg2Param, pSrcMpeg2Param, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_MPEG2_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
        } else {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_MPEG2TYPE *pDstMpeg2Component = NULL;
        EXYNOS_MPEG2DEC_HANDLE      *pMpeg2Dec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;

        pDstMpeg2Component = &pMpeg2Dec->Mpeg2Component[pSrcProfileLevel->nPortIndex];
        pDstMpeg2Component->eProfile = pSrcProfileLevel->eProfile;
        pDstMpeg2Component->eLevel = pSrcProfileLevel->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = NULL;
        EXYNOS_MPEG2DEC_HANDLE                *pMpeg2Dec                 = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstErrorCorrectionType = &pMpeg2Dec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    default:
        ret = Exynos_OMX_VideoDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    default:
        ret = Exynos_OMX_VideoDecodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE  hComponent,
    OMX_IN  OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE   *pIndexType)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if ((cParameterName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_ENABLE_THUMBNAIL) == 0) {
        EXYNOS_MPEG2DEC_HANDLE *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        *pIndexType = OMX_IndexVendorThumbnailMode;
        ret = OMX_ErrorNone;
    } else {
        ret = Exynos_OMX_VideoDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_MPEG2_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_Mpeg2Dec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_MPEG2DEC_HANDLE          *pMpeg2Dec           = (EXYNOS_MPEG2DEC_HANDLE *)pVideoDec->hCodecHandle;
    OMX_PTR                hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;

    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    CSC_METHOD csc_method = CSC_METHOD_SW;
    int i, plane;

    FunctionIn();

    pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCSrc = OMX_FALSE;
    pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCDst = OMX_FALSE;
    pExynosComponent->bUseFlagEOF = OMX_TRUE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;

    /* Mpeg2 Codec Open */
    ret = Mpeg2CodecOpen(pMpeg2Dec);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_OSAL_SemaphoreCreate(&pExynosInputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            pVideoDec->pMFCDecInputBuffer[i] = Exynos_OSAL_Malloc(sizeof(CODEC_DEC_BUFFER));
            Exynos_OSAL_Memset(pVideoDec->pMFCDecInputBuffer[i], 0, sizeof(CODEC_DEC_BUFFER));
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]);

            for (plane = 0; plane < MFC_INPUT_BUFFER_PLANE; plane++) {
            /* Use ION Allocator */
                pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane] = (void *)Exynos_OSAL_SharedMemory_Alloc(pVideoDec->hSharedMemory, DEFAULT_MFC_INPUT_BUFFER_SIZE, NORMAL_MEMORY);
                pVideoDec->pMFCDecInputBuffer[i]->fd[plane] = Exynos_OSAL_SharedMemory_VirtToION(pVideoDec->hSharedMemory, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane]);
                pVideoDec->pMFCDecInputBuffer[i]->bufferSize[plane] = DEFAULT_MFC_INPUT_BUFFER_SIZE;
            pVideoDec->pMFCDecInputBuffer[i]->dataSize   = 0;
                if (pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane] == NULL) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Fail input buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]->pVirAddr[%d]: 0x%x", i, plane, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane]);
            }

            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
        }
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_OSAL_SemaphoreCreate(&pExynosOutputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosOutputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pMpeg2Dec->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pMpeg2Dec->hSourceStartEvent);
    pMpeg2Dec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pMpeg2Dec->hDestinationStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp = 0;
    pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

#if 0//defined(USE_CSC_GSCALER)
    csc_method = CSC_METHOD_HW; //in case of Use ION buffer.
#endif
    pVideoDec->csc_handle = csc_init(csc_method);
    if (pVideoDec->csc_handle == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pVideoDec->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Terminate */
OMX_ERRORTYPE Exynos_Mpeg2Dec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE          ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_MPEG2DEC_HANDLE    *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    OMX_PTR                hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;

    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;

    int i, plane;

    FunctionIn();

    if (pVideoDec->csc_handle != NULL) {
        csc_deinit(pVideoDec->csc_handle);
        pVideoDec->csc_handle = NULL;
    }

    Exynos_OSAL_SignalTerminate(pMpeg2Dec->hDestinationStartEvent);
    pMpeg2Dec->hDestinationStartEvent = NULL;
    pMpeg2Dec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalTerminate(pMpeg2Dec->hSourceStartEvent);
    pMpeg2Dec->hSourceStartEvent = NULL;
    pMpeg2Dec->bSourceStart = OMX_FALSE;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            if (pVideoDec->pMFCDecOutputBuffer[i] != NULL) {
                for (plane = 0; plane < MFC_OUTPUT_BUFFER_PLANE; plane++) {
                    if (pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane] != NULL)
                        Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[plane]);
                }

                Exynos_OSAL_Free(pVideoDec->pMFCDecOutputBuffer[i]);
                pVideoDec->pMFCDecOutputBuffer[i] = NULL;
            }
        }

        Exynos_OSAL_QueueTerminate(&pExynosOutputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosOutputPort->codecSemID);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            if (pVideoDec->pMFCDecInputBuffer[i] != NULL) {
                for (plane = 0; plane < MFC_INPUT_BUFFER_PLANE; plane++) {
                    if (pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane] != NULL)
                        Exynos_OSAL_SharedMemory_Free(pVideoDec->hSharedMemory, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[plane]);
                }

            Exynos_OSAL_Free(pVideoDec->pMFCDecInputBuffer[i]);
            pVideoDec->pMFCDecInputBuffer[i] = NULL;
        }
        }

        Exynos_OSAL_QueueTerminate(&pExynosInputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosInputPort->codecSemID);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }
    Mpeg2CodecClose(pMpeg2Dec);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_SrcIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE               ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32  oneFrameSize = pSrcInputData->dataLen;
    OMX_BOOL bInStartCode = OMX_FALSE;
    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;
    ExynosVideoErrorType codecReturn = VIDEO_ERROR_NONE;
    int i;

    FunctionIn();

    if (pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = Mpeg2CodecSrcSetup(pOMXComponent, pSrcInputData);
        goto EXIT;
    }
    if (pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCDst == OMX_FALSE) {
        ret = Mpeg2CodecDstSetup(pOMXComponent);
    }

    if (((bInStartCode = Check_Mpeg2_StartCode(pSrcInputData->buffer.singlePlaneBuffer.dataBuffer, oneFrameSize)) == OMX_TRUE) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp] = pSrcInputData->timeStamp;
        pExynosComponent->nFlags[pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp] = pSrcInputData->nFlags;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "input timestamp %lld us (%.2f secs), Tag: %d, nFlags: 0x%x", pSrcInputData->timeStamp, pSrcInputData->timeStamp / 1E6, pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp, pSrcInputData->nFlags);
        pDecOps->Set_FrameTag(hMFCHandle, pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp);
        pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp++;
        pMpeg2Dec->hMFCMpeg2Handle.indexTimestamp %= MAX_TIMESTAMP;

        /* queue work for input buffer */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d, bufferHeader: 0x%x, dataBuffer: 0x%x", oneFrameSize, pSrcInputData->bufferHeader, pSrcInputData->buffer.singlePlaneBuffer.dataBuffer);
        codecReturn = pInbufOps->Enqueue(hMFCHandle, (unsigned char **)&pSrcInputData->buffer.singlePlaneBuffer.dataBuffer,
                                    (unsigned int *)&oneFrameSize, MFC_INPUT_BUFFER_PLANE, pSrcInputData->bufferHeader);
        if (codecReturn != VIDEO_ERROR_NONE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
            goto EXIT;
        }
        Mpeg2CodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pMpeg2Dec->bSourceStart == OMX_FALSE) {
            pMpeg2Dec->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pMpeg2Dec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
        if (pMpeg2Dec->bDestinationStart == OMX_FALSE) {
            pMpeg2Dec->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pMpeg2Dec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    } else if (bInStartCode == OMX_FALSE) {
        ret = OMX_ErrorCorruptedFrame;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_SrcOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT     *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pMpeg2Dec->hMFCMpeg2Handle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer;

    FunctionIn();

    pVideoBuffer = pInbufOps->Dequeue(hMFCHandle);

    pSrcOutputData->dataLen       = 0;
    pSrcOutputData->usedDataLen   = 0;
    pSrcOutputData->remainDataLen = 0;
    pSrcOutputData->nFlags    = 0;
    pSrcOutputData->timeStamp = 0;

    if (pVideoBuffer == NULL) {
        pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer = NULL;
        pSrcOutputData->allocSize  = 0;
        pSrcOutputData->pPrivate = NULL;
        pSrcOutputData->bufferHeader = NULL;
    } else {
        pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer = pVideoBuffer->planes[0].addr;
        pSrcOutputData->buffer.singlePlaneBuffer.fd = pVideoBuffer->planes[0].fd;
        pSrcOutputData->allocSize  = pVideoBuffer->planes[0].allocSize;

        if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
            int i = 0;
            while (pSrcOutputData->buffer.singlePlaneBuffer.dataBuffer != pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]) {
                if (i >= MFC_INPUT_BUFFER_NUM_MAX) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not find buffer");
                    ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
                    goto EXIT;
                }
                i++;
            }
            pVideoDec->pMFCDecInputBuffer[i]->dataSize = 0;
            pSrcOutputData->pPrivate = pVideoDec->pMFCDecInputBuffer[i];
        }

        /* For Share Buffer */
        pSrcOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE*)pVideoBuffer->pPrivate;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_DstIn(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;
    OMX_U32 dataLen[MFC_OUTPUT_BUFFER_PLANE] = {0,};
    ExynosVideoErrorType codecReturn = VIDEO_ERROR_NONE;

    FunctionIn();

    if (pDstInputData->buffer.multiPlaneBuffer.dataBuffer[0] == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to find input buffer");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "%s : %d => ADDR[0]: 0x%x, ADDR[1]: 0x%x", __FUNCTION__, __LINE__,
                                        pDstInputData->buffer.multiPlaneBuffer.dataBuffer[0],
                                        pDstInputData->buffer.multiPlaneBuffer.dataBuffer[1]);

    codecReturn = pOutbufOps->Enqueue(hMFCHandle, (unsigned char **)pDstInputData->buffer.multiPlaneBuffer.dataBuffer,
                     (unsigned int *)dataLen, MFC_OUTPUT_BUFFER_PLANE, pDstInputData->bufferHeader);

    if (codecReturn != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
        goto EXIT;
    }
    Mpeg2CodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_DstOut(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_MPEG2DEC_HANDLE         *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    void                          *hMFCHandle = pMpeg2Dec->hMFCMpeg2Handle.hMFCHandle;
    EXYNOS_OMX_BASEPORT *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps       *pDecOps    = pMpeg2Dec->hMFCMpeg2Handle.pDecOps;
    ExynosVideoDecBufferOps *pOutbufOps = pMpeg2Dec->hMFCMpeg2Handle.pOutbufOps;
    ExynosVideoBuffer       *pVideoBuffer = NULL;
    ExynosVideoFrameStatusType displayStatus = VIDEO_FRAME_STATUS_UNKNOWN;
    ExynosVideoGeometry *bufferGeometry;
    DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo = NULL;
    OMX_S32 indexTimestamp = 0;
    int plane;

    FunctionIn();

    if (pMpeg2Dec->bDestinationStart == OMX_FALSE) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    while (1) {
        if ((pVideoBuffer = pOutbufOps->Dequeue(hMFCHandle)) == NULL) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
        displayStatus = pVideoBuffer->displayStatus;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus: 0x%x", displayStatus);

        if ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
            (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
            (displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
            (CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            ret = OMX_ErrorNone;
            break;
        }
    }

    pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp++;
    pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp %= MAX_TIMESTAMP;

    pDstOutputData->allocSize = pDstOutputData->dataLen = 0;
    for (plane = 0; plane < MFC_OUTPUT_BUFFER_PLANE; plane++) {
        pDstOutputData->buffer.multiPlaneBuffer.dataBuffer[plane] = pVideoBuffer->planes[plane].addr;
        pDstOutputData->buffer.multiPlaneBuffer.fd[plane] = pVideoBuffer->planes[plane].fd;
        pDstOutputData->allocSize += pVideoBuffer->planes[plane].allocSize;
        pDstOutputData->dataLen +=  pVideoBuffer->planes[plane].dataSize;
    }
    pDstOutputData->usedDataLen = 0;
    pDstOutputData->pPrivate = pVideoBuffer;
    /* For Share Buffer */
    pDstOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE *)pVideoBuffer->pPrivate;

    pBufferInfo = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
    bufferGeometry = &pMpeg2Dec->hMFCMpeg2Handle.codecOutbufConf;
    pBufferInfo->imageWidth = bufferGeometry->nFrameWidth;
    pBufferInfo->imageHeight = bufferGeometry->nFrameHeight;
    switch (bufferGeometry->eColorFormat) {
    case VIDEO_COLORFORMAT_NV12:
        pBufferInfo->ColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
    case VIDEO_COLORFORMAT_NV12_TILED:
    default:
        pBufferInfo->ColorFormat = OMX_SEC_COLOR_FormatNV12Tiled;
        break;
    }

    indexTimestamp = pDecOps->Get_FrameTag(hMFCHandle);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "out indexTimestamp: %d", indexTimestamp);
    if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp != OMX_TRUE) &&
            (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp != OMX_TRUE)) {
            pDstOutputData->timeStamp = pExynosComponent->timeStamp[pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp];
            pDstOutputData->nFlags = pExynosComponent->nFlags[pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp];
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "missing out indexTimestamp: %d", indexTimestamp);
        } else {
            pDstOutputData->timeStamp = 0x00;
            pDstOutputData->nFlags = 0x00;
        }
    } else {
        /* For timestamp correction. if mfc support frametype detect */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "disp_pic_frame_type: %d", pVideoBuffer->frameType);
#ifdef NEED_TIMESTAMP_REORDER
        if ((pVideoBuffer->frameType == VIDEO_FRAME_I)) {
            pDstOutputData->timeStamp = pExynosComponent->timeStamp[indexTimestamp];
            pDstOutputData->nFlags = pExynosComponent->nFlags[indexTimestamp];
            pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp = indexTimestamp;
        } else {
            pDstOutputData->timeStamp = pExynosComponent->timeStamp[pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp];
            pDstOutputData->nFlags = pExynosComponent->nFlags[pMpeg2Dec->hMFCMpeg2Handle.outputIndexTimestamp];
        }
#else
        pDstOutputData->timeStamp = pExynosComponent->timeStamp[indexTimestamp];
        pDstOutputData->nFlags = pExynosComponent->nFlags[indexTimestamp];
#endif
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "timestamp %lld us (%.2f secs), indexTimestamp: %d, nFlags: 0x%x", pDstOutputData->timeStamp, pDstOutputData->timeStamp / 1E6, indexTimestamp, pDstOutputData->nFlags);
    }

    if ((displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
        ((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus:%d, nFlags0x%x", displayStatus, pDstOutputData->nFlags);
        pDstOutputData->remainDataLen = 0;
    } else {
        pDstOutputData->remainDataLen = bufferGeometry->nFrameWidth * bufferGeometry->nFrameHeight * 3 / 2;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_srcInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcInputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_MPEG2DEC_HANDLE    *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = Exynos_Mpeg2Dec_SrcIn(pOMXComponent, pSrcInputData);
    if ((ret != OMX_ErrorNone) &&
        (ret != OMX_ErrorInputDataDecodeYet) &&
        (ret != OMX_ErrorCorruptedFrame)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_srcOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pSrcOutputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_MPEG2DEC_HANDLE    *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT     *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    if ((pMpeg2Dec->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosInputPort))) {
        Exynos_OSAL_SignalWait(pMpeg2Dec->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        Exynos_OSAL_SignalReset(pMpeg2Dec->hSourceStartEvent);
    }

    ret = Exynos_Mpeg2Dec_SrcOut(pOMXComponent, pSrcOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_dstInputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstInputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_MPEG2DEC_HANDLE    *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pMpeg2Dec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pMpeg2Dec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pMpeg2Dec->hDestinationStartEvent);
        }
    }
    if (pMpeg2Dec->hMFCMpeg2Handle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_Mpeg2Dec_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_Mpeg2Dec_dstOutputBufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pDstOutputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_MPEG2DEC_HANDLE    *pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT     *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pMpeg2Dec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pMpeg2Dec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pMpeg2Dec->hDestinationStartEvent);
        }
    }
    ret = Exynos_Mpeg2Dec_DstOut(pOMXComponent, pDstOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      componentName)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_MPEG2DEC_HANDLE            *pMpeg2Dec            = NULL;
    int i = 0;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_MPEG2_DEC, componentName) != 0) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, componentName:%s, Line:%d", componentName, __LINE__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_VideoDecodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = HW_VIDEO_DEC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pMpeg2Dec = Exynos_OSAL_Malloc(sizeof(EXYNOS_MPEG2DEC_HANDLE));
    if (pMpeg2Dec == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pMpeg2Dec, 0, sizeof(EXYNOS_MPEG2DEC_HANDLE));
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoDec->hCodecHandle = (OMX_HANDLETYPE)pMpeg2Dec;

    Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_MPEG2_DEC);

    /* Set componentVersion */
    pExynosComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pExynosComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->specVersion.s.nStep         = STEP_NUMBER;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/mpeg2");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    //pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->bufferProcessType = BUFFER_COPY;
    pExynosPort->portWayType = WAY2_PORT;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight= DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY | BUFFER_ANBSHARE;
    pExynosPort->portWayType = WAY2_PORT;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pMpeg2Dec->Mpeg2Component[i], OMX_VIDEO_PARAM_MPEG2TYPE);
        pMpeg2Dec->Mpeg2Component[i].nPortIndex = i;
        pMpeg2Dec->Mpeg2Component[i].eProfile = OMX_VIDEO_MPEG2ProfileMain;
        pMpeg2Dec->Mpeg2Component[i].eLevel = OMX_VIDEO_MPEG2LevelML; /* Check again**** */
    }

    pOMXComponent->GetParameter      = &Exynos_Mpeg2Dec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_Mpeg2Dec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_Mpeg2Dec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_Mpeg2Dec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_Mpeg2Dec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_Mpeg2Dec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_Mpeg2Dec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_Mpeg2Dec_Terminate;

    pVideoDec->exynos_codec_srcInputProcess  = &Exynos_Mpeg2Dec_srcInputBufferProcess;
    pVideoDec->exynos_codec_srcOutputProcess = &Exynos_Mpeg2Dec_srcOutputBufferProcess;
    pVideoDec->exynos_codec_dstInputProcess  = &Exynos_Mpeg2Dec_dstInputBufferProcess;
    pVideoDec->exynos_codec_dstOutputProcess = &Exynos_Mpeg2Dec_dstOutputBufferProcess;

    pVideoDec->exynos_codec_start         = &Mpeg2CodecStart;
    pVideoDec->exynos_codec_stop          = &Mpeg2CodecStop;
    pVideoDec->exynos_codec_bufferProcessRun = &Mpeg2CodecOutputBufferProcessRun;
    pVideoDec->exynos_codec_enqueueAllBuffer = &Mpeg2CodecEnQueueAllBuffer;

    pVideoDec->exynos_checkInputFrame                 = &Check_Mpeg2_Frame;
    pVideoDec->exynos_codec_getCodecInputPrivateData  = &GetCodecInputPrivateData;
    pVideoDec->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;

    pVideoDec->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        Exynos_OSAL_Free(pMpeg2Dec);
        pMpeg2Dec = ((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(
    OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec = NULL;
    EXYNOS_MPEG2DEC_HANDLE        *pMpeg2Dec            = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_SharedMemory_Close(pVideoDec->hSharedMemory);

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pMpeg2Dec = (EXYNOS_MPEG2DEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pMpeg2Dec != NULL) {
        Exynos_OSAL_Free(pMpeg2Dec);
        pMpeg2Dec = pVideoDec->hCodecHandle = NULL;
    }

    ret = Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
