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
 * @file ump_uk_types.h
 * Defines the types and constants used in the user-kernel interface
 */

#ifndef __UMP_UK_TYPES_H__
#define __UMP_UK_TYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Helpers for API version handling */
#define MAKE_VERSION_ID(x) (((x) << 16UL) | (x))
#define IS_VERSION_ID(x) (((x) & 0xFFFF) == (((x) >> 16UL) & 0xFFFF))
#define GET_VERSION(x) (((x) >> 16UL) & 0xFFFF)
#define IS_API_MATCH(x, y) (IS_VERSION_ID((x)) && IS_VERSION_ID((y)) && (GET_VERSION((x)) == GET_VERSION((y))))

/**
 * API version define.
 * Indicates the version of the kernel API
 * The version is a 16bit integer incremented on each API change.
 * The 16bit integer is stored twice in a 32bit integer
 * So for version 1 the value would be 0x00010001
 */
#define UMP_IOCTL_API_VERSION MAKE_VERSION_ID(2)

typedef enum
{
	_UMP_IOC_QUERY_API_VERSION = 1,
	_UMP_IOC_ALLOCATE,
	_UMP_IOC_RELEASE,
	_UMP_IOC_SIZE_GET,
	_UMP_IOC_MAP_MEM,    /* not used in Linux */
	_UMP_IOC_UNMAP_MEM,  /* not used in Linux */
	_UMP_IOC_MSYNC,
}_ump_uk_functions;

typedef enum
{
	UMP_REF_DRV_UK_CONSTRAINT_NONE = 0,
	UMP_REF_DRV_UK_CONSTRAINT_PHYSICALLY_LINEAR = 1,
	UMP_REF_DRV_UK_CONSTRAINT_USE_CACHE = 4,
} ump_uk_alloc_constraints;

typedef enum
{
	_UMP_UK_MSYNC_CLEAN = 0,
	_UMP_UK_MSYNC_CLEAN_AND_INVALIDATE = 1,
	_UMP_UK_MSYNC_READOUT_CACHE_ENABLED = 128,
} ump_uk_msync_op;

/**
 * Get API version ([in,out] u32 api_version, [out] u32 compatible)
 */
typedef struct _ump_uk_api_version_s
{
	void *ctx;      /**< [in,out] user-kernel context (trashed on output) */
	u32 version;    /**< Set to the user space version on entry, stores the device driver version on exit */
	u32 compatible; /**< Non-null if the device is compatible with the client */
} _ump_uk_api_version_s;

/**
 * ALLOCATE ([out] u32 secure_id, [in,out] u32 size,  [in] contraints)
 */
typedef struct _ump_uk_allocate_s
{
	void *ctx;                              /**< [in,out] user-kernel context (trashed on output) */
	u32 secure_id;                          /**< Return value from DD to Userdriver */
	u32 size;                               /**< Input and output. Requested size; input. Returned size; output */
	ump_uk_alloc_constraints constraints;   /**< Only input to Devicedriver */
} _ump_uk_allocate_s;

/**
 * SIZE_GET ([in] u32 secure_id, [out]size )
 */
typedef struct _ump_uk_size_get_s
{
	void *ctx;                              /**< [in,out] user-kernel context (trashed on output) */
	u32 secure_id;                          /**< Input to DD */
	u32 size;                               /**< Returned size; output */
} _ump_uk_size_get_s;

/**
 * Release ([in] u32 secure_id)
 */
typedef struct _ump_uk_release_s
{
	void *ctx;                              /**< [in,out] user-kernel context (trashed on output) */
	u32 secure_id;                          /**< Input to DD */
} _ump_uk_release_s;

typedef struct _ump_uk_map_mem_s
{
	void *ctx;                      /**< [in,out] user-kernel context (trashed on output) */
	void *mapping;                  /**< [out] Returns user-space virtual address for the mapping */
	void *phys_addr;                /**< [in] physical address */
	unsigned long size;             /**< [in] size */
	u32 secure_id;                  /**< [in] secure_id to assign to mapping */
	void * _ukk_private;            /**< Only used inside linux port between kernel frontend and common part to store vma */
	u32 cookie;
	u32 is_cached;            /**< [in,out] caching of CPU mappings */
} _ump_uk_map_mem_s;

typedef struct _ump_uk_unmap_mem_s
{
	void *ctx;            /**< [in,out] user-kernel context (trashed on output) */
	void *mapping;
	u32 size;
	void * _ukk_private;
	u32 cookie;
} _ump_uk_unmap_mem_s;

typedef struct _ump_uk_msync_s
{
	void *ctx;            /**< [in,out] user-kernel context (trashed on output) */
	void *mapping;        /**< [in] mapping addr */
	void *address;        /**< [in] flush start addr */
	u32 size;             /**< [in] size to flush */
	ump_uk_msync_op op;   /**< [in] flush operation */
	u32 cookie;           /**< [in] cookie stored with reference to the kernel mapping internals */
	u32 secure_id;        /**< [in] cookie stored with reference to the kernel mapping internals */
	u32 is_cached;        /**< [out] caching of CPU mappings */
} _ump_uk_msync_s;

#ifdef __cplusplus
}
#endif

#endif /* __UMP_UK_TYPES_H__ */
