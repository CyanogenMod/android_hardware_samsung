/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file ump.h
 *
 * This file contains the user space part of the UMP API.
 *
 */

#ifndef _UMP_H_
#define _UMP_H_

/**
 * @page page_base_ump Unified Memory Provider API
 *
 * UMP(Universal Memory Provider) is an API to allocate memory with some special unique requirements;
 * @li Known physical addresses
 * @li Non-relocatable/pinned
 * @li Won't be paged out
 * @li Shareable between processes (selectable per allocation for security reasons)
 * @li Shareable with multiple hardware devices
 * @li Physically contiguous (optional)
 * @li Extended (not valid with the physically contiguous requirement for obvious reasons)
 *
 * Allocations from UMP can safely be used with hardware devices and other processes.
 * All uses are reference counted, so memory won't be released until all participating hardware devices and processes have released their use of the allocation.
 * This means that even if a process frees memory too early or crashes any hardware using the memory won't corrupt freed memory.
 *
 * Allocations inside a process is represented using an UMP memory handle.
 *
 * Each allocation is represented by a system-wide unique ID (called a secure ID),
 * which can be obtained from a handle and be shared with other processes or given to a device driver.
 *
 * Based on a secure ID a new handle can be created either in kernel space by a driver
 * or in user space by some other process to use the same allocation.
 *
 * Based on the handle a driver in kernel space can obtain information about the physical memory block(s)
 * an allocation consists of and increment or decrement the reference count.
 *
 * Usage in user-space also adds a reference to the memory, but it's managed by the UMP device driver.
 *
 * The user-space reference count is only local to the process, so a process can't by accident decrement
 * the count one time too many and cause the memory to be freed while it's in use by a hardware device.
 *
 * This is all handled by the UMP kernel code, no user-space code cooperation is needed.
 *
 * By default an allocation is only accessible in the same process or what other security boundary the OS uses.
 * If marked as shared it can be accessed in all processes, the kernel space customer defined security filter permitting of course.
 * See @ref ump_dd_security_filter for more information about this security filter.
 *
 * @sa ump_api
 * @sa example_user_api.c
 * @sa example_kernel_api.c
 *
 * @example example_user_api.c
 * @example example_kernel_api.c
 *
 */

/** @defgroup ump_api Unified Memory Provider APIs
 */

/**
 * @addtogroup ump_api
 * @{
 */

/** @defgroup ump_user_space_api UMP User Space API
 * @{ */


#include "ump_platform.h"
#include "ump_common.h"
#include "ion.h"

#ifndef __KERNEL__
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * External representation of a UMP handle in user space.
 */
typedef void * ump_handle;

/**
 * Value to indicate an invalid UMP memory handle.
 */
#define UMP_INVALID_MEMORY_HANDLE ((ump_handle)0)

/**
 * Opens and initializes the UMP library.
 *
 * This function must be called at least once before calling any other UMP API functions.
 * Each successful open call is reference counted and must be matched with a call to @ref ump_close.
 * It is safe to call @a ump_open after a @a ump_close has terminated a previous session.
 *
 * UMP_ERROR will be returned if:
 *   - the reference count is ULONG_MAX so would overflow.
 *   - the reference count is 0 and the backend fails to open.
 *
 * UMP API: v1 and v2
 * @see ump_close
 *
 * @return UMP_OK indicates success, UMP_ERROR indicates failure.
 */
UMP_API_EXPORT ump_result ump_open(void) CHECK_RESULT;


/**
 * Terminate the UMP library.
 *
 * This must be called once for every successful @ref ump_open. The UMP library is
 * terminated when, and only when, the last open reference to the UMP interface is closed.
 *
 * If this is called while having active allocations or mappings the behavior is undefined.
 *
 * UMP API: v1 and v2
 * @see ump_open
 */
UMP_API_EXPORT void ump_close(void);


