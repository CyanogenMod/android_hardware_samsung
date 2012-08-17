/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
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

#include <ump/ump.h>
#include <ump/ump_ref_drv.h>
#include <ColorFormat.h>

#define GRALLOC_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

#if GRALLOC_SIMULATE_FAILURES
#include <cutils/properties.h>

/* system property keys for controlling simulated UMP allocation failures */
#define PROP_MALI_TEST_GRALLOC_FAIL_FIRST     "mali.test.gralloc.fail_first"
#define PROP_MALI_TEST_GRALLOC_FAIL_INTERVAL  "mali.test.gralloc.fail_interval"

static int __ump_alloc_should_fail()
{

	static unsigned int call_count  = 0;
	unsigned int        first_fail  = 0;
	int                 fail_period = 0;
	int                 fail        = 0;
	
	++call_count;

	/* read the system properties that control failure simulation */	
	{
		char prop_value[PROPERTY_VALUE_MAX];
		
		if (property_get(PROP_MALI_TEST_GRALLOC_FAIL_FIRST, prop_value, "0") > 0)
		{
			sscanf(prop_value, "%u", &first_fail);
		}

		if (property_get(PROP_MALI_TEST_GRALLOC_FAIL_INTERVAL, prop_value, "0") > 0)
		{
			sscanf(prop_value, "%u", &fail_period);
		}
	}

	/* failure simulation is enabled by setting the first_fail property to non-zero */
	if (first_fail > 0)
	{
		ALOGI("iteration %u (fail=%u, period=%u)\n", call_count, first_fail, fail_period);
		
		fail = 	(call_count == first_fail) ||
				(call_count > first_fail && fail_period > 0 && 0 == (call_count - first_fail) % fail_period);
		
		if (fail) 
		{
			ALOGE("failed ump_ref_drv_allocate on iteration #%d\n", call_count);
		}
	}
	return fail;
}
#endif


static int gralloc_alloc_buffer(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle)
{
	ump_handle ump_mem_handle;
	void *cpu_ptr;
	ump_secure_id ump_id;
	ump_alloc_constraints constraints;

	size = round_up_to_page_size(size);

	if( (usage&GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN )
	{
		constraints =  UMP_REF_DRV_CONSTRAINT_USE_CACHE;
	}
	else
	{
		constraints = UMP_REF_DRV_CONSTRAINT_NONE;
	}

#ifdef GRALLOC_SIMULATE_FAILURES
	/* if the failure condition matches, fail this iteration */
	if (__ump_alloc_should_fail())
	{
		ump_mem_handle = UMP_INVALID_MEMORY_HANDLE;
	}
	else
#endif
	ump_mem_handle = ump_ref_drv_allocate(size, constraints);
	if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle)
	{
		cpu_ptr = ump_mapped_pointer_get(ump_mem_handle);
		if (NULL != cpu_ptr)
		{
			ump_id = ump_secure_id_get(ump_mem_handle);
			if (UMP_INVALID_SECURE_ID != ump_id)
			{
				private_handle_t* hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_UMP, size, (int)cpu_ptr,
				                                             private_handle_t::LOCK_STATE_MAPPED, ump_id, ump_mem_handle);
				if (NULL != hnd)
				{
					*pHandle = hnd;
					return 0;
				}
				else
				{
					ALOGE("gralloc_alloc_buffer() failed to allocate handle");
				}
			}
			else
			{
				ALOGE("gralloc_alloc_buffer() failed to retrieve valid secure id");
			}
			
			ump_mapped_pointer_release(ump_mem_handle);
		}
		else
		{
			ALOGE("gralloc_alloc_buffer() failed to map UMP memory");
		}

		ump_reference_release(ump_mem_handle);
	}
	else
	{
		ALOGE("gralloc_alloc_buffer() failed to allocate UMP memory");
	}

	return -1;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle)
{
	private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    
	// allocate the framebuffer
	if (m->framebuffer == NULL)
	{
		// initialize the framebuffer, the framebuffer is mapped once and forever.
		int err = init_frame_buffer_locked(m);
		if (err < 0)
		{
			return err;
		}
	}

	const uint32_t bufferMask = m->bufferMask;
	const uint32_t numBuffers = m->numBuffers;
	const size_t bufferSize = m->finfo.line_length * m->info.yres;
	if (numBuffers == 1)
	{
		// If we have only one buffer, we never use page-flipping. Instead,
		// we return a regular buffer which will be memcpy'ed to the main
		// screen when post is called.
		int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;
		ALOGE("fallback to single buffering");
		return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle);
	}

	if (bufferMask >= ((1LU<<numBuffers)-1))
	{
		// We ran out of buffers.
		return -ENOMEM;
	}

	int vaddr = m->framebuffer->base;
	// find a free slot
	for (uint32_t i=0 ; i<numBuffers ; i++)
	{
		if ((bufferMask & (1LU<<i)) == 0)
		{
			m->bufferMask |= (1LU<<i);
			break;
		}
		vaddr += bufferSize;
	}

	// The entire framebuffer memory is already mapped, now create a buffer object for parts of this memory
	private_handle_t* hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER, size, vaddr,
	                                             0, dup(m->framebuffer->fd), vaddr - m->framebuffer->base);
	*pHandle = hnd;

	return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev, size_t size, int usage, buffer_handle_t* pHandle)
{
	private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
	pthread_mutex_lock(&m->lock);
	int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle);
	pthread_mutex_unlock(&m->lock);
	return err;
}

