/*
 * Copyright (C) 2010, 2012 ARM Limited. All rights reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file ump_ref_drv.h
 *
 * Reference driver extensions to the UMP user space API for allocating UMP memory
 */

#ifndef _UNIFIED_MEMORY_PROVIDER_REF_DRV_H_
#define _UNIFIED_MEMORY_PROVIDER_REF_DRV_H_

#include "ump.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	/* This enum must match with the IOCTL enum in ump_ioctl.h */
	UMP_REF_DRV_CONSTRAINT_NONE = 0,
	UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR = 1,
	UMP_REF_DRV_CONSTRAINT_USE_CACHE = 4,
} ump_alloc_constraints;

/** Allocate an UMP handle containing a memory buffer.
 * Input: Size: The minimum size for the allocation.
 * Usage: If this is UMP_REF_DRV_CONSTRAINT_USE_CACHE, the allocation is mapped as cached by the cpu.
 *        If it is UMP_REF_DRV_CONSTRAINT_NONE it is mapped as noncached.
 *        The flag UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR is not supported.*/
UMP_API_EXPORT ump_handle ump_ref_drv_allocate(unsigned long size, ump_alloc_constraints usage);

typedef enum
{
	UMP_MSYNC_CLEAN = 0 ,
	UMP_MSYNC_CLEAN_AND_INVALIDATE = 1,
	UMP_MSYNC_READOUT_CACHE_ENABLED = 128,
} ump_cpu_msync_op;

/** Flushing cache for an ump_handle.
 * The function will always CLEAN_AND_INVALIDATE as long as the \a op is not UMP_MSYNC_READOUT_CACHE_ENABLED.
 * If so it will only report back if the given ump_handle is cacheable.
 * At the momement the implementation does not use \a address or \a size.
 * Return value is 1 if cache is enabled, and 0 if it is disabled for the given allocation.*/
UMP_API_EXPORT int ump_cpu_msync_now(ump_handle mem, ump_cpu_msync_op op, void* address, int size);


#ifdef __cplusplus
}
#endif

#endif /*_UNIFIED_MEMORY_PROVIDER_REF_DRV_H_ */
