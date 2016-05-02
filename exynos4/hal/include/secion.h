/*
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
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

#ifndef _LIB_SECION_H_
#define _LIB_SECION_H_

#include <unistd.h>

typedef unsigned long ion_phys_addr_t;
typedef int ion_client;
typedef int ion_buffer;

enum ION_MSYNC_FLAGS {
    IMSYNC_DEV_TO_READ = 0,
    IMSYNC_DEV_TO_WRITE = 1,
    IMSYNC_DEV_TO_RW = 2,
    IMSYNC_SYNC_FOR_DEV = 0x10000,
    IMSYNC_SYNC_FOR_CPU = 0x20000,
};

struct secion_param {
    ion_client client;
    ion_buffer buffer;
    size_t size;
    void *memory;
    ion_phys_addr_t physaddr;
};

#ifdef __cplusplus
extern "C" {
#endif

ion_client ion_client_create(void);
void ion_client_destroy(ion_client client);
ion_buffer ion_alloc(ion_client client, size_t len, size_t align, unsigned int flags);
void ion_free(ion_buffer buffer);
void *ion_map(ion_buffer buffer, size_t len, off_t offset);
int ion_unmap(void *addr, size_t len);
int ion_msync(ion_client client, ion_buffer buffer, enum ION_MSYNC_FLAGS flags, size_t size, off_t offset);
ion_phys_addr_t ion_getphys(ion_client client, ion_buffer buffer);
int createIONMem(struct secion_param *param, size_t size, unsigned int flags);
int destroyIONMem(struct secion_param *param);

#ifdef __cplusplus
}
#endif
#endif /* _LIB_SECION_H_ */
