LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	color_space_convertor.h \
	csc_fimc.h

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	color_space_convertor.c \
	csc_fimc.cpp \
	csc_tiled_to_linear_y_neon.s \
	csc_tiled_to_linear_uv_neon.s \
	csc_tiled_to_linear_uv_deinterleave_neon.s \
	csc_interleave_memcpy_neon.s

LOCAL_C_INCLUDES := \
	$(TOP)/device/samsung/multimedia/openmax/include/khronos \
	$(TOP)/device/samsung/$(TARGET_BOARD_PLATFORM)/include

LOCAL_MODULE := libseccscapi

LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS :=

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog libfimc

include $(BUILD_STATIC_LIBRARY)
