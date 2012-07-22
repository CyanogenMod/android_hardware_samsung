/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include "ump.h"

#include "gralloc_priv.h"
#include "alloc_device.h"
#include "framebuffer_device.h"
#include "ion.h"
#include <sys/mman.h>

static pthread_mutex_t s_map_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_ump_is_open = 0;

static int gralloc_map(gralloc_module_t const* module,
        buffer_handle_t handle, void** vaddr)
{
    private_handle_t* hnd = (private_handle_t*)handle;
    private_module_t* m = (private_module_t*)module;

    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)) {
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
            size_t size = hnd->size;
            hnd->ion_client = ion_client_create();
            void *mappedAddress = ion_map(hnd->fd, size, 0);

            if (mappedAddress == MAP_FAILED) {
                ALOGE("Could not ion_map %s fd(%d)", strerror(errno), hnd->fd);
                return -errno;
            }

            hnd->base = intptr_t(mappedAddress) + hnd->offset;
            ALOGD("gralloc_map() succeeded fd=%d, off=%d, size=%d, vaddr=%p",
                    hnd->fd, hnd->offset, hnd->size, mappedAddress);
        } else {
            ALOGE("In case of ION, It could not reach here!");
        }
    }
    *vaddr = (void*)hnd->base;
    return 0;
}

static int gralloc_unmap(gralloc_module_t const* module,
            buffer_handle_t handle)
{
    private_handle_t* hnd = (private_handle_t*)handle;
    private_module_t* m = (private_module_t*)module;

    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)) {
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
            void* base = (void*)hnd->base;
            size_t size = hnd->size;
            ALOGD("unmapping from %p, size=%d", base, size);

            if (ion_unmap(base, size) < 0)
                ALOGE("Could not ion_unmap %s", strerror(errno));

            ion_client_destroy(hnd->ion_client);
        }
    }
    hnd->base = 0;
    return 0;
}

static int gralloc_device_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    int status = -EINVAL;

    ALOGI("Opening ARM Gralloc device");

    if (!strcmp(name, GRALLOC_HARDWARE_GPU0))
        status = alloc_device_open(module, name, device);
    else if (!strcmp(name, GRALLOC_HARDWARE_FB0))
        status = framebuffer_device_open(module, name, device);

    return status;
}

static int gralloc_register_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
    int retval = -EINVAL;
    void *vaddr;
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Registering invalid buffer, returning error");
        return -EINVAL;
    }

    // if this handle was created in this process, then we keep it as is.
    private_handle_t* hnd = (private_handle_t*)handle;
    if (hnd->pid == getpid()) {
        ALOGE("Invalid Process ID from Private_Handle");
        return 0;
    }

    pthread_mutex_lock(&s_map_lock);
    if (!s_ump_is_open) {
        ump_result res = ump_open();
        if (res != UMP_OK) {
            pthread_mutex_unlock(&s_map_lock);
            ALOGE("Failed to open UMP library");
            return retval;
        }
        s_ump_is_open = 1;
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        hnd->ump_mem_handle = ump_from_secure_id(hnd->ump_id);
        if (UMP_INVALID_MEMORY_HANDLE != hnd->ump_mem_handle) {
            hnd->base = (int)ump_map(hnd->ump_mem_handle, 0, hnd->size);
            if (0 != hnd->base) {
                hnd->lockState = private_handle_t::LOCK_STATE_MAPPED;
                hnd->writeOwner = 0;
                hnd->lockState = 0;

                pthread_mutex_unlock(&s_map_lock);
                return 0;
            } else {
                ALOGE("Failed to map UMP handle");
            }

            ump_release(hnd->ump_mem_handle);
        } else {
            ALOGE("Failed to create UMP handle");
        }
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
        hnd->ump_mem_handle = ump_handle_create_from_secure_id(hnd->ump_id);
        retval = gralloc_map(module, handle, &vaddr);
    } else {
        ALOGE("registering non-UMP&ION buffer not supported");
    }

    pthread_mutex_unlock(&s_map_lock);

    return retval;
}

static int gralloc_unregister_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("unregistering invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;

    ALOGE_IF(hnd->lockState & private_handle_t::LOCK_STATE_READ_MASK,
                "[unregister] handle %p still locked (state=%08x)", hnd, hnd->lockState);

    /* never unmap buffers that were created in this process */
    if (hnd->pid != getpid()) {
        pthread_mutex_lock(&s_map_lock);

        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
            ump_unmap(hnd->ump_mem_handle, (void*)hnd->base, hnd->size);
            hnd->base = 0;
            ump_release(hnd->ump_mem_handle);
            hnd->ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {

            ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
            ump_reference_release((ump_handle)hnd->ump_mem_handle);
            if (hnd->base)
                gralloc_unmap(module, handle);

            hnd->base = 0;
            hnd->ump_mem_handle = (int)UMP_INVALID_MEMORY_HANDLE;
            hnd->lockState  = 0;
            hnd->writeOwner = 0;
        } else {
            ALOGE("unregistering non-UMP buffer not supported");
        }

        hnd->base = 0;
        hnd->lockState  = 0;
        hnd->writeOwner = 0;

        pthread_mutex_unlock(&s_map_lock);
    }

    return 0;
}

static int gralloc_lock(gralloc_module_t const* module, buffer_handle_t handle,
                        int usage, int l, int t, int w, int h, void** vaddr)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Locking invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        hnd->writeOwner = usage & GRALLOC_USAGE_SW_WRITE_MASK;

        if (usage & GRALLOC_USAGE_SW_READ_MASK)
            ump_cpu_msync_now(hnd->ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE,
                    (void*)hnd->base, hnd->size);
    }

    if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
        *vaddr = (void*)hnd->base;

    if (usage & GRALLOC_USAGE_YUV_ADDR) {
        vaddr[0] = (void*)hnd->base;
        vaddr[1] = (void*)(hnd->base + hnd->uoffset);
        vaddr[2] = (void*)(hnd->base + hnd->uoffset + hnd->voffset);
    }
    return 0;
}

static int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Unlocking invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;
    private_module_t* m = (private_module_t*)module;
    int32_t current_value;
    int32_t new_value;
    int retry;

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP && hnd->writeOwner)
        ump_cpu_msync_now(hnd->ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, (void*)hnd->base, hnd->size);
    else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
        ion_msync(hnd->ion_client, hnd->fd, IMSYNC_DEV_TO_READ | IMSYNC_SYNC_FOR_DEV, hnd->size, hnd->offset);

    return 0;
}

static struct hw_module_methods_t gralloc_module_methods =
{
    open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM =
{
    base:
    {
        common:
        {
            tag: HARDWARE_MODULE_TAG,
            version_major: 1,
            version_minor: 0,
            id: GRALLOC_HARDWARE_MODULE_ID,
            name: "Graphics Memory Allocator Module",
            author: "ARM Ltd.",
            methods: &gralloc_module_methods,
            dso: NULL,
            reserved : {0,},
        },
        registerBuffer: gralloc_register_buffer,
        unregisterBuffer: gralloc_unregister_buffer,
        lock: gralloc_lock,
        unlock: gralloc_unlock,
        perform: NULL,
        reserved_proc: {0,},
    },
    framebuffer: NULL,
    flags: 0,
    numBuffers: 0,
    bufferMask: 0,
    lock: PTHREAD_MUTEX_INITIALIZER,
    currentBuffer: NULL,
    ion_client: -1,
};