static int alloc_device_alloc(alloc_device_t* dev, int w, int h, int format, int usage, buffer_handle_t* pHandle, int* pStride)
{
	if (!pHandle || !pStride)
	{
		return -EINVAL;
	}

    size_t size = 0;
    size_t stride = 0;
    size_t stride_raw = 0;

    if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP ||
        format == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_422_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_420_P  ||
        format == HAL_PIXEL_FORMAT_YV12 ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_LR ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_RL ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_LR ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_RL) {
        /* FIXME: there is no way to return the vstride */
        int vstride;
        stride = (w + 15) & ~15;
        vstride = (h + 15) & ~15;
        switch (format) {
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_LR:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_SBS_RL:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_LR:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_P_TB_RL:
            size = stride * vstride * 2;
            if(usage & GRALLOC_USAGE_HW_FIMC1)
                size += PAGE_SIZE * 2;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            size = (stride * vstride) + (w/2 * h/2) * 2;
            break;
        default:
            return -EINVAL;
        }
    } else {
        int align = 8;
        int bpp = 0;
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
        default:
            return -EINVAL;
        }
        size_t bpr = (w*bpp + (align-1)) & ~(align-1);
        size = bpr * h;
        stride = bpr / bpp;
        stride_raw = bpr;
    }

	int err;
	if (usage & GRALLOC_USAGE_HW_FB)
	{
		err = gralloc_alloc_framebuffer(dev, size, usage, pHandle);
	}
	else
	{
		err = gralloc_alloc_buffer(dev, size, usage, pHandle);
	}

	if (err < 0)
	{
		return err;
	}

	*pStride = stride;
	return 0;
}

static int alloc_device_free(alloc_device_t* dev, buffer_handle_t handle)
{
	if (private_handle_t::validate(handle) < 0)
	{
		return -EINVAL;
	}

	private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
	if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
	{
		// free this buffer
		private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
		const size_t bufferSize = m->finfo.line_length * m->info.yres;
		int index = (hnd->base - m->framebuffer->base) / bufferSize;
		m->bufferMask &= ~(1<<index); 
		close(hnd->fd);
	}
	else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP)
	{
		ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
		ump_reference_release((ump_handle)hnd->ump_mem_handle);
	}

	delete hnd;

	return 0;
}

static int alloc_device_close(struct hw_device_t *device)
{
	alloc_device_t* dev = reinterpret_cast<alloc_device_t*>(device);
	if (dev)
	{
		delete dev;
		ump_close(); // Our UMP memory refs will be released automatically here...
	}
	return 0;
}

int alloc_device_open(hw_module_t const* module, const char* name, hw_device_t** device)
{
	alloc_device_t *dev;
	
	dev = new alloc_device_t;
	if (NULL == dev)
	{
		return -1;
	}

	ump_result ump_res = ump_open();
	if (UMP_OK != ump_res)
	{
		ALOGE("UMP open failed");
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
