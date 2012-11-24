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
 * @file      Exynos_OMX_Wmadec.h
 * @brief
 * @author    Sungyeon Kim (sy85.kim@samsung.com)
 * @version   1.0.0
 * @history
 *   2012.09.07 : Create
 */

#ifndef EXYNOS_OMX_WMA_DEC_COMPONENT
#define EXYNOS_OMX_WMA_DEC_COMPONENT

#include "Exynos_OMX_Def.h"
#include "OMX_Component.h"
#include "ffmpeg_api.h"

/*
 * This structure is the same as BitmapInfoHhr struct in pv_avifile_typedefs.h file
 */
typedef struct _CodecInfoHhr
{
    OMX_U16 codecID;
    OMX_U16 numberOfChannels;
    OMX_U32 sampleRates;
    OMX_U32 averageNumberOfbytesPerSecond;
    OMX_U16 blockAlignment;
    OMX_U16 bitsPerSample;
    OMX_U16 codecSpecificDataSize;
} CodecInfoHhr;

typedef struct _EXYNOS_WMA_HANDLE
{
    /* OMX Codec specific */
    OMX_AUDIO_PARAM_WMATYPE     wmaParam;
    OMX_AUDIO_PARAM_PCMMODETYPE pcmParam;

    FFmpeg ffmpeg;
} EXYNOS_WMA_HANDLE;

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(OMX_HANDLETYPE hComponent, OMX_STRING componentName);
                OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(OMX_HANDLETYPE hComponent);

#ifdef __cplusplus
};
#endif

#endif /* EXYNOS_OMX_WMA_DEC_COMPONENT */