/**
 * Retrieves the secure ID for the specified UMP memory.
 *
 * This identifier is unique across the entire system, and uniquely identifies
 * the specified UMP memory allocation. This identifier can later be used through the
 * v2 API:
 * @ref ump_from_secure_id or
 * @ref ump_dd_from_secure_id
 * v1 API:
 * @ref ump_handle_create_from_secure_id or
 * @ref ump_dd_handle_create_from_secure_id
 *
 * functions in order to access this UMP memory, for instance from another process.
 * Unless the allocation was marked as shared the returned ID will only be resolvable in the same process as did the allocation.
 *
 * If called on an @a UMP_INVALID_MEMORY_HANDLE it will return @a UMP_INVALID_SECURE_ID.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_secure_id_get
 *
 * UMP API: v1 and v2
 *
 * @see ump_dd_secure_id_get
 * v2 API:
 * @see ump_from_secure_id
 * @see ump_dd_from_secure_id
 * v1 API:
 * @see ump_handle_create_from_secure_id
 * @see ump_dd_from_secure_id
 *
 * @param mem Handle to UMP memory.
 *
 * @return Returns the secure ID for the specified UMP memory.
 */
UMP_API_EXPORT ump_secure_id ump_secure_id_get(const ump_handle mem) CHECK_RESULT;


/**
 * Synchronous mapping cache sync operation.
 *
 * Performs the requested CPU side cache sync operations before returning.
 * A clean must be done before the memory is guaranteed to be visible in main memory.
 * Any device-specific cache clean/invalidate must be done in combination with this routine, if needed.
 * Function returns cache status for the allocation.
 *
 * Example:
 * @code
 * 	ump_cpu_msync_now(handle, UMP_MSYNC_CLEAN, ptr, size);
 * 	device_invalidate(...);
 * 	// ... run device ...
 * 	device_clean(...);
 * 	ump_cpu_msync_now(handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, ptr, size);
 * 	// ... safe to access on the cpu side again ...
 * @endcode
 *
 *
 * Calls to operate on an @a UMP_INVALID_MEMORY_HANDLE will result in undefined behavior.
 * Debug builds will assert on this.
 *
 * If @a address is not inside a mapping previously obtained from the @a ump_handle provided results in undefined behavior.
 * If @a address combined with @a size results on reaching beyond the end of the buffer results in undefined behavior.
 *
 * UMP API: v1 and v2
 * @param mem Handle to UMP memory
 * @param op Cache operation to perform
 * @param[in] address The CPU address where to start the sync operation, this can be at an offset from the start of the allocation.
 * @param size The number of bytes to be synced.
 *
 * @return Returns 1 if cache is enabled, 0 if cache is disabled for the given allocation.
 *
 */
UMP_API_EXPORT int ump_cpu_msync_now(ump_handle mem, ump_cpu_msync_op op, void * address, size_t size);


#ifndef UMP_BLOCK_V2_API

/**
 * Allocate a buffer.
 * The life-time of the allocation is controlled by a reference count.
 * The reference count of the returned buffer is set to 1.
 * The memory will be freed once the reference count reaches 0.
 * Use @ref ump_retain and @ref ump_release to control the reference count.
 * The contens of the memory returned will be zero initialized.
 *
 * The @ref UMP_V1_API_DEFAULT_ALLOCATION_FLAGS can be used
 * to create a buffer that can be shared with v1 API applications.
 * The allocation will be limited to 32-bit PA.
 *
 * The @ref UMP_CONSTRAINT_UNCACHED flag disables cache for all cpu mappings for this allocation.
 *
 * UMP API: v2
 * @param size Number of bytes to allocate. Will be padded up to a multiple of the page size.
 * @param flags Bit-wise OR of zero or more of the allocation flag bits.
 * @return Handle to the new allocation, or @a UMP_INVALID_MEMORY_HANDLE on allocation failure.
 */
UMP_API_EXPORT ump_handle ump_allocate_64(u64 size, ump_alloc_flags flags) CHECK_RESULT;


/**
 * Creates a handle based on a shared UMP memory allocation.
 *
 * The usage of UMP memory is reference counted, so this will increment the reference
 * count by one for the specified UMP memory.
 *
 * If called on an @a UMP_INVALID_SECURE_ID this will return @a UMP_INVALID_MEMORY_HANDLE.
 * If called on an non-shared allocation and this is a different process @a UMP_INVALID_MEMORY_HANDLE will be returned.
 *
 * Use @ref ump_release when there is no longer any
 * use for the retrieved handle.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_from_secure_id
 *
 * UMP API: v2
 * @see ump_release
 * @see ump_dd_from_secure_id
 *
 * @param secure_id The secure ID of the UMP memory to open, that can be retrieved using the @ref ump_secure_id_get function.
 *
 * @return @a UMP_INVALID_MEMORY_HANDLE indicates failure, otherwise a valid handle is returned.
 */
