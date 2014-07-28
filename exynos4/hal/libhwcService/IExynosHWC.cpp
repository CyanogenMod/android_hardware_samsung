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

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

#include <binder/Parcel.h>
#include <binder/IInterface.h>

#include "IExynosHWC.h"

namespace android {

enum {
    SET_WFD_MODE = 0,
    SET_WFD_OUTPUT_RESOLUTION,
    SET_EXT_FB_MODE,
    SET_CAMERA_MODE,
    SET_FORCE_MIRROR_MODE,
    SET_VIDEO_PLAY_STATUS,
    SET_EXTERNAL_DISPLAY_PAUSE,
    SET_DISPLAY_ORIENTATION,
    SET_PROTECTION_MODE,
    SET_EXTERNAL_DISP_LAY_NUM,
    SET_FORCE_GPU,
    SET_VIDEO_ROTATION,
    GET_VIDEO_ROTATION,
    SET_HDMI_CABLE_STATUS,
    SET_HDMI_MODE,
    SET_HDMI_RESOLUTION,
    SET_HDMI_HDCP,
    SET_HDMI_HWC_LAYER,
    SET_HDMI_ENABLE,
    SET_HDMI_LAYER_ENABLE,
    SET_HDMI_LAYER_DISABLE,
    SET_HDMI_AUDIO_CHANNEL,
    SET_HDMI_SUBTITLES,
    SET_HDMI_ROTATE,
    SET_HDMI_PATH,
    SET_HDMI_DRM,
    SET_PRESENTATION_MODE,
    GET_WFD_MODE,
    GET_WFD_OUTPUT_RESOLUTION,
    GET_WFD_OUTPUT_INFO,
    GET_PRESENTATION_MODE,
    GET_HDMI_CABLE_STATUS,
    GET_HDMI_RESOLUTION,
    GET_HDMI_AUDIO_CHANNEL,
    SET_WFD_SLEEP_CTRL,
};

class BpExynosHWCService : public BpInterface<IExynosHWCService> {
public:
    BpExynosHWCService(const sp<IBinder>& impl)
        : BpInterface<IExynosHWCService>(impl)
    {
    }

