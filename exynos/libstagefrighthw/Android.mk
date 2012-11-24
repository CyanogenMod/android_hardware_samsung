LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	Exynos_OMX_Plugin.cpp

LOCAL_CFLAGS += $(PV_CFLAGS_MINUS_VISIBILITY)

LOCAL_C_INCLUDES:= \
	frameworks/native/include/media/hardware \
	frameworks/native/include/media/openmax \
	frameworks/native/include

LOCAL_SHARED_LIBRARIES :=    \
	libbinder            \
	libutils             \
	libcutils            \
	libui                \
	libdl                \
	libstagefright_foundation

LOCAL_MODULE := libstagefrighthw

LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

