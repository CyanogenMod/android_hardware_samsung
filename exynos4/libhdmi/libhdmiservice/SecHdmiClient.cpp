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

#define LOG_TAG "libhdmiclient"

#include "SecHdmiClient.h"

namespace android {

static sp<ISecTVOut> g_SecTVOutService = 0;

SecHdmiClient::SecHdmiClient()
{
    g_SecTVOutService = m_getSecTVOutService();
    mEnable = 0;
}

SecHdmiClient::~SecHdmiClient()
{
}

SecHdmiClient * SecHdmiClient::getInstance(void)
{
    static SecHdmiClient singleton;
    return &singleton;
}

void SecHdmiClient::setHdmiCableStatus(int status)
{
    //LOGD("%s HDMI status: %d\n", __func__, status);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiCableStatus(status);
}

uint32_t SecHdmiClient::getHdmiCableStatus()
{
    uint32_t cableStatus = 0;
    if (g_SecTVOutService == 0)
        g_SecTVOutService = m_getSecTVOutService();

    if (g_SecTVOutService != 0)
        cableStatus = g_SecTVOutService->getHdmiCableStatus();
    return cableStatus;
}

void SecHdmiClient::setHdmiMode(int mode)
{
    //LOGD("%s HDMI Mode: %d\n", __func__, mode);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiMode(mode);
}

void SecHdmiClient::setHdmiResolution(int resolution, HDMI_S3D_MODE s3dMode)
{
    //LOGD("%s HDMI Resolution: %d\n", __func__, resolution);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiResolution(resolution, s3dMode);
}

void SecHdmiClient::setHdmiHdcp(int enHdcp)
{
    //LOGD("%s HDMI HDCP: %d\n", __func__, enHdcp);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiHdcp(enHdcp);
}

void SecHdmiClient::setExtDispLayerNum(int extDispLayerNum)
{
    if (g_SecTVOutService == 0)
        g_SecTVOutService = m_getSecTVOutService();

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setExtDispLayerNum(extDispLayerNum);
}

void SecHdmiClient::setHdmiRotate(int rotVal, uint32_t hwcLayer)
{
    //LOGD("%s HDMI ROTATE: %d\n", __func__, rotVal);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiRotate(rotVal, hwcLayer);
}

void SecHdmiClient::setHdmiHwcLayer(uint32_t hwcLayer)
{
    //LOGD("%s HDMI HWCLAYER: %d\n", __func__, hwcLayer);

    if (g_SecTVOutService != 0)
        g_SecTVOutService->setHdmiHwcLayer(hwcLayer);
}

void SecHdmiClient::setHdmiEnable(uint32_t enable)
{
    //LOGD("%s HDMI ENABLE: %d\n", __func__, enable);

    if (g_SecTVOutService != 0)
        mEnable = enable;
}

void SecHdmiClient::blit2Hdmi(uint32_t w, uint32_t h,
                                uint32_t colorFormat,
                                uint32_t physYAddr,
                                uint32_t physCbAddr,
                                uint32_t physCrAddr,
                                uint32_t dstX,
                                uint32_t dstY,
                                uint32_t hdmiLayer,
                                uint32_t num_of_hwc_layer)
{
    if (g_SecTVOutService != 0 && mEnable == 1)
        g_SecTVOutService->blit2Hdmi(w, h, colorFormat, physYAddr, physCbAddr, physCrAddr, dstX, dstY, hdmiLayer, num_of_hwc_layer);
}

sp<ISecTVOut> SecHdmiClient::m_getSecTVOutService(void)
{
    int ret = 0;

    if (g_SecTVOutService == 0) {
        sp<IBinder> binder;
        sp<ISecTVOut> sc;
        sp<IServiceManager> sm = defaultServiceManager();
        int getSvcTimes = 0;
        for(getSvcTimes = 0; getSvcTimes < GETSERVICETIMEOUT; getSvcTimes++) {
            binder = sm->getService(String16("SecTVOutService"));
            if (binder == 0) {
                LOGW("SecTVOutService not published, waiting...");
                usleep(500000); // 0.5 s
            } else {
                break;
            }
        }
        // grab the lock again for updating g_surfaceFlinger
        if (getSvcTimes < GETSERVICETIMEOUT) {
            sc = interface_cast<ISecTVOut>(binder);
            g_SecTVOutService = sc;
        } else {
            LOGW("Failed to get SecTVOutService... SecHdmiClient will get it later..");
        }
    }
    return g_SecTVOutService;
}

}
