/*
 * Copyright (C) 2011 ARM Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GRALLOC_VSYNC_REPORT_H_
#define GRALLOC_VSYNC_REPORT_H_

#ifdef __cplusplus
extern "C"
{
#endif


typedef enum mali_vsync_event
{
	MALI_VSYNC_EVENT_BEGIN_WAIT = 0,
	MALI_VSYNC_EVENT_END_WAIT
} mali_vsync_event;

extern void _mali_base_arch_vsync_event_report(mali_vsync_event);

inline void gralloc_mali_vsync_report(mali_vsync_event event)
{
	_mali_base_arch_vsync_event_report(event);
}

#ifdef __cplusplus
}
#endif
#endif /* GRALLOC_VSYNC_REPORT_H_ */
