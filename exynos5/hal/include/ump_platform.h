/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file ump_platform.h
 *
 * This file should define UMP_API_EXPORT,
 * which dictates how the UMP user space API should be exported/imported.
 * Modify this file, if needed, to match your platform setup.
 */


#ifndef _UMP_PLATFORM_H_
#define _UMP_PLATFORM_H_

#include "malisw/mali_stdtypes.h"

/** @addtogroup ump_user_space_api
 * @{ */

/**
 * A define which controls how UMP user space API functions are imported and exported.
 *
 * Functions exported by the driver is tagged with UMP_API_EXPORT to allow
 * the compiler/build system/OS loader to detect and handle functions which is to be exported/imported from a shared library. @n
 * This define should be set by the implementor of the UMP API to match their needs if needed.
 *
 * Typical usage example in the driver:
 *
 * UMP_API_EXPORT void my_api_call(void);
 */

#if defined(_WIN32)

#define UMP_API_EXPORT

#elif defined(__SYMBIAN32__)

#define UMP_API_EXPORT IMPORT_C

#else

#define UMP_API_EXPORT

#endif

/** @} */ /* end group ump_user_space_api */


#endif /* _UMP_PLATFORM_H_ */
