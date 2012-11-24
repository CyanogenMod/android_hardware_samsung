/*
 * Copyright@ Samsung Electronics Co. LTD
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

/*!
 * \file      ExynosBuffer.h
 * \brief     header file for ExynosBuffer
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2011/06/02
 *
 * <b>Revision History: </b>
 * - 2010/06/03 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 *
 * - 2012/03/14 : sangwoo.park(sw5771.park@samsung.com) \n
 *   Change file, struct name to ExynosXXX.
 *
 * - 2012/10/08 : sangwoo.park(sw5771.park@samsung.com) \n
 *   Add BUFFER_PLANE_NUM_DEFAULT, and, Increase Buffer as 4.
 *
 */

#ifndef EXYNOS_BUFFER_H_
#define EXYNOS_BUFFER_H_

#include <sys/types.h>

//! Buffer information
/*!
 * \ingroup Exynos
 */
struct ExynosBuffer
{
public:
    //! Buffer type
    enum BUFFER_TYPE
    {
        BUFFER_TYPE_BASE     = 0,
        BUFFER_TYPE_VIRT     = 1 << 0, //!< virtual address
        BUFFER_TYPE_PHYS     = 1 << 1, //!< physical address
        BUFFER_TYPE_ION      = 1 << 2, //!< ion address
        BUFFER_TYPE_RESERVED = 1 << 3, //!< reserved type
        BUFFER_TYPE_MAX,
    };

    //! Buffer plane number
    enum BUFFER_PLANE_NUM
    {
        BUFFER_PLANE_NUM_DEFAULT = 4,
    };

    //! Buffer virtual address
    union {
        char *p;       //! single address.
        char *extP[BUFFER_PLANE_NUM_DEFAULT]; //! Y Cb Cr.
    } virt;

    //! Buffer physical address
    union {
        unsigned int p;       //! single address.
        unsigned int extP[BUFFER_PLANE_NUM_DEFAULT]; //! Y Cb Cr.
    } phys;

    union {
        unsigned int p;       //! single address.
        unsigned int extP[BUFFER_PLANE_NUM_DEFAULT]; //! Y Cb Cr.
    } ion;

    //! Buffer reserved id
    union {
        unsigned int p;       //! \n
        unsigned int extP[BUFFER_PLANE_NUM_DEFAULT]; //! \n
    } reserved;

    //! Buffer size
    union {
        unsigned int s;
        unsigned int extS[BUFFER_PLANE_NUM_DEFAULT];
    } size;

#ifdef __cplusplus
    //! Constructor
    ExynosBuffer()
    {
        for (int i = 0; i < BUFFER_PLANE_NUM_DEFAULT; i++) {
            virt.    extP[i] = NULL;
            phys.    extP[i] = 0;
            ion.     extP[i] = 0;
            reserved.extP[i] = 0;
            size.    extS[i] = 0;
        }
    }

    //! Constructor
    ExynosBuffer(const ExynosBuffer *other)
    {
        for (int i = 0; i < BUFFER_PLANE_NUM_DEFAULT; i++) {
            virt.    extP[i] = other->virt.extP[i];
            phys.    extP[i] = other->phys.extP[i];
            ion.     extP[i] = other->ion.extP[i];
            reserved.extP[i] = other->reserved.extP[i];
            size.    extS[i] = other->size.extS[i];
        }
    }

    //! Operator(=) override
    ExynosBuffer& operator =(const ExynosBuffer &other)
    {
        for (int i = 0; i < BUFFER_PLANE_NUM_DEFAULT; i++) {
            virt.    extP[i] = other.virt.extP[i];
            phys.    extP[i] = other.phys.extP[i];
            ion.     extP[i] = other.ion.extP[i];
            reserved.extP[i] = other.reserved.extP[i];
            size.    extS[i] = other.size.extS[i];
        }
        return *this;
    }

    //! Operator(==) override
    bool operator ==(const ExynosBuffer &other) const
    {
        bool ret = true;

        for (int i = 0; i < BUFFER_PLANE_NUM_DEFAULT; i++) {
            if (   virt.    extP[i] != other.virt.    extP[i]
                || phys.    extP[i] != other.phys.    extP[i]
                || ion.     extP[i] != other.ion.     extP[i]
                || reserved.extP[i] != other.reserved.extP[i]
                || size.    extS[i] != other.size.    extS[i]) {
                ret = false;
                break;
            }
        }

        return ret;
    }

    //! Operator(!=) override
    bool operator !=(const ExynosBuffer &other) const
    {
        // use operator(==)
        return !(*this == other);
    }

    //! Get Buffer type
    static int BUFFER_TYPE(ExynosBuffer *buf)
    {
        int type = BUFFER_TYPE_BASE;
        if (buf->virt.p)
            type |= BUFFER_TYPE_VIRT;
        if (buf->phys.p)
            type |= BUFFER_TYPE_PHYS;
        if (buf->ion.p)
            type |= BUFFER_TYPE_ION;
        if (buf->reserved.p)
            type |= BUFFER_TYPE_RESERVED;

        return type;
    }
#endif
};

#endif //EXYNOS_BUFFER_H_
