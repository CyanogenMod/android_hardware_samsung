LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SRC_FILES := audio_hw.c ril_interface.c

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/expat/lib \
	system/media/audio_utils/include \
	system/media/audio_effects/include

LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libaudioutils \
	libdl libexpat

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