UMP_API_EXPORT ump_handle ump_from_secure_id(ump_secure_id secure_id) CHECK_RESULT;


/**
 * 64 bit version of @ref ump_size_get. Retrieves the actual size of the specified UMP memory.
 *
 * The size is reported in bytes, and is typically a multiple of the page size.
 * If called on an @a UMP_INVALID_MEMORY_HANDLE will result in undefined behavior.
 * Debug builds will assert on this.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_size_get_64
 *
 * UMP API: v2
 * @see ump_dd_size_get_64
 *
 * @param mem Handle to UMP memory.
 *
 * @return Returns the allocated 64-bit size of the specified UMP memory, in bytes.
 */
UMP_API_EXPORT u64 ump_size_get_64(const ump_handle mem) CHECK_RESULT;


/**
 * Retrieves a memory mapped pointer to the specified UMP memory.
 *
 * This function retrieves a memory mapped pointer to the specified UMP memory, that can be used by the CPU.@n
 * Every successful call to @a ump_map must be matched with a call to @ref ump_unmap when the mapping is no longer needed.
 *
 * An offset and/or size resulting in going beyond the end of the buffer will case  the function to return NULL.
 *
 * Calling on @a UMP_INVALID_MEMORY_HANDLE results in undefined behavior.
 * Debug builds will assert on this.
 *
 * @note Systems without a MMU for the CPU only return the physical address, because no mapping is required.
 *
 * UMP API: v2
 * @see ump_unmap
 *
 * @param mem Handle to UMP memory.
 * @param offset An offset at which the mapping begins.
 * @param size The number of bytes to map. Passing 0 does not retrieve a memory mapped
 * pointer - instead NULL is returned.
 *
 * @return NULL indicates failure, otherwise a CPU mapped pointer is returned.
 */
UMP_API_EXPORT void * ump_map(ump_handle mem, u64 offset, size_t size) CHECK_RESULT;


/**
 * Releases a previously mapped pointer to the specified UMP memory.
 *
 * Every successful call to @ref ump_map must be matched with a call to @a ump_unmap when the mapping is no longer needed.
 *
 * The following results in undefined behavior:
 * - Called with an address not returned from @ref ump_map
 * - Called with a different @a ump_handle than was used to obtain the pointer
 * - Called with a different @a size than was used to obtain the pointer
 *
 * @note Systems without a MMU must still implement this function, even though no unmapping should be needed.
 *
 * UMP API: v2
 * @param mem Handle to UMP memory.
 * @param[in] address The CPU virtual address returned by @ref ump_map
 * @param size Size matching argument given to ump_map
 */
UMP_API_EXPORT void ump_unmap(ump_handle mem, void* address, size_t size);


/**
 * Adds an extra reference to the specified UMP memory.
 *
 * This function adds an extra reference to the specified UMP memory. This function should
 * be used every time a UMP memory handle is duplicated, that is, assigned to another ump_handle
 * variable. The function @ref ump_release must then be used
 * to release each copy of the UMP memory handle.
 *
 * It's safe to call this on both shared and non-shared handles.
 * Calling on an @a UMP_INVALID_MEMORY_HANDLE results in undefined behavior.
 * Debug builds will assert on this.
 *
 * @note You are not required to call @ref ump_retain
 * for UMP handles returned from
 * @ref ump_from_secure_id,
 * because these handles are already reference counted by this function.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_retain
 *
 * UMP API: v2
 * @see ump_dd_retain
 *
 * @param mem Handle to UMP memory.
 * @return UMP_OK indicates success, UMP_ERROR indicates failure.
 */
UMP_API_EXPORT ump_result ump_retain(ump_handle mem);


/**
 * Releases a reference from the specified UMP memory.
 *
 * This function should be called once for every reference to the UMP memory handle.
 * When the last reference is released, all resources associated with this UMP memory
 * handle are freed.
 *
 * One can only call ump_release when matched with a successful ump_retain, ump_allocate_64 or ump_from_secure_id
 * It's safe to call this on both shared and non-shared handles.
 * If called on an @a UMP_INVALID_MEMORY_HANDLE it will return early.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_release
 *
 * UMP API: v2
 * @see ump_release
 *
 * @param mem Handle to UMP memory.
 */
UMP_API_EXPORT void ump_release(ump_handle mem);


