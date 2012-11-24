LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	swscaler.c \
	SW_Scale_up_Y_NEON.S \
	SW_Scale_up_CbCr_NEON.S \
	SW_Memcpy_NEON.S

LOCAL_SHARED_LIBRARIES := \
	libutils

LOCAL_MODULE:= libswscaler
LOCAL_ARM_MODE := arm

LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include

include $(BUILD_SHARED_LIBRARY)
