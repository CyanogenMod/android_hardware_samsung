#
# Copyright (C) 2012 The Android Open Source Project
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

common_exynos_dirs := \
	libv4l2 \
	libexynosutils \
	libcsc \
	libstagefrighthw \
	openmax

ifneq ($(TARGET_SOC),exynos4210)
common_exynos_dirs := \
	libfimg4x
endif
include $(call all-named-subdir-makefiles,$(common_exynos_dirs))
