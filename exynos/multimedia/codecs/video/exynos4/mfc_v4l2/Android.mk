LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	dec/src/SsbSipMfcDecAPI.c \
	enc/src/SsbSipMfcEncAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	device/samsung/$(TARGET_BOARD_PLATFORM)/include

LOCAL_MODULE := libsecmfcapi

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS := -DCONFIG_MFC_FPS
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_STATIC_LIBRARY)

#
# MFC Dec API
#
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	dec/src/SsbSipMfcDecAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	device/samsung/$(TARGET_BOARD_PLATFORM)/include

LOCAL_MODULE := libsecmfcdecapi

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS := -DCONFIG_MFC_FPS
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_SHARED_LIBRARY)

#
# MFC Enc API
#
include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	enc/src/SsbSipMfcEncAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	device/samsung/$(TARGET_BOARD_PLATFORM)/include

LOCAL_MODULE := libsecmfcencapi

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS := -DCONFIG_MFC_FPS
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_STATIC_LIBRARY)
