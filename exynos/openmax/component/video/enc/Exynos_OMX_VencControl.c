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
 * @file        Exynos_OMX_VencControl.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2012.02.20 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Exynos_OMX_Macros.h"
#include "Exynos_OSAL_Event.h"
#include "Exynos_OMX_Venc.h"
#include "Exynos_OMX_VencControl.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OSAL_Thread.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Mutex.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_SharedMemory.h"

#ifdef USE_ANB
#include "Exynos_OSAL_Android.h"
#endif

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_VIDEO_ENCCONTROL"
#define EXYNOS_LOG_OFF
//#define EXYNOS_TRACE_ON
#include "Exynos_OSAL_Log.h"


OMX_ERRORTYPE Exynos_OMX_UseBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes,
    OMX_IN OMX_U8                   *pBuffer)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pTempBufferHdr    = NULL;
    OMX_U32                   i                 = 0;

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

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (pExynosPort->portState != OMX_StateIdle) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ASSIGNED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);
            pTempBufferHdr->pBuffer        = pBuffer;
            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;
            pExynosPort->assignedBufferNum++;
            if (pExynosPort->assignedBufferNum == pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;
                /* Exynos_OSAL_MutexLock(pExynosComponent->compMutex); */
                Exynos_OSAL_SemaphorePost(pExynosPort->loadedResource);
                /* Exynos_OSAL_MutexUnlock(pExynosComponent->compMutex); */
            }
            *ppBufferHdr = pTempBufferHdr;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pTempBufferHdr);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_AllocateBuffer(
    OMX_IN OMX_HANDLETYPE            hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE **ppBufferHdr,
    OMX_IN OMX_U32                   nPortIndex,
    OMX_IN OMX_PTR                   pAppPrivate,
    OMX_IN OMX_U32                   nSizeBytes)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pTempBufferHdr     = NULL;
    OMX_U8                          *pTempBuffer        = NULL;
    int                              fdTempBuffer       = -1;
    MEMORY_TYPE                      eMemType;
    OMX_U32                          i                  = 0;

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

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
/*
    if (pExynosPort->portState != OMX_StateIdle ) {
        ret = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }
*/
    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (pExynosPort->bufferProcessType & BUFFER_SHARE)
        eMemType = NORMAL_MEMORY;
    else
        eMemType = SYSTEM_MEMORY;

    pTempBuffer = Exynos_OSAL_SharedMemory_Alloc(pVideoEnc->hSharedMemory, nSizeBytes, eMemType);
    if (pTempBuffer == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    fdTempBuffer = Exynos_OSAL_SharedMemory_VirtToION(pVideoEnc->hSharedMemory, pTempBuffer);

    pTempBufferHdr = (OMX_BUFFERHEADERTYPE *)Exynos_OSAL_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
    if (pTempBufferHdr == NULL) {
        Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pTempBuffer);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    Exynos_OSAL_Memset(pTempBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

    for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
        if (pExynosPort->bufferStateAllocate[i] == BUFFER_STATE_FREE) {
            pExynosPort->extendBufferHeader[i].OMXBufferHeader = pTempBufferHdr;
            pExynosPort->extendBufferHeader[i].buf_fd[0] = fdTempBuffer;
            pExynosPort->bufferStateAllocate[i] = (BUFFER_STATE_ALLOCATED | HEADER_STATE_ALLOCATED);
            INIT_SET_SIZE_VERSION(pTempBufferHdr, OMX_BUFFERHEADERTYPE);
            pTempBufferHdr->pBuffer        = pTempBuffer;
            pTempBufferHdr->nAllocLen      = nSizeBytes;
            pTempBufferHdr->pAppPrivate    = pAppPrivate;
            if (nPortIndex == INPUT_PORT_INDEX)
                pTempBufferHdr->nInputPortIndex = INPUT_PORT_INDEX;
            else
                pTempBufferHdr->nOutputPortIndex = OUTPUT_PORT_INDEX;
            pExynosPort->assignedBufferNum++;
            if (pExynosPort->assignedBufferNum == pExynosPort->portDefinition.nBufferCountActual) {
                pExynosPort->portDefinition.bPopulated = OMX_TRUE;
                /* Exynos_OSAL_MutexLock(pExynosComponent->compMutex); */
                Exynos_OSAL_SemaphorePost(pExynosPort->loadedResource);
                /* Exynos_OSAL_MutexUnlock(pExynosComponent->compMutex); */
            }
            *ppBufferHdr = pTempBufferHdr;
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pTempBufferHdr);
    Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pTempBuffer);
    ret = OMX_ErrorInsufficientResources;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeBuffer(
    OMX_IN OMX_HANDLETYPE        hComponent,
    OMX_IN OMX_U32               nPortIndex,
    OMX_IN OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE            *pOMXBufferHdr      = NULL;
    OMX_U32                          i                  = 0;

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

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    if (CHECK_PORT_TUNNELED(pExynosPort) && CHECK_PORT_BUFFER_SUPPLIER(pExynosPort)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((pExynosPort->portState != OMX_StateLoaded) &&
        (pExynosPort->portState != OMX_StateInvalid)) {
        (*(pExynosComponent->pCallbacks->EventHandler)) (pOMXComponent,
                        pExynosComponent->callbackData,
                        (OMX_U32)OMX_EventError,
                        (OMX_U32)OMX_ErrorPortUnpopulated,
                        nPortIndex, NULL);
    }

    for (i = 0; i < /*pExynosPort->portDefinition.nBufferCountActual*/MAX_BUFFER_NUM; i++) {
        if ((pExynosPort->bufferStateAllocate[i] != BUFFER_STATE_FREE) &&
            (pExynosPort->extendBufferHeader[i].OMXBufferHeader != NULL)) {
            pOMXBufferHdr = pExynosPort->extendBufferHeader[i].OMXBufferHeader;

            if (pOMXBufferHdr->pBuffer == pBufferHdr->pBuffer) {
                if (pExynosPort->bufferStateAllocate[i] & BUFFER_STATE_ALLOCATED) {
                    Exynos_OSAL_SharedMemory_Free(pVideoEnc->hSharedMemory, pOMXBufferHdr->pBuffer);
                    pOMXBufferHdr->pBuffer = NULL;
                    pBufferHdr->pBuffer = NULL;
                } else if (pExynosPort->bufferStateAllocate[i] & BUFFER_STATE_ASSIGNED) {
                    ; /* None*/
                }
                pExynosPort->assignedBufferNum--;

                if (pExynosPort->bufferStateAllocate[i] & HEADER_STATE_ALLOCATED) {
                    Exynos_OSAL_Free(pOMXBufferHdr);
                    pExynosPort->extendBufferHeader[i].OMXBufferHeader = NULL;
                    pBufferHdr = NULL;
                }

                pExynosPort->bufferStateAllocate[i] = BUFFER_STATE_FREE;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }
    }

EXIT:
    if ((ret == OMX_ErrorNone) &&
        (pExynosPort->assignedBufferNum == 0)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pExynosPort->unloadedResource signal set");
        /* Exynos_OSAL_MutexLock(pExynosComponent->compMutex); */
        Exynos_OSAL_SemaphorePost(pExynosPort->unloadedResource);
        /* Exynos_OSAL_MutexUnlock(pExynosComponent->compMutex); */
        pExynosPort->portDefinition.bPopulated = OMX_FALSE;
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_AllocateTunnelBuffer(
    EXYNOS_OMX_BASEPORT     *pOMXBasePort,
    OMX_U32                  nPortIndex)
{
    OMX_ERRORTYPE                 ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT          *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE         *pTempBufferHdr    = NULL;
    OMX_U8                       *pTempBuffer       = NULL;
    OMX_U32                       nBufferSize       = 0;
    OMX_PARAM_PORTDEFINITIONTYPE  portDefinition;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FreeTunnelBuffer(
    EXYNOS_OMX_BASEPORT     *pOMXBasePort,
    OMX_U32                  nPortIndex)
{
    OMX_ERRORTYPE            ret            = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT     *pExynosPort    = NULL;
    OMX_BUFFERHEADERTYPE    *pTempBufferHdr = NULL;
    OMX_U8                  *pTempBuffer    = NULL;
    OMX_U32                  nBufferSize    = 0;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentTunnelRequest(
    OMX_IN OMX_HANDLETYPE hComp,
    OMX_IN OMX_U32        nPort,
    OMX_IN OMX_HANDLETYPE hTunneledComp,
    OMX_IN OMX_U32        nTunneledPort,
    OMX_INOUT OMX_TUNNELSETUPTYPE *pTunnelSetup)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = OMX_ErrorTunnelingUnsupported;
EXIT:
    return ret;
}

OMX_ERRORTYPE Exynos_OMX_GetFlushBuffer(
    EXYNOS_OMX_BASEPORT     *pExynosPort,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer[])
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    FunctionIn();

    *pDataBuffer = NULL;

    if (pExynosPort->portWayType == WAY1_PORT) {
        *pDataBuffer = &pExynosPort->way.port1WayDataBuffer.dataBuffer;
    } else if (pExynosPort->portWayType == WAY2_PORT) {
        pDataBuffer[0] = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
        pDataBuffer[1] = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_FlushPort(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_S32              nPortIndex)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;
    EXYNOS_OMX_DATABUFFER    *pDataBuffer[2]    = {NULL, NULL};
    EXYNOS_OMX_MESSAGE       *pMessage          = NULL;
    OMX_S32                   nSemaCnt          = 0;
    int                       i                 = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if ((nPortIndex < 0) ||
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    while (Exynos_OSAL_GetElemNum(&pExynosPort->bufferQ) > 0) {
        Exynos_OSAL_Get_SemaphoreCount(pExynosPort->bufferSemID, &nSemaCnt);
        if (nSemaCnt == 0)
            Exynos_OSAL_SemaphorePost(pExynosPort->bufferSemID);

        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if ((pMessage != NULL) &&
            (pMessage->messageType != EXYNOS_OMX_CommandFakeBuffer)) {
            pBufferHdr = (OMX_BUFFERHEADERTYPE *)pMessage->pCmdData;
            pBufferHdr->nFilledLen = 0;

            if (nPortIndex == OUTPUT_PORT_INDEX) {
                Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
            } else if (nPortIndex == INPUT_PORT_INDEX) {
                Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
            }
        }
        Exynos_OSAL_Free(pMessage);
        pMessage = NULL;
    }

    Exynos_OMX_GetFlushBuffer(pExynosPort, pDataBuffer);
    if ((pDataBuffer[0] != NULL) &&
        (pDataBuffer[0]->dataValid == OMX_TRUE)) {
        if (nPortIndex == INPUT_PORT_INDEX)
            Exynos_FlushInputBufferReturn(pOMXComponent, pDataBuffer[0]);
        else if (nPortIndex == OUTPUT_PORT_INDEX)
            Exynos_FlushOutputBufferReturn(pOMXComponent, pDataBuffer[0]);
    }
    if ((pDataBuffer[1] != NULL) &&
        (pDataBuffer[1]->dataValid == OMX_TRUE)) {
        if (nPortIndex == INPUT_PORT_INDEX)
            Exynos_FlushInputBufferReturn(pOMXComponent, pDataBuffer[1]);
        else if (nPortIndex == OUTPUT_PORT_INDEX)
            Exynos_FlushOutputBufferReturn(pOMXComponent, pDataBuffer[1]);
    }

    if (pExynosComponent->bMultiThreadProcess == OMX_TRUE) {
        if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
            if (pExynosPort->processData.bufferHeader != NULL) {
                if (nPortIndex == INPUT_PORT_INDEX) {
                    Exynos_OMX_InputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
                } else if (nPortIndex == OUTPUT_PORT_INDEX) {
                    Exynos_OMX_OutputBufferReturn(pOMXComponent, pExynosPort->processData.bufferHeader);
                }
            }
            Exynos_ResetCodecData(&pExynosPort->processData);

            for (i = 0; i < pExynosPort->portDefinition.nBufferCountActual; i++) {
                if (pExynosPort->extendBufferHeader[i].bBufferInOMX == OMX_TRUE) {
                    if (nPortIndex == OUTPUT_PORT_INDEX) {
                        Exynos_OMX_OutputBufferReturn(pOMXComponent,
                                                      pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                    } else if (nPortIndex == INPUT_PORT_INDEX) {
                        Exynos_OMX_InputBufferReturn(pOMXComponent,
                                                     pExynosPort->extendBufferHeader[i].OMXBufferHeader);
                    }
                }
            }
        }
    } else {
        Exynos_ResetCodecData(&pExynosPort->processData);
    }

    while (1) {
        OMX_S32 cnt = 0;
        Exynos_OSAL_Get_SemaphoreCount(pExynosPort->bufferSemID, &cnt);
        if (cnt <= 0)
            break;
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
    }
    Exynos_OSAL_ResetQueue(&pExynosPort->bufferQ);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_BufferFlush(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_S32              nPortIndex,
    OMX_BOOL             bEvent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc          = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_DATABUFFER           *pDataBuffer[2]     = {NULL, NULL};
    OMX_U32                          i                  = 0;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    if (pExynosComponent->hComponentHandle == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

    if ((nPortIndex < 0) ||
        (nPortIndex >= pExynosComponent->portParam.nPorts)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE,"OMX_CommandFlush start, port:%d", nPortIndex);

    pExynosPort->bIsPortFlushed = OMX_TRUE;

    if (pExynosComponent->bMultiThreadProcess == OMX_FALSE) {
        Exynos_OSAL_SignalSet(pExynosComponent->pauseEvent);
    } else {
        Exynos_OSAL_SignalSet(pExynosPort->pauseEvent);
    }

    Exynos_OMX_GetFlushBuffer(pExynosPort, pDataBuffer);
    if (pDataBuffer[0] == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosPort->bufferProcessType & BUFFER_COPY)
        Exynos_OSAL_SemaphorePost(pExynosPort->codecSemID);
    Exynos_OSAL_SemaphorePost(pExynosPort->bufferSemID);

    pVideoEnc->exynos_codec_bufferProcessRun(pOMXComponent, nPortIndex);

    Exynos_OSAL_MutexLock(pDataBuffer[0]->bufferMutex);
    pVideoEnc->exynos_codec_stop(pOMXComponent, nPortIndex);

    if (pDataBuffer[1] != NULL)
        Exynos_OSAL_MutexLock(pDataBuffer[1]->bufferMutex);

    ret = Exynos_OMX_FlushPort(pOMXComponent, nPortIndex);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    if (pExynosPort->bufferProcessType & BUFFER_COPY)
        pVideoEnc->exynos_codec_enqueueAllBuffer(pOMXComponent, nPortIndex);
    Exynos_ResetCodecData(&pExynosPort->processData);

    if (ret == OMX_ErrorNone) {
        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosComponent->checkTimeStamp.needSetStartTimeStamp = OMX_TRUE;
            pExynosComponent->checkTimeStamp.needCheckStartTimeStamp = OMX_FALSE;
            Exynos_OSAL_Memset(pExynosComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
            Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
            pExynosComponent->getAllDelayBuffer = OMX_FALSE;
            pExynosComponent->bSaveFlagEOS = OMX_FALSE;
            pExynosComponent->reInputData = OMX_FALSE;
        }

        pExynosPort->bIsPortFlushed = OMX_FALSE;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE,"OMX_CommandFlush EventCmdComplete, port:%d", nPortIndex);
        if (bEvent == OMX_TRUE)
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                            pExynosComponent->callbackData,
                            OMX_EventCmdComplete,
                            OMX_CommandFlush, nPortIndex, NULL);
    }

    if (pDataBuffer[1] != NULL)
        Exynos_OSAL_MutexUnlock(pDataBuffer[1]->bufferMutex);

    Exynos_OSAL_MutexUnlock(pDataBuffer[0]->bufferMutex);

EXIT:
    if ((ret != OMX_ErrorNone) && (pOMXComponent != NULL) && (pExynosComponent != NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR,"%s : %d", __FUNCTION__, __LINE__);
        pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                        pExynosComponent->callbackData,
                        OMX_EventError,
                        ret, 0, NULL);
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_InputBufferReturn(
    OMX_COMPONENTTYPE   *pOMXComponent)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
    EXYNOS_OMX_DATABUFFER       *pDataBuffer        = NULL;
    OMX_BUFFERHEADERTYPE        *pBufferHdr         = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);

    if (pExynosPort->bufferProcessType & BUFFER_COPY) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
    } else if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    }

    if (pDataBuffer != NULL)
        pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        if (pExynosPort->markType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent = pExynosPort->markType.hMarkTargetComponent;
            pBufferHdr->pMarkData            = pExynosPort->markType.pMarkData;
            pExynosPort->markType.hMarkTargetComponent = NULL;
            pExynosPort->markType.pMarkData = NULL;
        }

        if (pBufferHdr->hMarkTargetComponent != NULL) {
            if (pBufferHdr->hMarkTargetComponent == pOMXComponent) {
                pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                pExynosComponent->callbackData,
                                OMX_EventMark,
                                0, 0, pBufferHdr->pMarkData);
            } else {
                pExynosComponent->propagateMarkType.hMarkTargetComponent = pBufferHdr->hMarkTargetComponent;
                pExynosComponent->propagateMarkType.pMarkData = pBufferHdr->pMarkData;
            }
        }

        pBufferHdr->nFilledLen = 0;
        pBufferHdr->nOffset = 0;
        Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FlushInputBufferReturn(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer)
{
    OMX_ERRORTYPE                ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT         *pExynosPort        = NULL;
    OMX_BUFFERHEADERTYPE        *pBufferHdr         = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);

    pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        if (pExynosPort->markType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent  = pExynosPort->markType.hMarkTargetComponent;
            pBufferHdr->pMarkData             = pExynosPort->markType.pMarkData;
            pExynosPort->markType.hMarkTargetComponent = NULL;
            pExynosPort->markType.pMarkData = NULL;
        }

        if (pBufferHdr->hMarkTargetComponent != NULL) {
            if (pBufferHdr->hMarkTargetComponent == pOMXComponent) {
                pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                                pExynosComponent->callbackData,
                                OMX_EventMark,
                                0, 0, pBufferHdr->pMarkData);
            } else {
                pExynosComponent->propagateMarkType.hMarkTargetComponent = pBufferHdr->hMarkTargetComponent;
                pExynosComponent->propagateMarkType.pMarkData = pBufferHdr->pMarkData;
            }
        }

        pBufferHdr->nFilledLen = 0;
        pBufferHdr->nOffset = 0;
        Exynos_OMX_InputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_InputBufferGetQueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;
    EXYNOS_OMX_DATABUFFER *pDataBuffer  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[INPUT_PORT_INDEX]);
    pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        if (pDataBuffer->dataValid != OMX_TRUE) {
            pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (pMessage == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (pMessage->messageType == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(pMessage);
                ret = OMX_ErrorCodecFlush;
                goto EXIT;
            }

            pDataBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
            pDataBuffer->allocSize     = pDataBuffer->bufferHeader->nAllocLen;
            pDataBuffer->dataLen       = pDataBuffer->bufferHeader->nFilledLen;
            pDataBuffer->remainDataLen = pDataBuffer->dataLen;
            pDataBuffer->usedDataLen   = 0;
            pDataBuffer->dataValid     = OMX_TRUE;
            pDataBuffer->nFlags        = pDataBuffer->bufferHeader->nFlags;
            pDataBuffer->timeStamp     = pDataBuffer->bufferHeader->nTimeStamp;

            Exynos_OSAL_Free(pMessage);

            if (pDataBuffer->allocSize <= pDataBuffer->dataLen)
                Exynos_OSAL_Log(EXYNOS_LOG_WARNING, "Input Buffer Full, Check input buffer size! allocSize:%d, dataLen:%d", pDataBuffer->allocSize, pDataBuffer->dataLen);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OutputBufferReturn(
    OMX_COMPONENTTYPE   *pOMXComponent)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;
    EXYNOS_OMX_DATABUFFER    *pDataBuffer       = NULL;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        pBufferHdr->nFilledLen = pDataBuffer->remainDataLen;
        pBufferHdr->nOffset    = 0;
        pBufferHdr->nFlags     = pDataBuffer->nFlags;
        pBufferHdr->nTimeStamp = pDataBuffer->timeStamp;

        if (pExynosComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent = pExynosComponent->propagateMarkType.hMarkTargetComponent;
            pBufferHdr->pMarkData            = pExynosComponent->propagateMarkType.pMarkData;
            pExynosComponent->propagateMarkType.hMarkTargetComponent = NULL;
            pExynosComponent->propagateMarkType.pMarkData = NULL;
        }

        if ((pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            pBufferHdr->nFilledLen = 0;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "event OMX_BUFFERFLAG_EOS!!!");
            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                            pExynosComponent->callbackData,
                            OMX_EventBufferFlag,
                            OUTPUT_PORT_INDEX,
                            pBufferHdr->nFlags, NULL);
        }

        Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FlushOutputBufferReturn(
    OMX_COMPONENTTYPE       *pOMXComponent,
    EXYNOS_OMX_DATABUFFER   *pDataBuffer)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    OMX_BUFFERHEADERTYPE     *pBufferHdr        = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pBufferHdr = pDataBuffer->bufferHeader;

    if (pBufferHdr != NULL) {
        pBufferHdr->nFilledLen = pDataBuffer->remainDataLen;
        pBufferHdr->nOffset    = 0;
        pBufferHdr->nFlags     = pDataBuffer->nFlags;
        pBufferHdr->nTimeStamp = pDataBuffer->timeStamp;

        if (pExynosComponent->propagateMarkType.hMarkTargetComponent != NULL) {
            pBufferHdr->hMarkTargetComponent    = pExynosComponent->propagateMarkType.hMarkTargetComponent;
            pBufferHdr->pMarkData               = pExynosComponent->propagateMarkType.pMarkData;
            pExynosComponent->propagateMarkType.hMarkTargetComponent = NULL;
            pExynosComponent->propagateMarkType.pMarkData = NULL;
        }

        if ((pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) {
            pBufferHdr->nFilledLen = 0;
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "event OMX_BUFFERFLAG_EOS!!!");
            pExynosComponent->pCallbacks->EventHandler(pOMXComponent,
                            pExynosComponent->callbackData,
                            OMX_EventBufferFlag,
                            OUTPUT_PORT_INDEX,
                            pBufferHdr->nFlags, NULL);
        }
        Exynos_OMX_OutputBufferReturn(pOMXComponent, pBufferHdr);
    }

    /* reset dataBuffer */
    Exynos_ResetDataBuffer(pDataBuffer);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OutputBufferGetQueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorUndefined;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;
    EXYNOS_OMX_DATABUFFER *pDataBuffer  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosPort->bufferProcessType & BUFFER_COPY) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.outputDataBuffer);
    } else if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
        pDataBuffer = &(pExynosPort->way.port2WayDataBuffer.inputDataBuffer);
    }

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);
        if ((pDataBuffer != NULL) &&
            (pDataBuffer->dataValid != OMX_TRUE)) {
            pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
            if (pMessage == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }
            if (pMessage->messageType == EXYNOS_OMX_CommandFakeBuffer) {
                Exynos_OSAL_Free(pMessage);
                ret = OMX_ErrorCodecFlush;
                goto EXIT;
            }

            pDataBuffer->bufferHeader  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
            pDataBuffer->allocSize     = pDataBuffer->bufferHeader->nAllocLen;
            pDataBuffer->dataLen       = 0; //dataBuffer->bufferHeader->nFilledLen;
            pDataBuffer->remainDataLen = pDataBuffer->dataLen;
            pDataBuffer->usedDataLen   = 0; //dataBuffer->bufferHeader->nOffset;
            pDataBuffer->dataValid     = OMX_TRUE;
            /* pDataBuffer->nFlags             = pDataBuffer->bufferHeader->nFlags; */
            /* pDtaBuffer->nTimeStamp         = pDataBuffer->bufferHeader->nTimeStamp; */
/*
            if (pExynosPort->bufferProcessType & BUFFER_SHARE)
                pDataBuffer->pPrivate      = pDataBuffer->bufferHeader->pOutputPortPrivate;
            else if (pExynosPort->bufferProcessType & BUFFER_COPY) {
                pExynosPort->processData.dataBuffer = pDataBuffer->bufferHeader->pBuffer;
                pExynosPort->processData.allocSize  = pDataBuffer->bufferHeader->nAllocLen;
            }
*/
            Exynos_OSAL_Free(pMessage);
        }
        ret = OMX_ErrorNone;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_BUFFERHEADERTYPE *Exynos_OutputBufferGetQueue_Direct(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent)
{
    OMX_BUFFERHEADERTYPE  *pBufferHdr   = NULL;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;
    EXYNOS_OMX_MESSAGE    *pMessage     = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        pBufferHdr = NULL;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX]);

    if (pExynosComponent->currentState != OMX_StateExecuting) {
        pBufferHdr = NULL;
        goto EXIT;
    } else if ((pExynosComponent->transientState != EXYNOS_OMX_TransStateExecutingToIdle) &&
               (!CHECK_PORT_BEING_FLUSHED(pExynosPort))) {
        Exynos_OSAL_SemaphoreWait(pExynosPort->bufferSemID);

        pMessage = (EXYNOS_OMX_MESSAGE *)Exynos_OSAL_Dequeue(&pExynosPort->bufferQ);
        if (pMessage == NULL) {
            pBufferHdr = NULL;
            goto EXIT;
        }
        if (pMessage->messageType == EXYNOS_OMX_CommandFakeBuffer) {
            Exynos_OSAL_Free(pMessage);
            pBufferHdr = NULL;
            goto EXIT;
        }

        pBufferHdr  = (OMX_BUFFERHEADERTYPE *)(pMessage->pCmdData);
        Exynos_OSAL_Free(pMessage);
    }

