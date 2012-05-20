/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Portions of this code have been modified from the original.
 * These modifications are:
 *    * includes
 *    * round_up_to_page_size()
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

#ifndef GRALLOC_HELPER_H_
#define GRALLOC_HELPER_H_

#include <sys/mman.h>

inline size_t round_up_to_page_size(size_t x)
{
    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}

#endif /* GRALLOC_HELPER_H_ */
