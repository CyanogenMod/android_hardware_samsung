#
# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

ifeq ($(TARGET_BOARD_PLATFORM),exynos5)
common_exynos5_dirs := libgralloc_ump libhdmi libhwcomposer libfimg4x libcamera
exynos5250_dirs := $(common_exynos5_dirs)

ifeq ($(BOARD_USES_HWJPEG),true)
exynos5250_dirs += libhwjpeg
endif

ifeq ($(TARGET_SOC),exynos5250)
  include $(call all-named-subdir-makefiles,$(exynos5250_dirs))
endif
endif