EXIT:
    FunctionOut();

    return pBufferHdr;
}

OMX_ERRORTYPE Exynos_CodecBufferEnqueue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex,
    OMX_PTR                      pData)
{
    OMX_ERRORTYPE          ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

    if (pData == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    ret = Exynos_OSAL_Queue(&pExynosPort->codecBufferQ, (void *)pData);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }
    Exynos_OSAL_SemaphorePost(pExynosPort->codecSemID);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferDequeue(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex,
    OMX_PTR                     *pData)
{
    OMX_ERRORTYPE          ret         = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort = NULL;
    OMX_PTR                pTempData   = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

    Exynos_OSAL_SemaphoreWait(pExynosPort->codecSemID);
    pTempData = (OMX_PTR)Exynos_OSAL_Dequeue(&pExynosPort->codecBufferQ);
    if (pTempData != NULL) {
        *pData = (OMX_PTR)pTempData;
        ret = OMX_ErrorNone;
    } else {
        *pData = NULL;
        ret = OMX_ErrorUndefined;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_CodecBufferReset(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_U32                      nPortIndex)
{
    OMX_ERRORTYPE          ret          = OMX_ErrorNone;
    EXYNOS_OMX_BASEPORT   *pExynosPort  = NULL;

    FunctionIn();

    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex >= pExynosComponent->portParam.nPorts) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }
    pExynosPort = &(pExynosComponent->pExynosPort[nPortIndex]);

    ret = Exynos_OSAL_ResetQueue(&pExynosPort->codecBufferQ);
    if (ret != 0) {
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    while (1) {
        int cnt = 0;
        Exynos_OSAL_Get_SemaphoreCount(pExynosPort->codecSemID, &cnt);
        if (cnt > 0)
            Exynos_OSAL_SemaphoreWait(pExynosPort->codecSemID);
        else
            break;
    }
    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;

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

    if (pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamVideoInit:
    {
        OMX_PORT_PARAM_TYPE *pPortParam = (OMX_PORT_PARAM_TYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pPortParam->nPorts           = pExynosComponent->portParam.nPorts;
        pPortParam->nStartPortNumber = pExynosComponent->portParam.nStartPortNumber;
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat     = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex      = pPortFormat->nPortIndex;
        OMX_U32                         nIndex          = pPortFormat->nIndex;
        EXYNOS_OMX_BASEPORT            *pExynosPort     = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef        = NULL;
        OMX_U32                         nSupportFormat  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            switch (nIndex) {
            case supportFormat_0:
                pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                pPortFormat->eColorFormat       = OMX_COLOR_FormatYUV420Planar;
                pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                break;
            case supportFormat_1:
                pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                pPortFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                break;
            case supportFormat_2:
                pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                pPortFormat->eColorFormat       = OMX_SEC_COLOR_FormatNV12Tiled;
                pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                break;
            case supportFormat_3:
                pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                pPortFormat->eColorFormat       = OMX_SEC_COLOR_FormatNV21Linear;
                pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                break;
            case supportFormat_4:
                pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                pPortFormat->eColorFormat       = OMX_COLOR_FormatAndroidOpaque;
                pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                break;
            default:
                if (nIndex > supportFormat_0) {
                    ret = OMX_ErrorNoMore;
                    goto EXIT;
                }
                break;
            }
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            nSupportFormat = OUTPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (nIndex > nSupportFormat) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pPortDef = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
            pPortFormat->eColorFormat       = pPortDef->format.video.eColorFormat;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE     *pVideoBitrate  = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex     = pVideoBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT             *pExynosPort    = NULL;
        EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc      = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef       = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoBitrate->eControlRate = pVideoEnc->eControlRate[nPortIndex];
            pVideoBitrate->nTargetBitrate = pPortDef->format.video.nBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE  *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                            nPortIndex         = pVideoQuantization->nPortIndex;
        EXYNOS_OMX_BASEPORT               *pExynosPort        = NULL;
        EXYNOS_OMX_VIDEOENC_COMPONENT     *pVideoEnc          = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE      *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoQuantization->nQpI = pVideoEnc->quantization.nQpI;
            pVideoQuantization->nQpP = pVideoEnc->quantization.nQpP;
            pVideoQuantization->nQpB = pVideoEnc->quantization.nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef      = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex    = pPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort   = NULL;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
        Exynos_OSAL_Memcpy(pPortDef, &pExynosPort->portDefinition, pPortDef->nSize);

#ifdef USE_STOREMETADATA
        if ((nPortIndex == 0) &&
            (pExynosPort->bStoreMetaData == OMX_TRUE)) {
            pPortDef->nBufferSize = MAX_INPUT_METADATA_BUFFER_SIZE;
        }
#endif
    }
        break;
    default:
    {
        ret = Exynos_OMX_GetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeSetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort       = NULL;

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

    if (pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat     = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex      = pPortFormat->nPortIndex;
        OMX_U32                         nIndex          = pPortFormat->nIndex;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef        = NULL;
        OMX_U32                         nSupportFormat  = 0;

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);

        pPortDef->format.video.eColorFormat       = pPortFormat->eColorFormat;
        pPortDef->format.video.eCompressionFormat = pPortFormat->eCompressionFormat;
        pPortDef->format.video.xFramerate         = pPortFormat->xFramerate;
    }
        break;
    case OMX_IndexParamVideoBitrate:
    {
        OMX_VIDEO_PARAM_BITRATETYPE     *pVideoBitrate = (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32                          nPortIndex    = pVideoBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT             *pExynosPort   = NULL;
        EXYNOS_OMX_VIDEOENC_COMPONENT   *pVideoEnc     = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE    *pPortDef      = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pPortDef = &(pExynosComponent->pExynosPort[nPortIndex].portDefinition);

            pVideoEnc->eControlRate[nPortIndex] = pVideoBitrate->eControlRate;
            pPortDef->format.video.nBitrate = pVideoBitrate->nTargetBitrate;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamVideoQuantization:
    {
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE *pVideoQuantization = (OMX_VIDEO_PARAM_QUANTIZATIONTYPE *)pComponentParameterStructure;
        OMX_U32                           nPortIndex         = pVideoQuantization->nPortIndex;
        EXYNOS_OMX_BASEPORT              *pExynosPort        = NULL;
        EXYNOS_OMX_VIDEOENC_COMPONENT    *pVideoEnc          = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE     *pPortDef           = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;
            if (pVideoEnc == NULL) {
                ret = OMX_ErrorBadParameter;
                goto EXIT;
            }
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pPortDef = &pExynosPort->portDefinition;

            pVideoEnc->quantization.nQpI = pVideoQuantization->nQpI;
            pVideoEnc->quantization.nQpP = pVideoQuantization->nQpP;
            pVideoEnc->quantization.nQpB = pVideoQuantization->nQpB;
        }
        ret = OMX_ErrorNone;
    }
        break;
    case OMX_IndexParamPortDefinition:
    {
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef      = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParameterStructure;
        OMX_U32                       nPortIndex    = pPortDef->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort   = NULL;
        OMX_U32 width, height, size;

        if (nPortIndex >= pExynosComponent->portParam.nPorts) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        ret = Exynos_OMX_Check_SizeVersion(pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];

        if ((pExynosComponent->currentState != OMX_StateLoaded) &&
            (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            if (pExynosPort->portDefinition.bEnabled == OMX_TRUE) {
                ret = OMX_ErrorIncorrectStateOperation;
                goto EXIT;
            }
        }

        if (pPortDef->nBufferCountActual < pExynosPort->portDefinition.nBufferCountMin) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }

        Exynos_OSAL_Memcpy(&pExynosPort->portDefinition, pPortDef, pPortDef->nSize);
        if (nPortIndex == INPUT_PORT_INDEX) {
            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            Exynos_UpdateFrameSize(pOMXComponent);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pExynosOutputPort->portDefinition.nBufferSize: %d",
                            pExynosPort->portDefinition.nBufferSize);
        }
        ret = OMX_ErrorNone;
    }
        break;
#ifdef USE_STOREMETADATA
    case OMX_IndexParamStoreMetaDataBuffer:
    {
        ret = Exynos_OSAL_SetANBParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
#endif
    default:
    {
        ret = Exynos_OMX_SetParameter(hComponent, nParamIndex, pComponentParameterStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nParamIndex,
    OMX_PTR         pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
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

    switch (nParamIndex) {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pConfigBitrate->nEncodeBitrate = pExynosPort->portDefinition.format.video.nBitrate;
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;
        EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pConfigFramerate->xEncodeFramerate = pExynosPort->portDefinition.format.video.xFramerate;
        }
    }
        break;
    default:
    {
        ret = Exynos_OMX_GetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeSetConfig(
    OMX_HANDLETYPE  hComponent,
    OMX_INDEXTYPE   nParamIndex,
    OMX_PTR         pComponentConfigStructure)
    {
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if ((hComponent == NULL) ||
        (pComponentConfigStructure == NULL)) {
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

    switch (nParamIndex) {
    case OMX_IndexConfigVideoBitrate:
    {
        OMX_VIDEO_CONFIG_BITRATETYPE *pConfigBitrate = (OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentConfigStructure;
        OMX_U32                       nPortIndex     = pConfigBitrate->nPortIndex;
        EXYNOS_OMX_BASEPORT          *pExynosPort    = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pExynosPort->portDefinition.format.video.nBitrate = pConfigBitrate->nEncodeBitrate;
        }
    }
        break;
    case OMX_IndexConfigVideoFramerate:
    {
        OMX_CONFIG_FRAMERATETYPE *pConfigFramerate = (OMX_CONFIG_FRAMERATETYPE *)pComponentConfigStructure;
        OMX_U32                   nPortIndex       = pConfigFramerate->nPortIndex;
        EXYNOS_OMX_BASEPORT      *pExynosPort      = NULL;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pExynosPort = &pExynosComponent->pExynosPort[nPortIndex];
            pExynosPort->portDefinition.format.video.xFramerate = pConfigFramerate->xEncodeFramerate;
        }
    }
        break;
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        OMX_CONFIG_INTRAREFRESHVOPTYPE *pIntraRefreshVOP = (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentConfigStructure;
        EXYNOS_OMX_VIDEOENC_COMPONENT *pVideoEnc         = NULL;
        OMX_U32                        nPortIndex        = pIntraRefreshVOP->nPortIndex;

        if (pExynosComponent->hComponentHandle == NULL) {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pVideoEnc = (EXYNOS_OMX_VIDEOENC_COMPONENT *)pExynosComponent->hComponentHandle;

        if (nPortIndex != OUTPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        } else {
            pVideoEnc->IntraRefreshVOP = pIntraRefreshVOP->IntraRefreshVOP;
        }
    }
        break;
    default:
    {
        ret = Exynos_OMX_SetConfig(hComponent, nParamIndex, pComponentConfigStructure);
    }
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_VideoEncodeGetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      szParamName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

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

    if ((szParamName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

#ifdef USE_STOREMETADATA
    if (Exynos_OSAL_Strcmp(szParamName, EXYNOS_INDEX_PARAM_STORE_METADATA_BUFFER) == 0) {
        *pIndexType = (OMX_INDEXTYPE)OMX_IndexParamStoreMetaDataBuffer;
    } else {
        ret = Exynos_OMX_GetExtensionIndex(hComponent, szParamName, pIndexType);
    }
#else
    ret = Exynos_OMX_GetExtensionIndex(hComponent, szParamName, pIndexType);
#endif
EXIT:
    FunctionOut();

    return ret;
}
