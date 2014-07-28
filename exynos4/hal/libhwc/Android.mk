# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libEGL libGLESv1_CM libhardware \
	libhardware_legacy libion libutils libsync libexynosv4l2 libexynosutils libexynosfimc

ifeq ($(BOARD_USES_HWC_SERVICES),true)
	LOCAL_SHARED_LIBRARIES += libExynosHWCService
	LOCAL_CFLAGS += -DHWC_SERVICES

ifeq ($(BOARD_USES_WFD),true)
	LOCAL_CFLAGS += -DUSES_WFD
	LOCAL_SHARED_LIBRARIES += libfimg
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libfimg4x
endif

ifeq ($(BOARD_USE_S3D_SUPPORT),true)
	LOCAL_CFLAGS += -DS3D_SUPPORT
endif
endif

ifeq ($(BOARD_USES_CEC),true)
	LOCAL_SHARED_LIBRARIES += libcec
	LOCAL_CFLAGS += -DUSES_CEC
endif

ifeq ($(BOARD_USES_GSC_VIDEO),true)
	LOCAL_CFLAGS += -DGSC_VIDEO
endif

ifeq ($(BOARD_USES_FB_PHY_LINEAR),true)
	LOCAL_CFLAGS += -DUSE_FB_PHY_LINEAR
endif

ifeq ($(BOARD_USES_VFB),true)
	LOCAL_CFLAGS += -DUSES_VFB
endif

#ifeq ($(BOARD_HDMI_INCAPABLE), true)
	LOCAL_CFLAGS += -DHDMI_INCAPABLE
#endif

LOCAL_CFLAGS += -DLOG_TAG=\"ExynosHWC\"

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libexynosutils \
	$(LOCAL_PATH)/../libgralloc_ump
	#$(TOP)/hardware/samsung/$(TARGET_SOC)/hal/libhwcmodule \

ifeq ($(BOARD_USE_GRALLOC_FLAG_FOR_HDMI),true)
	LOCAL_CFLAGS += -DUSE_GRALLOC_FLAG_FOR_HDMI
	LOCAL_SHARED_LIBRARIES += libfimg
	LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libfimg4x
endif

LOCAL_SRC_FILES := ExynosHWC.cpp

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

