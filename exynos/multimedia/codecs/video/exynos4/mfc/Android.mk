LOCAL_PATH := $(call my-dir)

#
# mfc combined api
#

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE := libsecmfcapi
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	dec/src/SsbSipMfcDecAPI.c \
	enc/src/SsbSipMfcEncAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include/

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS += -DCONFIG_MFC_FPS
#LOCAL_CFLAGS += -DCONFIG_MFC_PERF_LOG
endif

ifeq ($(TARGET_SOC),exynos4210)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

ifeq ($(BOARD_USE_S3D_SUPPORT), true)
LOCAL_CFLAGS += -DS3D_SUPPORT
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_STATIC_LIBRARY)

#
# mfc dec api
#

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE := libsecmfcdecapi
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	dec/src/SsbSipMfcDecAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include/

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS += -DCONFIG_MFC_FPS
#LOCAL_CFLAGS += -DCONFIG_MFC_PERF_LOG
endif

ifeq ($(TARGET_SOC),exynos4210)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

ifeq ($(BOARD_USE_S3D_SUPPORT), true)
LOCAL_CFLAGS += -DS3D_SUPPORT
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_SHARED_LIBRARY)

#
# mfc enc api
#

include $(CLEAR_VARS)

LOCAL_COPY_HEADERS_TO := libsecmm
LOCAL_COPY_HEADERS := \
	include/mfc_errno.h \
	include/mfc_interface.h \
	include/SsbSipMfcApi.h

LOCAL_MODULE := libsecmfcencapi
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	enc/src/SsbSipMfcEncAPI.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include/

LOCAL_PRELINK_MODULE := false

ifeq ($(BOARD_USES_MFC_FPS),true)
LOCAL_CFLAGS += -DCONFIG_MFC_FPS
#LOCAL_CFLAGS += -DCONFIG_MFC_PERF_LOG
endif

ifeq ($(TARGET_SOC),exynos4210)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

ifeq ($(BOARD_USE_S3D_SUPPORT), true)
LOCAL_CFLAGS += -DS3D_SUPPORT
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := liblog

include $(BUILD_SHARED_LIBRARY)
