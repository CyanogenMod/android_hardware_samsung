ifeq ($(BOARD_HAVE_SAMSUNG_WIFI),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    macloader.c

LOCAL_SHARED_LIBRARIES := \
    liblog libutils

LOCAL_MODULE := macloader
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

endif
