LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include device/samsung/$(TARGET_DEVICE)/BoardConfig.mk

BOARD_USE_ANB := true

# Set to false to use Android's OMX header files
BOARD_USE_KHRONOS_OMX_HEADER := false

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), false)
ANDROID_MEDIA_INC := $(TOP)/frameworks/native/include/media
endif

EXYNOS_OMX_TOP := $(LOCAL_PATH)
EXYNOS_OMX_INC := $(EXYNOS_OMX_TOP)/include
EXYNOS_OMX_COMPONENT := $(EXYNOS_OMX_TOP)/component

EXYNOS_VIDEO_CODEC := \
	hardware/samsung/exynos4/libcodec/video
EXYNOS_AUDIO_CODEC := \
	hardware/samsung/exynos4/libcodec/audio

include $(EXYNOS_OMX_TOP)/osal/Android.mk
include $(EXYNOS_OMX_TOP)/core/Android.mk

include $(EXYNOS_OMX_COMPONENT)/common/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/h264/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/mpeg4/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/vp8/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/mpeg2/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/dec/vc1/Android.mk

include $(EXYNOS_OMX_COMPONENT)/video/enc/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/enc/h264/Android.mk
include $(EXYNOS_OMX_COMPONENT)/video/enc/mpeg4/Android.mk

ifeq ($(BOARD_USE_ALP_AUDIO), true)
include $(EXYNOS_OMX_COMPONENT)/audio/dec/Android.mk
include $(EXYNOS_OMX_COMPONENT)/audio/dec/mp3/Android.mk
endif

ifeq ($(BOARD_USE_WMA_CODEC), true)
include $(EXYNOS_OMX_COMPONENT)/audio/dec/wma/Android.mk
endif