/**
 * Import external memory into UMP.
 *
 * This function creates a ump_handle wrapping memory provided by some external source.
 *
 * For reference counted types the returned handle represents one new reference,
 * which must be freed using @a ump_release.
 * The handle passed in still holds its reference and can still be used and must be released
 * as it would be normally.
 *
 * For ownership based types the returned handle has claimed the ownership which will be released
 * with @a ump_release.
 * The handle passed in is no longer valid.
 *
 * A pointer to the handle type is required, not the type it self.
 *
 * The flags are used in the same way as for @a ump_allocate_64, except that these flags are ignored:
 * @li UMP_CONSTRAINT_32BIT_ADDRESSABLE
 * @li UMP_CONSTRAINT_PHYSICALLY_LINEAR
 *
 * The returned UMP handle can be used as any other ump_handle.
 *
 * Example for UMP_EXTERNAL_MEM_TYPE_ION:
 *
 * @code
 * ump_handle h;
 * ump_alloc_flags flags = get_requested_flags();
 * int fd = ion_fd_get();
 * h = ump_import(UMP_EXTERNAL_MEM_TYPE_ION, &fd, flags);
 * // native release
 * close(fd);
 * ...
 * ump_release(h);
 * @endcode
 *
 * Example for a generic ownership based type:
 *
 * @code
 * ump_handle h;
 * ump_alloc_flags = get_requested_flags();
 * type t = type_claim();
 * h = ump_import(UMP_OWNERSHIP_BASED_TYPE, &t, flags);
 * // native handle no longer valid
 * t = INVALID;
 * ...
 * ump_release(h);
 * @endcode
 *
 * UMP API: v2
 * @see ump_release
 *
 * @param type Type of external memory to import
 * @param phandle Pointer to the handle to import.
 * @param flags Bit-wise OR of zero or more of the allocation flag bits.
 * @return Handle wrapping the imported memory, or @a UMP_INVALID_MEMORY_HANDLE on import failure.
 */
UMP_API_EXPORT ump_handle ump_import(enum ump_external_memory_type type, void * phandle, ump_alloc_flags flags) CHECK_RESULT;

#endif /* UMP_BLOCK_V2_API */

/** @name UMP v1 API
 * Functions provided to support compatibility with UMP v1 API
 *
 * You should use v1 API only with handles created with @ref ump_ref_drv_allocate
 * and @ref ump_handle_create_from_secure_id.
 * Using v1 API for handles created with v2 API can cause undefined behavior.
 *
 *@{
 */

#ifndef UMP_BLOCK_V1_API

/** Allocate an UMP handle containing a memory buffer.
 *
 * If usage is UMP_REF_DRV_CONSTRAINT_USE_CACHE, the allocation is mapped as cached by the cpu.
 * If it is UMP_REF_DRV_CONSTRAINT_NONE it is mapped as noncached.
 * The flag UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR is not supported.
 *
 * UMP API: v1
 * @param size The minimum size for the allocation.
 * @param usage The allocation constraints.
 */

UMP_API_EXPORT ump_handle ump_ref_drv_allocate(unsigned long size, ump_alloc_constraints usage);


/**
 * Retrieves the actual size of the specified UMP memory.
 *
 * The size is reported in bytes, and is typically page aligned.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_size_get "ump_dd_size_get"
 *
 * UMP API: v1
 * @see ump_dd_size_get
 *
 * @param mem Handle to UMP memory.
 *
 * @return Returns the allocated size of the specified UMP memory, in bytes.
 */
UMP_API_EXPORT unsigned long ump_size_get(ump_handle mem) CHECK_RESULT;


/**
 * Retrieves a handle to allocated UMP memory.
 *
 * The usage of UMP memory is reference counted, so this will increment the reference
 * count by one for the specified UMP memory.
 * Use @ref ump_reference_release "ump_reference_release" when there is no longer any
 * use for the retrieved handle.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_handle_create_from_secure_id "ump_dd_handle_create_from_secure_id"
 *
 * UMP API: v1
 * @see ump_reference_release
 * @see ump_dd_handle_create_from_secure_id
 *
 * @param secure_id The secure ID of the UMP memory to open, that can be retrieved using the @ref ump_secure_id_get "ump_secure_id_get " function.
 *
 * @return UMP_INVALID_MEMORY_HANDLE indicates failure, otherwise a valid handle is returned.
 */
UMP_API_EXPORT ump_handle ump_handle_create_from_secure_id(ump_secure_id secure_id) CHECK_RESULT;


