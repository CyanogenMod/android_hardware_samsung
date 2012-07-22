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

#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "alloc_device.h"
#include "gralloc_priv.h"
#include "gralloc_helper.h"
#include "framebuffer_device.h"

#include "ump.h"

#include "sec_format.h"

#include <fcntl.h>

#ifdef ION_ALLOC
#include "ion.h"
#endif

#include <linux/ashmem.h>
#include <cutils/ashmem.h>

/* Treat GPU as UMP Device Z */
#define GRALLOC_DEVICE_SHIFT UMP_DEVICE_Z_SHIFT

#define EXYNOS_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
static int gralloc_alloc_buffer(alloc_device_t* dev, size_t size, int usage,
                                buffer_handle_t* pHandle, int w, int h,
                                int format, int bpp, int stride_raw, int stride)
{
    ion_buffer ion_fd = 0;
    ump_handle ump_mem_handle;
    void *cpu_ptr   = NULL;
    ump_secure_id ump_id=UMP_INVALID_SECURE_ID;
    ump_alloc_flags ump_flags = UMP_PROT_SHAREABLE | UMP_HINT_CPU_RD | UMP_HINT_CPU_WR;
    int sw_read_usage;
    int sw_write_usage;
    int hw_usage;
    size = round_up_to_page_size(size);

    unsigned int ion_flags = 0;
    int priv_alloc_flag = private_handle_t::PRIV_FLAGS_USES_UMP;

    if (usage & GRALLOC_USAGE_HW_ION) {
        private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
        ion_fd = ion_alloc(m->ion_client, size, 0, ION_HEAP_EXYNOS_MASK);
        if (ion_fd < 0) {
            ALOGE("Failed to ion_alloc");
            return -1;
        }

        cpu_ptr = ion_map(ion_fd, size, 0);
        if (NULL == cpu_ptr) {
            ALOGE("Failed to ion_map");
            ion_free(ion_fd);
            return -1;
        }
        /* TODO: GPU does not use ION. */
        ump_mem_handle = ump_import(UMP_EXTERNAL_MEM_TYPE_ION, &ion_fd, ump_flags);
        if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle) {
            priv_alloc_flag = private_handle_t::PRIV_FLAGS_USES_ION;
        } else {
            ALOGE("gralloc_alloc_buffer() failed to import ION memory");
            ion_unmap(cpu_ptr, size);
            ion_free(ion_fd);
            return -1;
        }
    } else {
        usage |= GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
        sw_read_usage = usage & GRALLOC_USAGE_SW_READ_MASK;
        sw_write_usage = usage & GRALLOC_USAGE_SW_WRITE_MASK;
        hw_usage = usage & GRALLOC_USAGE_HW_MASK;

        if (sw_read_usage) {
            ump_flags |= UMP_PROT_CPU_RD;
            if (sw_read_usage == GRALLOC_USAGE_SW_READ_OFTEN)
                ump_flags |= UMP_HINT_CPU_RD;
        }
        if (sw_write_usage) {
            ump_flags |= UMP_PROT_CPU_WR;
            if (sw_write_usage == GRALLOC_USAGE_SW_WRITE_OFTEN)
                ump_flags |= UMP_HINT_CPU_WR;
        }

        if (hw_usage) {
            int hw_flags = 0;
            if (hw_usage & (GRALLOC_USAGE_HW_TEXTURE))
                hw_flags |= (UMP_PROT_DEVICE_RD | UMP_HINT_DEVICE_RD);

            if (hw_usage & (GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_FB))
                hw_flags |= (UMP_PROT_DEVICE_RD | UMP_HINT_DEVICE_RD | UMP_PROT_DEVICE_WR | UMP_HINT_DEVICE_WR);

            ump_flags |= (hw_flags << GRALLOC_DEVICE_SHIFT);
        }

        ump_mem_handle = ump_allocate_64(size, ump_flags);
        if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle)
            cpu_ptr = ump_map(ump_mem_handle, 0, size);
        else
            ALOGE("gralloc_alloc_buffer() failed to allcoate UMP memory");
    }

    if (NULL != cpu_ptr) {
        ump_id = ump_secure_id_get(ump_mem_handle);

        if (UMP_INVALID_SECURE_ID != ump_id || usage & GRALLOC_USAGE_HW_ION) {
            private_handle_t* hnd = new private_handle_t(priv_alloc_flag, size, (int)cpu_ptr,
                                                        private_handle_t::LOCK_STATE_MAPPED,
                                                        ump_id,
                                                        ump_mem_handle);
            if (NULL != hnd) {
                *pHandle = hnd;
                hnd->fd = ion_fd;
                hnd->format = format;
                hnd->usage = usage;
                hnd->width = w;
                hnd->height = h;
                hnd->bpp = bpp;
                hnd->stride = stride;
                hnd->uoffset = ((EXYNOS_ALIGN(hnd->width, 16) * EXYNOS_ALIGN(hnd->height, 16)));
                hnd->voffset = ((EXYNOS_ALIGN((hnd->width >> 1), 8) * EXYNOS_ALIGN((hnd->height >> 1), 8)));
                return 0;
            } else {
                ALOGE("gralloc_alloc_buffer() failed to allocate handle");
            }
        } else {
            ALOGE("gralloc_alloc_buffer() failed to retrieve valid secure id");
        }

        ump_unmap(ump_mem_handle, cpu_ptr, size);
    } else {
        ALOGE("gralloc_alloc_buffer() failed to map UMP memory");
    }

    ump_release(ump_mem_handle);

    return -1;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev, size_t size, int usage,
                                            buffer_handle_t* pHandle, int w, int h,
                                            int format, int bpp)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    /* allocate the framebuffer */
    if (m->framebuffer == NULL) {
        /* initialize the framebuffer, the framebuffer is mapped once and forever. */
        int err = init_frame_buffer_locked(m);

        if (err < 0)
            return err;
    }

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    const size_t bufferSize = m->finfo.line_length * m->info.yres;

    if (numBuffers == 1) {
        /*
         * If we have only one buffer, we never use page-flipping. Instead,
         * we return a regular buffer which will be memcpy'ed to the main
         * screen when post is called.
         */
        int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;

        ALOGD("fallback to single buffering");

        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle, w, h, format, bpp, 0, 0);
    }

    if (bufferMask >= ((1LU<<numBuffers)-1)) {
        ALOGE("Ran out of buffers");
        return -ENOMEM;
    }

    int vaddr = m->framebuffer->base;
    /* find a free slot */
    for (uint32_t i = 0; i < numBuffers; i++) {
        if ((bufferMask & (1LU<<i)) == 0) {
            m->bufferMask |= (1LU<<i);
            break;
        }
        vaddr += bufferSize;
    }

    /* The entire framebuffer memory is already mapped, now create a buffer object for parts of this memory */
    private_handle_t* hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER, size, vaddr,
                                                 0, dup(m->framebuffer->fd), vaddr - m->framebuffer->base);
    hnd->format = format;
    hnd->usage = usage;
    hnd->width = w;
    hnd->height = h;
    hnd->bpp = bpp;
    *pHandle = hnd;

    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev, size_t size, int usage,
                                     buffer_handle_t* pHandle, int w, int h,
                                     int format, int bpp)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle, w, h, format, bpp);
    pthread_mutex_unlock(&m->lock);

    return err;
}

