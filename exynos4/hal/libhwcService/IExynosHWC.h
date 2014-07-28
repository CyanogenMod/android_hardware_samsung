/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDROID_EXYNOS_IHWC_H_
#define ANDROID_EXYNOS_IHWC_H_

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>

struct wfd_layer_t {
    bool   isPresentation;
    bool   isDrm;
    struct timeval tv_stamp;
};

namespace android {

enum {
    VIDEO_PLAY_NORMAL = 0,
    VIDEO_PLAY_PAUSE,
    VIDEO_PLAY_SEEK,
};

class IExynosHWCService : public IInterface {
public:

    DECLARE_META_INTERFACE(ExynosHWCService);

    /*
     * setWFDMode() function sets the WFD operation Mode.
     * It enables / disables the WFD.
     */
    virtual int setWFDMode(unsigned int mode) = 0;
    virtual int setWFDOutputResolution(unsigned int width, unsigned int height,
                                  unsigned int disp_w, unsigned int disp_h) = 0;

    /*
     * setExtraFBMode() function Enables / disables the extra FB usage.
     */
    virtual int setExtraFBMode(unsigned int mode) = 0;
    virtual int setCameraMode(unsigned int mode) = 0;
    virtual int setForceMirrorMode(unsigned int mode) = 0;

    /*
     * setVideoPlayStatus() function sets the Video playback status.
     * It is used to inform the HWC about the video playback seek and
     * pause status.
     */
    virtual int setVideoPlayStatus(unsigned int mode) = 0;
    virtual int setExternalDisplayPause(bool onoff) = 0;
    virtual int setDispOrientation(unsigned int transform) = 0;
    virtual int setProtectionMode(unsigned int mode) = 0;
    virtual int setExternalDispLayerNum(unsigned int num) = 0;
    virtual int setForceGPU(unsigned int on) = 0;
    virtual int setVideoRotation(unsigned int rotation_degree) = 0;
    virtual int getVideoRotation(void) = 0;

    virtual void setHdmiResolution(int resolution, int s3dMode) = 0;
    virtual void setHdmiCableStatus(int status) = 0;
    virtual void setHdmiHdcp(int status) = 0;
    virtual void setHdmiAudioChannel(uint32_t channels) = 0;
    virtual void setHdmiSubtitles(bool use) = 0;
    virtual void setPresentationMode(bool use) = 0;
    virtual void setWFDSleepCtrl(bool black) = 0;

    virtual int  getWFDMode() = 0;
    virtual void getWFDOutputResolution(unsigned int *width, unsigned int *height) = 0;
    virtual int getWFDOutputInfo(int *fd1, int *fd2, struct wfd_layer_t *wfd_info) = 0;
    virtual int getPresentationMode(void) = 0;
    virtual void getHdmiResolution(uint32_t *width, uint32_t *height) = 0;
    virtual uint32_t getHdmiCableStatus() = 0;
    virtual uint32_t getHdmiAudioChannel() = 0;
};

/* Native Interface */
class BnExynosHWCService : public BnInterface<IExynosHWCService> {
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);

};
}
#endif
