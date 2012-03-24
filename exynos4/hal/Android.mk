ifeq ($(TARGET_BOARD_PLATFORM),exynos4)
exynos4_dirs := libfimg libhwconverter liblights libs5pjpeg libsensors libswconverter libump
include $(call all-named-subdir-makefiles,$(exynos4_dirs))
endif