    virtual int setWFDMode(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_WFD_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setWFDOutputResolution(unsigned int width, unsigned int height,
                                       unsigned int disp_w, unsigned int disp_h)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(width);
        data.writeInt32(height);
        data.writeInt32(disp_w);
        data.writeInt32(disp_h);
        int result = remote()->transact(SET_WFD_OUTPUT_RESOLUTION, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual void setWFDSleepCtrl(bool black)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(black);
        remote()->transact(SET_WFD_SLEEP_CTRL, data, &reply);
    }

    virtual int setExtraFBMode(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_EXT_FB_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setCameraMode(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_CAMERA_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setForceMirrorMode(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_FORCE_MIRROR_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setVideoPlayStatus(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_VIDEO_PLAY_STATUS, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setExternalDisplayPause(bool onoff)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(onoff);
        int result = remote()->transact(SET_EXTERNAL_DISPLAY_PAUSE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setDispOrientation(unsigned int transform)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(transform);
        int result = remote()->transact(SET_DISPLAY_ORIENTATION, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setProtectionMode(unsigned int mode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(mode);
        int result = remote()->transact(SET_PROTECTION_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setExternalDispLayerNum(unsigned int num)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(num);
        int result = remote()->transact(SET_EXTERNAL_DISP_LAY_NUM, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setForceGPU(unsigned int on)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(on);
        int result = remote()->transact(SET_FORCE_GPU, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int setVideoRotation(unsigned int rotation_degree)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(rotation_degree);
        int result = remote()->transact(SET_VIDEO_ROTATION, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual int getVideoRotation(void)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        int result = remote()->transact(GET_VIDEO_ROTATION, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual void setHdmiResolution(int resolution, int s3dMode)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(resolution);
        data.writeInt32(s3dMode);
        remote()->transact(SET_HDMI_RESOLUTION, data, &reply);
    }

    virtual void setHdmiCableStatus(int status)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(status);
        remote()->transact(SET_HDMI_CABLE_STATUS, data, &reply);
    }

    virtual void setHdmiHdcp(int status)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(status);
        remote()->transact(SET_HDMI_HDCP, data, &reply);
    }

    virtual void setHdmiAudioChannel(uint32_t channels)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(channels);
        remote()->transact(SET_HDMI_AUDIO_CHANNEL, data, &reply);
    }

    virtual void setHdmiSubtitles(bool use)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(use);
        remote()->transact(SET_HDMI_SUBTITLES, data, &reply);
    }

    virtual void setPresentationMode(bool use)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        data.writeInt32(use);
        remote()->transact(SET_PRESENTATION_MODE, data, &reply);
    }

    virtual int getWFDMode()
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        int result = remote()->transact(GET_WFD_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual void getWFDOutputResolution(unsigned int *width, unsigned int *height)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        remote()->transact(GET_WFD_OUTPUT_RESOLUTION, data, &reply);
        *width  = reply.readInt32();
        *height = reply.readInt32();
    }

    virtual int getWFDOutputInfo(int *fd1, int *fd2, struct wfd_layer_t *wfd_info)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        remote()->transact(GET_WFD_OUTPUT_INFO, data, &reply);
        int y_fd  = reply.readFileDescriptor();
        int uv_fd = reply.readFileDescriptor();
        reply.read(wfd_info, sizeof(struct wfd_layer_t));

        if (y_fd && uv_fd) {
            *fd1 = dup(y_fd);
            *fd2 = dup(uv_fd);
        } else {
            *fd1 = *fd2 = -1;
        }

        return reply.readInt32();
    }

    virtual int getPresentationMode(void)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        int result = remote()->transact(GET_PRESENTATION_MODE, data, &reply);
        result = reply.readInt32();
        return result;
    }

    virtual void getHdmiResolution(uint32_t *width, uint32_t *height)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        remote()->transact(GET_HDMI_RESOLUTION, data, &reply);
        *width = (uint32_t)reply.readInt32();
        *height = (uint32_t)reply.readInt32();
    }

    virtual uint32_t getHdmiCableStatus()
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        remote()->transact(GET_HDMI_CABLE_STATUS, data, &reply);
        return (uint32_t)reply.readInt32();
    }

    virtual uint32_t getHdmiAudioChannel()
    {
        Parcel data, reply;
        data.writeInterfaceToken(IExynosHWCService::getInterfaceDescriptor());
        remote()->transact(GET_HDMI_AUDIO_CHANNEL, data, &reply);
        return (uint32_t)reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(ExynosHWCService, "android.hal.ExynosHWCService");

status_t BnExynosHWCService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case SET_WFD_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setWFDMode(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_WFD_OUTPUT_RESOLUTION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int width  = data.readInt32();
            int height = data.readInt32();
            int disp_w = data.readInt32();
            int disp_h = data.readInt32();
            int res = setWFDOutputResolution(width, height, disp_w, disp_h);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_WFD_SLEEP_CTRL: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            bool mode = data.readInt32();
            setWFDSleepCtrl(mode);
            return NO_ERROR;
        } break;
        case SET_EXT_FB_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setExtraFBMode(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_CAMERA_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setCameraMode(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_FORCE_MIRROR_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setForceMirrorMode(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_VIDEO_PLAY_STATUS: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setVideoPlayStatus(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_EXTERNAL_DISPLAY_PAUSE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            bool onoff = data.readInt32();
            int res = setExternalDisplayPause(onoff);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_DISPLAY_ORIENTATION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int transform = data.readInt32();
            int res = setDispOrientation(transform);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_PROTECTION_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int mode = data.readInt32();
            int res = setProtectionMode(mode);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_EXTERNAL_DISP_LAY_NUM: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int num = data.readInt32();
            int res = setExternalDispLayerNum(num);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_FORCE_GPU: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int on = data.readInt32();
            int res = setForceGPU(on);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case SET_VIDEO_ROTATION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int rotation_degree = data.readInt32();
            int res = setVideoRotation(rotation_degree);
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case GET_VIDEO_ROTATION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int rotation_degree = getVideoRotation();
            reply->writeInt32(rotation_degree);
            return NO_ERROR;
        } break;
        case SET_HDMI_RESOLUTION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int resolution = data.readInt32();
            int s3dMode = data.readInt32();
            setHdmiResolution(resolution, s3dMode);
            return NO_ERROR;
        } break;
        case GET_HDMI_RESOLUTION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            uint32_t width, height;
            getHdmiResolution(&width, &height);
            reply->writeInt32(width);
            reply->writeInt32(height);
            return NO_ERROR;
        } break;
        case SET_HDMI_CABLE_STATUS: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int status = data.readInt32();
            setHdmiCableStatus(status);
            return NO_ERROR;
        } break;
        case GET_HDMI_CABLE_STATUS: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            reply->writeInt32(getHdmiCableStatus());
            return NO_ERROR;
        } break;
        case SET_HDMI_HDCP: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int status = data.readInt32();
            setHdmiHdcp(status);
            return NO_ERROR;
        } break;
        case GET_HDMI_AUDIO_CHANNEL: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            reply->writeInt32(getHdmiAudioChannel());
            return NO_ERROR;
        } break;
        case SET_HDMI_AUDIO_CHANNEL: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int channels = data.readInt32();
            setHdmiAudioChannel(channels);
            return NO_ERROR;
        } break;
        case SET_HDMI_SUBTITLES: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int use = data.readInt32();
            setHdmiSubtitles(use);
            return NO_ERROR;
        } break;
        case SET_PRESENTATION_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int use = data.readInt32();
            setPresentationMode(use);
            return NO_ERROR;
        } break;

        case GET_WFD_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int res = getWFDMode();
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case GET_WFD_OUTPUT_RESOLUTION: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            uint32_t width, height;
            getWFDOutputResolution(&width, &height);
            reply->writeInt32(width);
            reply->writeInt32(height);
            return NO_ERROR;
        } break;
        case GET_WFD_OUTPUT_INFO: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int fd1, fd2;
            struct wfd_layer_t wfd_info;
            int res = getWFDOutputInfo(&fd1, &fd2, &wfd_info);
            reply->writeFileDescriptor(fd1);
            reply->writeFileDescriptor(fd2);
            reply->write(&wfd_info, sizeof(struct wfd_layer_t));
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        case GET_PRESENTATION_MODE: {
            CHECK_INTERFACE(IExynosHWCService, data, reply);
            int res = getPresentationMode();
            reply->writeInt32(res);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}
}