static int alloc_device_alloc(alloc_device_t* dev, int w, int h, int format,
                              int usage, buffer_handle_t* pHandle, int* pStride)
{
    if (!pHandle || !pStride) {
        ALOGE("Invalid Handle, Stride value");
        return -EINVAL;
    }

    int err;
    int align = 16;
    int bpp = 0;
    size_t stride_raw = 0;
    size_t size = 0;
    size_t stride = (w + 15) & ~15;
    size_t vstride = (h + 15) & ~15;

    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_RGB_888:
        bpp = 3;
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
        bpp = 2;
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YV12:
        size = stride * vstride * 2;
        bpp = 4;
        break;
    default:
        ALOGE("Not Support Pixel Format");
        return -EINVAL;
    }

    if (format != HAL_PIXEL_FORMAT_YCbCr_420_P &&
        format != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
        format != HAL_PIXEL_FORMAT_YV12) {
        size_t bpr = (w * bpp + (align-1)) & ~(align-1);
        size = bpr * h;
        stride = bpr / bpp;
        stride_raw = bpr;
    }

    if (usage & GRALLOC_USAGE_HW_FB)
        err = gralloc_alloc_framebuffer(dev, size, usage, pHandle, w, h, format, 32);
    else
        err = gralloc_alloc_buffer(dev, size, usage, pHandle, w, h,
                    format, 0, (int)stride_raw, (int)stride);

    if (err < 0) {
        ALOGE("Fail to alloc Gralloc memory");
        return err;
    }

    *pStride = stride;
    return 0;
}

static int alloc_device_free(alloc_device_t* dev, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Invalid Value : private_handle");
        return -EINVAL;
    }

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        /* free this buffer */
        private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;
        m->bufferMask &= ~(1<<index);
        close(hnd->fd);
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        ump_unmap(hnd->ump_mem_handle, (void*)hnd->base, hnd->size);
        ump_release(hnd->ump_mem_handle);
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
        ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
        ump_reference_release((ump_handle)hnd->ump_mem_handle);

        ion_unmap((void*)hnd->base, hnd->size);
        ion_free(hnd->fd);
    }

    delete hnd;

    return 0;
}

static int alloc_device_close(struct hw_device_t *device)
{
    alloc_device_t* dev = reinterpret_cast<alloc_device_t*>(device);
    if (dev) {
        private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
        ion_client_destroy(m->ion_client);
        delete dev;
        ump_close(); /* Our UMP memory refs will be released automatically here... */
    }
    return 0;
}

int alloc_device_open(hw_module_t const* module, const char* name, hw_device_t** device)
{
    alloc_device_t *dev;

    dev = new alloc_device_t;
    if (NULL == dev) {
        ALOGE("Fail to create alloc_device");
        return -1;
    }

    dev->common.module = const_cast<hw_module_t*>(module);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    m->ion_client=ion_client_create();
    ALOGI("gralloc create ion_client %d", m->ion_client);

    ump_result ump_res = ump_open();

    if ((UMP_OK != ump_res) || (0 > m->ion_client)) {
        ALOGE("UMP open failed, ump_res %d, ion_client %d", ump_res, m->ion_client);
        delete dev;
        return -1;
    }

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = const_cast<hw_module_t*>(module);
    dev->common.close = alloc_device_close;
    dev->alloc = alloc_device_alloc;
    dev->free = alloc_device_free;

    *device = &dev->common;

    return 0;
}
