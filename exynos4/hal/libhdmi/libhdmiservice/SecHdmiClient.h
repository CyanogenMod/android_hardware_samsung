/*
**
** Copyright 2008, The Android Open Source Project
** Copyright 2010, Samsung Electronics Co. LTD
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
*/

/*
**
** @author  Taikyung, Yu(taikyung.yu@samsung.com)
** @date    2011-07-06
*/

#ifndef __SEC_HDMI_CLIENT_H__
#define __SEC_HDMI_CLIENT_H__

#include "utils/Log.h"

#include <linux/errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <utils/RefBase.h>
#include <cutils/log.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include "ISecTVOut.h"

#define GETSERVICETIMEOUT (5)

namespace android {

class SecHdmiClient
{
public:
    enum HDMI_MODE
    {
        HDMI_MODE_NONE = 0,
        HDMI_MODE_UI,
        HDMI_MODE_VIDEO,
    };

private:
    SecHdmiClient();
    virtual ~SecHdmiClient();
    uint32_t    mEnable;

public:
        static SecHdmiClient * getInstance(void);
        void setHdmiCableStatus(int status);
        void setHdmiMode(int mode);
        void setHdmiResolution(int resolution);
        void setHdmiHdcp(int enHdcp);
        void setHdmiRotate(int rotVal, uint32_t hwcLayer);
        void setHdmiHwcLayer(uint32_t hwcLayer);
        void setHdmiEnable(uint32_t enable);
        virtual void blit2Hdmi(uint32_t w, uint32_t h,
                                        uint32_t colorFormat,
                                        uint32_t physYAddr,
                                        uint32_t physCbAddr,
                                        uint32_t physCrAddr,
                                        uint32_t dstX,
                                        uint32_t dstY,
                                        uint32_t hdmiLayer,
                                        uint32_t num_of_hwc_layer);

private:
        sp<ISecTVOut> m_getSecTVOutService(void);

};

};

#endif
