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
 * @file      library_register.h
 * @brief
 * @author    Sungyeon Kim (sy85.kim@samsung.com)
 * @version   1.0.0
 * @history
 *   2012.09.07 : Create
 */

#ifndef EXYNOS_OMX_WMA_DEC_REG
#define EXYNOS_OMX_WMA_DEC_REG

#include "Exynos_OMX_Def.h"
#include "OMX_Component.h"
#include "Exynos_OMX_Component_Register.h"


#define OSCL_EXPORT_REF __attribute__((visibility("default")))
#define MAX_COMPONENT_NUM         1
#define MAX_COMPONENT_ROLE_NUM    1

/* WMA */
#define EXYNOS_OMX_COMPONENT_WMA_DEC         "OMX.Exynos.WMA.Decoder"
#define EXYNOS_OMX_COMPONENT_WMA_DEC_ROLE    "audio_decoder.wma"

#ifdef __cplusplus
extern "C" {
#endif

OSCL_EXPORT_REF int Exynos_OMX_COMPONENT_Library_Register(ExynosRegisterComponentType **ppExynosComponent);

#ifdef __cplusplus
};
#endif

#endif /* EXYNOS_OMX_WMA_DEC_REG */
