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

#ifndef ISECTVOUT_H
#define ISECTVOUT_H
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {
    class ISecTVOut: public IInterface
    {
        public:
            DECLARE_META_INTERFACE(SecTVOut);
            virtual void setHdmiCableStatus(uint32_t status) = 0;
            virtual void setHdmiMode(uint32_t mode) = 0;
            virtual void setHdmiResolution(uint32_t resolution) = 0;
            virtual void setHdmiHdcp(uint32_t enHdcp) = 0;
            virtual void setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer) = 0;
            virtual void setHdmiHwcLayer(uint32_t hwcLayer) = 0;
            virtual void blit2Hdmi(uint32_t w, uint32_t h,
                                        uint32_t colorFormat,
                                        uint32_t physYAddr,
                                        uint32_t physCbAddr,
                                        uint32_t physCrAddr,
                                        uint32_t dstX,
                                        uint32_t dstY,
                                        uint32_t hdmiLayer,
                                        uint32_t num_of_hwc_layer) = 0;
    };
    //--------------------------------------------------------------
    class BpSecTVOut: public BpInterface<ISecTVOut>
    {
        public:
            BpSecTVOut(const sp<IBinder>& impl): BpInterface<ISecTVOut>(impl){}
            virtual void setHdmiCableStatus(uint32_t status);
            virtual void setHdmiMode(uint32_t mode);
            virtual void setHdmiResolution(uint32_t resolution);
            virtual void setHdmiHdcp(uint32_t enHdcp);
            virtual void setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer);
            virtual void setHdmiHwcLayer(uint32_t hwcLayer);
            virtual void blit2Hdmi(uint32_t w, uint32_t h,
                                        uint32_t colorFormat,
                                        uint32_t physYAddr,
                                        uint32_t physCbAddr,
                                        uint32_t physCrAddr,
                                        uint32_t dstX,
                                        uint32_t dstY,
                                        uint32_t hdmiLayer,
                                        uint32_t num_of_hwc_layer);
    };
};
#endif