/**
 * Adds an extra reference to the specified UMP memory.
 *
 * This function adds an extra reference to the specified UMP memory. This function should
 * be used every time a UMP memory handle is duplicated, that is, assigned to another ump_handle
 * variable. The function @ref ump_reference_release "ump_reference_release" must then be used
 * to release each copy of the UMP memory handle.
 *
 * @note You are not required to call @ref ump_reference_add "ump_reference_add"
 * for UMP handles returned from
 * @ref ump_handle_create_from_secure_id "ump_handle_create_from_secure_id",
 * because these handles are already reference counted by this function.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_reference_add "ump_dd_reference_add"
 *
 * UMP API: v1
 * @see ump_dd_reference_add
 *
 * @param mem Handle to UMP memory.
 */
UMP_API_EXPORT void ump_reference_add(ump_handle mem);


/**
 * Releases a reference from the specified UMP memory.
 *
 * This function should be called once for every reference to the UMP memory handle.
 * When the last reference is released, all resources associated with this UMP memory
 * handle are freed.
 *
 * @note There is a kernel space equivalent function called @ref ump_dd_reference_release "ump_dd_reference_release"
 *
 * UMP API: v1
 * @see ump_dd_reference_release
 *
 * @param mem Handle to UMP memory.
 */
UMP_API_EXPORT void ump_reference_release(ump_handle mem);


/**
 * Retrieves a memory mapped pointer to the specified UMP memory.
 *
 * This function retrieves a memory mapped pointer to the specified UMP memory,
 * that can be used by the CPU. Every successful call to
 * @ref ump_mapped_pointer_get "ump_mapped_pointer_get" is reference counted,
 * and must therefor be followed by a call to
 * @ref ump_mapped_pointer_release "ump_mapped_pointer_release " when the
 * memory mapping is no longer needed.
 *
 * @note Systems without a MMU for the CPU only return the physical address, because no mapping is required.
 *
 * UMP API: v1
 * @see ump_mapped_pointer_release
 *
 * @param mem Handle to UMP memory.
 *
 * @return NULL indicates failure, otherwise a CPU mapped pointer is returned.
 */
UMP_API_EXPORT void * ump_mapped_pointer_get(ump_handle mem);


/**
 * Releases a previously mapped pointer to the specified UMP memory.
 *
 * The CPU mapping of the specified UMP memory memory is reference counted,
 * so every call to @ref ump_mapped_pointer_get "ump_mapped_pointer_get" must
 * be matched with a call to this function when the mapping is no longer needed.
 *
 * The CPU mapping is not removed before all references to the mapping is released.
 *
 * UMP API: v1
 * @note Systems without a MMU must still implement this function, even though no unmapping should be needed.
 *
 * @param mem Handle to UMP memory.
 */
UMP_API_EXPORT void ump_mapped_pointer_release(ump_handle mem);


/**
 * Read from specified UMP memory.
 *
 * Another way of reading from (and writing to) UMP memory is to use the
 * @ref ump_mapped_pointer_get "ump_mapped_pointer_get" to retrieve
 * a CPU mapped pointer to the memory.
 *
 * UMP API: v1
 * @see ump_mapped_pointer_get
 *
 * @param dst Destination buffer.
 * @param src Handle to UMP memory to read from.
 * @param offset Where to start reading, given in bytes.
 * @param length How much to read, given in bytes.
 */
UMP_API_EXPORT void ump_read(void * dst, ump_handle src, unsigned long offset, unsigned long length);


/**
 * Write to specified UMP memory.
 *
 * Another way of writing to (and reading from) UMP memory is to use the
 * @ref ump_mapped_pointer_get "ump_mapped_pointer_get" to retrieve
 * a CPU mapped pointer to the memory.
 *
 * UMP API: v1
 * @see ump_mapped_pointer_get
 *
 * @param dst Handle to UMP memory to write to.
 * @param offset Where to start writing, given in bytes.
 * @param src Buffer to read from.
 * @param length How much to write, given in bytes.
 */
UMP_API_EXPORT void ump_write(ump_handle dst, unsigned long offset, const void * src, unsigned long length);

#endif /* UMP_BLOCK_V1_API */

/* @} */

#ifdef __cplusplus
}
#endif


/** @} */ /* end group ump_user_space_api */

/** @} */ /* end group ump_api */

#endif /* _UMP_H_ */
