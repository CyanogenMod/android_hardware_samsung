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
 * @file      Exynos_OMX_Wmadec.c
 * @brief
 * @author    Sungyeon Kim (sy85.kim@samsung.com)
 * @version   1.0.0
 * @history
 *   2012.09.07 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Adec.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_Wmadec.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_WMA_DEC"
#define EXYNOS_LOG_OFF
#include "Exynos_OSAL_Log.h"


OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_GetParameter(
    OMX_IN    OMX_HANDLETYPE hComponent,
    OMX_IN    OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;
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
    case OMX_IndexParamAudioWma:
    {
        OMX_AUDIO_PARAM_WMATYPE *pDstWmaParam = (OMX_AUDIO_PARAM_WMATYPE *)pComponentParameterStructure;
        OMX_AUDIO_PARAM_WMATYPE *pSrcWmaParam = NULL;
        EXYNOS_WMA_HANDLE       *pWmaDec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstWmaParam, sizeof(OMX_AUDIO_PARAM_WMATYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pDstWmaParam->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmaDec = (EXYNOS_WMA_HANDLE *)((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcWmaParam = &pWmaDec->wmaParam;

        Exynos_OSAL_Memcpy(pDstWmaParam, pSrcWmaParam, sizeof(OMX_AUDIO_PARAM_WMATYPE));
    }
        break;
    case OMX_IndexParamAudioPcm:
    {
        OMX_AUDIO_PARAM_PCMMODETYPE *pDstPcmParam = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        OMX_AUDIO_PARAM_PCMMODETYPE *pSrcPcmParam = NULL;
        EXYNOS_WMA_HANDLE           *pWmaDec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstPcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pDstPcmParam->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmaDec = (EXYNOS_WMA_HANDLE *)((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcPcmParam = &pWmaDec->pcmParam;

        Exynos_OSAL_Memcpy(pDstPcmParam, pSrcPcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_S32 codecType;
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_WMA_DEC_ROLE);
    }
        break;
    default:
        ret = Exynos_OMX_AudioDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;
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
    case OMX_IndexParamAudioWma:
    {
        OMX_AUDIO_PARAM_WMATYPE *pDstWmaParam = NULL;
        OMX_AUDIO_PARAM_WMATYPE *pSrcWmaParam = (OMX_AUDIO_PARAM_WMATYPE *)pComponentParameterStructure;
        EXYNOS_WMA_HANDLE       *pWmaDec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcWmaParam, sizeof(OMX_AUDIO_PARAM_WMATYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcWmaParam->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmaDec = (EXYNOS_WMA_HANDLE *)((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstWmaParam = &pWmaDec->wmaParam;

        Exynos_OSAL_Memcpy(pDstWmaParam, pSrcWmaParam, sizeof(OMX_AUDIO_PARAM_WMATYPE));
    }
        break;
    case OMX_IndexParamAudioPcm:
    {
        OMX_AUDIO_PARAM_PCMMODETYPE *pDstPcmParam = NULL;
        OMX_AUDIO_PARAM_PCMMODETYPE *pSrcPcmParam = (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        EXYNOS_WMA_HANDLE           *pWmaDec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcPcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcPcmParam->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pWmaDec = (EXYNOS_WMA_HANDLE *)((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstPcmParam = &pWmaDec->pcmParam;

        Exynos_OSAL_Memcpy(pDstPcmParam, pSrcPcmParam, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_WMA_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
        } else {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    default:
        ret = Exynos_OMX_AudioDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_GetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;
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
    /* TODO : Support for various OMX_INDEXTYPE */
    default:
        ret = Exynos_OMX_AudioDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_SetConfig(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;
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
    /* TODO : Support for various OMX_INDEXTYPE */
    default:
        ret = Exynos_OMX_AudioDecodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_GetExtensionIndex(
    OMX_IN OMX_HANDLETYPE  hComponent,
    OMX_IN OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;

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

    ret = Exynos_OMX_AudioDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_ComponentRoleEnum(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_U8        *cRole,
    OMX_IN  OMX_U32        nIndex)
{
    OMX_ERRORTYPE               ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE          *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT   *pExynosComponent = NULL;
    OMX_S32                     codecType;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (nIndex != (MAX_COMPONENT_ROLE_NUM - 1)) {
        ret = OMX_ErrorNoMore;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone)
        goto EXIT;
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid ) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_WMA_DEC_ROLE);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_AUDIODEC_COMPONENT *pAudioDec = (EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMA_HANDLE             *pWmaDec = (EXYNOS_WMA_HANDLE *)pAudioDec->hCodecHandle;

    FunctionIn();

    Exynos_OSAL_Memset(pExynosComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pExynosComponent->bUseFlagEOF = OMX_TRUE; /* Wma extractor should parse into frame unit. */
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    FFmpeg_Init(&pWmaDec->ffmpeg);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_AUDIODEC_COMPONENT *pAudioDec = (EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMA_HANDLE             *pWmaDec = (EXYNOS_WMA_HANDLE *)pAudioDec->hCodecHandle;

    FunctionIn();

    FFmpeg_DeInit(&pWmaDec->ffmpeg);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_codecConfigure(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pInputData)
{
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_AUDIODEC_COMPONENT *pAudioDec = (EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMA_HANDLE             *pWmaDec = (EXYNOS_WMA_HANDLE *)pAudioDec->hCodecHandle;

    const int codec_info_size = 18;

    FFmpeg *ffmpeg = &pWmaDec->ffmpeg;
    CodecInfoHhr codecInfo;
    char extra_data[10];

    Exynos_OSAL_Memcpy(&codecInfo, pInputData->buffer.singlePlaneBuffer.dataBuffer, codec_info_size);
    Exynos_OSAL_Memcpy(extra_data, ((char*)pInputData->buffer.singlePlaneBuffer.dataBuffer) + codec_info_size,
                                    codecInfo.codecSpecificDataSize);

    return FFmpeg_CodecOpen(ffmpeg, codecInfo.codecID, codecInfo.averageNumberOfbytesPerSecond * 8,
                            extra_data, codecInfo.codecSpecificDataSize, codecInfo.sampleRates,
                            codecInfo.numberOfChannels, codecInfo.blockAlignment);
}

OMX_ERRORTYPE Exynos_FFMPEG_WmaDec_bufferProcess(OMX_COMPONENTTYPE *pOMXComponent, EXYNOS_OMX_DATA *pInputData, EXYNOS_OMX_DATA *pOutputData)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_AUDIODEC_COMPONENT *pAudioDec = (EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_WMA_HANDLE             *pWmaDec = (EXYNOS_WMA_HANDLE *)pAudioDec->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT      *pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pInputPort)) || (!CHECK_PORT_ENABLED(pOutputPort)) ||
            (!CHECK_PORT_POPULATED(pInputPort)) || (!CHECK_PORT_POPULATED(pOutputPort))) {
        if (pInputData->nFlags & OMX_BUFFERFLAG_EOS)
            ret = OMX_ErrorInputDataDecodeYet;
        else
            ret = OMX_ErrorNone;

        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent)) {
        if (pInputData->nFlags & OMX_BUFFERFLAG_EOS)
            ret = OMX_ErrorInputDataDecodeYet;
        else
            ret = OMX_ErrorNone;

        goto EXIT;
    }
    if (pInputData->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        ret = Exynos_FFMPEG_WmaDec_codecConfigure(pOMXComponent, pInputData);
    } else {
        /* Save timestamp and flags of input data */
        pOutputData->timeStamp = pInputData->timeStamp;
        pOutputData->nFlags = pInputData->nFlags & (~OMX_BUFFERFLAG_EOS);

        /* WARNING IT DOESNT GUARANTEE WORKING WELL TO MULTIPLE FRAMES */
        pInputData->dataLen = pInputData->allocSize;
        pOutputData->dataLen = pOutputData->allocSize;
        ret = FFmpeg_Decode(&pWmaDec->ffmpeg,
                            pInputData->buffer.singlePlaneBuffer.dataBuffer, (int*)&pInputData->dataLen,
                            pOutputData->buffer.singlePlaneBuffer.dataBuffer, (int*)&pOutputData->dataLen);
    }

    if (ret != OMX_ErrorNone) {
        if (ret == OMX_ErrorInputDataDecodeYet) {
            pOutputData->usedDataLen = 0;
            pOutputData->remainDataLen = pOutputData->dataLen;
        } else {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                    pExynosComponent->callbackData,
                                                    OMX_EventError, ret, 0, NULL);
        }
    } else {
        pInputData->usedDataLen += pInputData->dataLen;
        pInputData->remainDataLen = pInputData->dataLen - pInputData->usedDataLen;
        pInputData->dataLen -= pInputData->usedDataLen;
        pInputData->usedDataLen = 0;

        pOutputData->usedDataLen = 0;
        pOutputData->remainDataLen = pOutputData->dataLen;

        if (pInputData->nFlags & OMX_BUFFERFLAG_EOS)
            pOutputData->nFlags |= OMX_BUFFERFLAG_EOS;
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(OMX_HANDLETYPE hComponent, OMX_STRING componentName)
{
    OMX_ERRORTYPE                  ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE             *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = NULL;
    EXYNOS_OMX_BASEPORT           *pExynosPort = NULL;
    EXYNOS_OMX_AUDIODEC_COMPONENT *pAudioDec = NULL;
    EXYNOS_WMA_HANDLE             *pWmaDec = NULL;
    OMX_PTR                        pInputBuffer = NULL;
    OMX_PTR                        pOutputBuffer = NULL;
    unsigned int                   inputBufferSize = AUDIO_INBUF_SIZE;
    unsigned int                   inputBufferNum = 1;
    unsigned int                   outputBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    unsigned int                   outputBufferNum = 1;
    OMX_S32                        returnCodec;
    int i = 0;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: parameters are null, ret: %X", __FUNCTION__, ret);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_WMA_DEC, componentName) != 0) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: componentName(%s) error, ret: %X", __FUNCTION__, componentName, ret);
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_AudioDecodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: Exynos_OMX_AudioDecodeComponentInit error, ret: %X", __FUNCTION__, ret);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = HW_AUDIO_DEC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: componentName alloc error, ret: %X", __FUNCTION__, ret);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT_ERROR_1;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);
    Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_WMA_DEC);

    pWmaDec = Exynos_OSAL_Malloc(sizeof(EXYNOS_WMA_HANDLE));
    if (pWmaDec == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s: EXYNOS_WMA_HANDLE alloc error, ret: %X", __FUNCTION__, ret);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT_ERROR_2;
    }
    Exynos_OSAL_Memset(pWmaDec, 0, sizeof(EXYNOS_WMA_HANDLE));
    pAudioDec = (EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pAudioDec->hCodecHandle = (OMX_HANDLETYPE)pWmaDec;

    /* Get input buffer info */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->processData.buffer.singlePlaneBuffer.dataBuffer = Exynos_OSAL_Malloc(inputBufferSize);
    if (pExynosPort->processData.buffer.singlePlaneBuffer.dataBuffer == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Input data buffer alloc failed");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT_ERROR_3;
    }
    pExynosPort->processData.allocSize = inputBufferSize;

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
    pExynosPort->portDefinition.nBufferCountActual = inputBufferNum;
    pExynosPort->portDefinition.nBufferCountMin = inputBufferNum;
    pExynosPort->portDefinition.nBufferSize = inputBufferSize;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.audio.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.audio.cMIMEType, "audio/x-ms-wma");
    pExynosPort->portDefinition.format.audio.pNativeRender = 0;
    pExynosPort->portDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
    pExynosPort->portWayType = WAY1_PORT;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.nBufferCountActual = outputBufferNum;
    pExynosPort->portDefinition.nBufferCountMin = outputBufferNum;
    pExynosPort->portDefinition.nBufferSize = outputBufferSize;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.audio.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.audio.cMIMEType, "audio/raw");
    pExynosPort->portDefinition.format.audio.pNativeRender = 0;
    pExynosPort->portDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    pExynosPort->portWayType = WAY1_PORT;

    /* Default values for Wma audio param */
    INIT_SET_SIZE_VERSION(&pWmaDec->wmaParam, OMX_AUDIO_PARAM_WMATYPE);
    pWmaDec->wmaParam.nPortIndex      = INPUT_PORT_INDEX;
    pWmaDec->wmaParam.nChannels       = DEFAULT_AUDIO_CHANNELS_NUM;
    pWmaDec->wmaParam.nBitRate        = 0;
    pWmaDec->wmaParam.nSamplingRate     = DEFAULT_AUDIO_SAMPLING_FREQ;
    pWmaDec->wmaParam.eFormat         = OMX_AUDIO_WMAFormat9;

    /* Default values for PCM audio param */
    INIT_SET_SIZE_VERSION(&pWmaDec->pcmParam, OMX_AUDIO_PARAM_PCMMODETYPE);
    pWmaDec->pcmParam.nPortIndex         = OUTPUT_PORT_INDEX;
    pWmaDec->pcmParam.nChannels          = DEFAULT_AUDIO_CHANNELS_NUM;
    pWmaDec->pcmParam.eNumData           = OMX_NumericalDataSigned;
    pWmaDec->pcmParam.eEndian            = OMX_EndianLittle;
    pWmaDec->pcmParam.bInterleaved       = OMX_TRUE;
    pWmaDec->pcmParam.nBitPerSample      = DEFAULT_AUDIO_BIT_PER_SAMPLE;
    pWmaDec->pcmParam.nSamplingRate      = DEFAULT_AUDIO_SAMPLING_FREQ;
    pWmaDec->pcmParam.ePCMMode           = OMX_AUDIO_PCMModeLinear;
    pWmaDec->pcmParam.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pWmaDec->pcmParam.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pOMXComponent->GetParameter      = &Exynos_FFMPEG_WmaDec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_FFMPEG_WmaDec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_FFMPEG_WmaDec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_FFMPEG_WmaDec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_FFMPEG_WmaDec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_FFMPEG_WmaDec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    /* ToDo: Change the function name associated with a specific codec */
    pExynosComponent->exynos_codec_componentInit      = &Exynos_FFMPEG_WmaDec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_FFMPEG_WmaDec_Terminate;
    pAudioDec->exynos_codec_bufferProcess = &Exynos_FFMPEG_WmaDec_bufferProcess;
    pAudioDec->exynos_checkInputFrame = NULL;

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;
    goto EXIT; /* This function is performed successfully. */

EXIT_ERROR_3:
    Exynos_OSAL_Free(pWmaDec);
    pAudioDec->hCodecHandle = NULL;
EXIT_ERROR_2:
    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;
EXIT_ERROR_1:
    Exynos_OMX_AudioDecodeComponentDeinit(pOMXComponent);
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE             ret = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;
    EXYNOS_WMA_HANDLE        *pWmaDec = NULL;
    EXYNOS_OMX_BASEPORT      *pExynosPort = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    if (pExynosPort->processData.buffer.singlePlaneBuffer.dataBuffer) {
        Exynos_OSAL_Free(pExynosPort->processData.buffer.singlePlaneBuffer.dataBuffer);
        pExynosPort->processData.buffer.singlePlaneBuffer.dataBuffer = NULL;
        pExynosPort->processData.allocSize = 0;
    }

    pWmaDec = (EXYNOS_WMA_HANDLE *)((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    if (pWmaDec != NULL) {
        Exynos_OSAL_Free(pWmaDec);
        ((EXYNOS_OMX_AUDIODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle = NULL;
    }

    ret = Exynos_OMX_AudioDecodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone)
        goto EXIT;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
