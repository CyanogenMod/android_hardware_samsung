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

#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include "ISecTVOut.h"

namespace android {

    enum {
        SET_HDMI_STATUS = IBinder::FIRST_CALL_TRANSACTION,
        SET_HDMI_MODE,
        SET_HDMI_RESOLUTION,
        SET_HDMI_HDCP,
        SET_HDMI_ROTATE,
        SET_HDMI_HWCLAYER,
        BLIT_2_HDMI
    };

    void BpSecTVOut::setHdmiCableStatus(uint32_t status)
    {
        Parcel data, reply;
        data.writeInt32(status);
        remote()->transact(SET_HDMI_STATUS, data, &reply);
    }

    void BpSecTVOut::setHdmiMode(uint32_t mode)
    {
        Parcel data, reply;
        data.writeInt32(mode);
        remote()->transact(SET_HDMI_MODE, data, &reply);
    }

    void BpSecTVOut::setHdmiResolution(uint32_t resolution)
    {
        Parcel data, reply;
        data.writeInt32(resolution);
        remote()->transact(SET_HDMI_RESOLUTION, data, &reply);
    }

    void BpSecTVOut::setHdmiHdcp(uint32_t resolution)
    {
        Parcel data, reply;
        data.writeInt32(resolution);
        remote()->transact(SET_HDMI_HDCP, data, &reply);
    }

    void BpSecTVOut::setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer)
    {
        Parcel data, reply;
        data.writeInt32(rotVal);
        data.writeInt32(hwcLayer);
        remote()->transact(SET_HDMI_ROTATE, data, &reply);
    }

    void BpSecTVOut::setHdmiHwcLayer(uint32_t hwcLayer)
    {
        Parcel data, reply;
        data.writeInt32(hwcLayer);
        remote()->transact(SET_HDMI_HWCLAYER, data, &reply);
    }

    void BpSecTVOut::blit2Hdmi(uint32_t w, uint32_t h,
                                        uint32_t colorFormat,
                                        uint32_t physYAddr,
                                        uint32_t physCbAddr,
                                        uint32_t physCrAddr,
                                        uint32_t dstX,
                                        uint32_t dstY,
                                        uint32_t hdmiLayer,
                                        uint32_t num_of_hwc_layer)
    {
        Parcel data, reply;
        data.writeInt32(w);
        data.writeInt32(h);
        data.writeInt32(colorFormat);
        data.writeInt32(physYAddr);
        data.writeInt32(physCbAddr);
        data.writeInt32(physCrAddr);
        data.writeInt32(dstX);
        data.writeInt32(dstY);
        data.writeInt32(hdmiLayer);
        data.writeInt32(num_of_hwc_layer);
        remote()->transact(BLIT_2_HDMI, data, &reply);
    }

    IMPLEMENT_META_INTERFACE(SecTVOut, "android.os.ISecTVOut");
};
