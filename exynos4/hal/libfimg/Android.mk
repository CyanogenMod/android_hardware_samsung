LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
    $(BOARD_HAL_PATH)/include \

LOCAL_SRC_FILES := \
	FimgApi.cpp   \
	FimgC210.cpp

LOCAL_SHARED_LIBRARIES:= liblog libutils libbinder

LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := libfimg

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
