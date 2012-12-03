/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
 *
 * This file is part of the UPPAAL DBM library.
 *
 * The UPPAAL DBM library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * The UPPAAL DBM library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the UPPAAL DBM library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// Filename : bitptr.h (base)
//
// Provides: bitptr_u bitptr_t
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: bitptr.h,v 1.3 2004/06/14 07:36:53 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_BITPTR_H
#define INCLUDE_BASE_BITPTR_H

#include <assert.h>
#include <stdlib.h>
#include "base/inttypes.h"

namespace base
{
    /** Cast in a nutshell
     * Conversion int <-> ptr with a union.
     * Use of (u)intptr_t to avoid possible
     * endianness problems when manipulating
     * the lower bits.
     */
    typedef union
    {
        intptr_t ival;
        uintptr_t uval;
        void *ptr;
    } bitptr_u;

    /** Encapsulation of lower bit manipulation of a pointer.
     * bitptr_t protects operation on the 2 lower bits
     * of a pointer. These bits may contain data.
     * Valid operation is done only on those 2 bits.
     * Name IntPtr chosen because it is a hybrid type
     * both an int and a pointer.
     * Name in lower case because it is a wrapper around
     * a pointer/int basic scalar type.
     */
    template<class PtrType>
    class bitptr_t
    {
    public:
        
        /** Default constructor
         */
        bitptr_t()
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            value.ptr = NULL;
        }
        
        /** Pointer constructor
         * @param ptr: pointer to represent.
         * @pre ptr is 32 bit aligned.
         */
        explicit bitptr_t(PtrType *ptr)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            value.ptr = ptr;
            assert(getBits(3) == 0);
        }

        /** Bits constructor
         * @param mask: bits of the int part.
         * @pre mask <= 3.
         */
        explicit bitptr_t(uintptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.uval = mask;
        }
        
        /** Bits constructor, only a wrapper
         * for convenience.
         * @param mask: bits of the int part.
         * @pre mask <= 3 and mask >= 0
         */
        explicit bitptr_t(intptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ival = mask;
        }

#ifdef ENABLE_64BIT
        explicit bitptr_t(uint32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.uval = mask;
        }
        explicit bitptr_t(int32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ival = mask;
        }
#endif
        
        /** Pointer and bits constructor.
         * @param ptr: pointer to represent.
         * @param mask: bits of the int part.
         * @pre
         * - ptr is 32 bits aligned.
         * - mask <= 3
         */
        bitptr_t(PtrType *ptr, uintptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.ptr = ptr;
            value.uval |= mask;
        }
        
        /** Pointer and bits constructor, wrapper
         * for convenience only.
         * @param ptr: pointer to represent.
         * @param mask: bits of the int part.
         * @pre
         * - ptr is 32 bits aligned.
         * - mask <= 3 and mask >= 0
         */
        bitptr_t(PtrType *ptr, intptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ptr = ptr;
            value.ival |= mask;
        }
        
        /** IntPtr and new bits for int part.
         * Takes pointer from ptr and bits from mask
         * @param ptr: pointer part to take.
         * @param mask: bits of the int part.
         * @pre mask <= 3
         */
        bitptr_t(bitptr_t<PtrType> ptr, uintptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.uval = (ptr.value.uval & ~3) | mask;
        }
        
        /** IntPtr and new bits for int part, wrapper
         * for convenience only. 
         * Takes pointer from ptr and bits from mask
         * @param ptr: pointer part to take.
         * @param mask: bits of the int part.
         * @pre mask <= 3 and mask >= 0
         */
        bitptr_t(bitptr_t<PtrType> ptr, intptr_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ival = (ptr.value.ival & ~3) | mask;
        }

#ifdef ENABLE_64BIT
        bitptr_t(PtrType *ptr, uint32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.ptr = ptr;
            value.uval |= mask;
        }
        bitptr_t(PtrType *ptr, int32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ptr = ptr;
            value.ival |= mask;
        }
        bitptr_t(bitptr_t<PtrType> ptr, uint32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3);
            value.uval = (ptr.value.uval & ~3) | mask;
        }
        bitptr_t(bitptr_t<PtrType> ptr, int32_t mask)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            assert(mask <= 3 && mask >= 0);
            value.ival = (ptr.value.ival & ~3) | mask;
        }
#endif
        
        /** IntPtr and new bits for int part.
         * Takes pointer from ptr and bits from mask
         * @param ptr: pointer part to take.
         * @param bits: bits of the int part.
         */
        bitptr_t(PtrType *ptr, bitptr_t bits)
        {
            assert(sizeof(uintptr_t) == sizeof(void*));
            value.ptr = ptr;
            value.uval |= bits.value.uval & 3;
        }
        
        /** Read the int bits only.
         * @return the int part of IntPtr.
         */
        uintptr_t getBits() const
        {
            return value.uval & 3;
        }
        
        /** Read some bits only from the int bits.
         * @param mask: mask to read the bits.
         * @pre mask <= 3
         */
        uintptr_t getBits(uintptr_t mask) const
        {
            assert(mask <= 3);
            return value.uval & mask;
        }
        
        /** Read some bits only from the int bits,
         * wrapper for convenience only.
         * @param mask: mask to read the bits.
         * @pre mask <= 3 and mask >= 0
         */    
        intptr_t getBits(intptr_t mask) const
        {
            assert(mask <= 3 && mask >= 0);
            return value.ival & mask;
        }
        
#ifdef ENABLE_64BIT
        uintptr_t getBits(uint32_t mask) const
        {
            assert(mask <= 3);
            return value.uval & mask;
        }
        intptr_t getBits(int32_t mask) const
        {
            assert(mask <= 3 && mask >= 0);
            return value.ival & mask;
        }
#endif

        /** Read the pointer part.
         * @return a clean pointer aligned on 32 bits.
         */
        PtrType* getPtr() const
        {
            bitptr_u result = value;
            result.uval &= ~3;
            return reinterpret_cast<PtrType*>(result.ptr);
        }
        
        /** Add bits and don't touche the
         * pointer part.
         * @param mask: bits to add.
         * @pre mask <= 3
         */
        void addBits(uintptr_t mask)
        {
            assert(mask <= 3);
            value.uval |= mask;
        }
        
        /** Add bits and don't touche the
         * pointer part, for convenience
         * only.
         * @param mask: bits to add.
         * @pre mask <= 3 and mask >= 0
         */
        void addBits(intptr_t mask)
        {
            assert(mask <= 3 && mask >= 0);
            value.ival |= mask;
        }
        
        /** Delete bits and don't touch
         * the pointer part.
         * @param mask: bits to remove.
         * @pre mask <= 3
         */
        void delBits(uintptr_t mask)
        {
            assert(mask <= 3);
            value.uval &= ~mask;
        }
        
        /** Delete bits and don't touch
         * the pointer part, for convenience
         * only.
         * @param mask: bits to remove.
         * @pre mask <= 3 and mask >= 0
         */
        void delBits(intptr_t mask)
        {
            assert(mask <= 3 && mask >= 0);
            value.ival &= ~mask;
        }

#ifdef ENABLE_64BIT
        void addBits(uint32_t mask)
        {
            assert(mask <= 3);
            value.uval |= mask;
        }
        void addBits(int32_t mask)
        {
            assert(mask <= 3 && mask >= 0);
            value.ival |= mask;
        }
        void delBits(uint32_t mask)
        {
            assert(mask <= 3);
            value.uval &= ~mask;
        }
        void delBits(int32_t mask)
        {
            assert(mask <= 3 && mask >= 0);
            value.ival &= ~mask;
        }
#endif

        /** Set the pointer part and keep
         * the int part.
         * @param ptr: new pointer value.
         * @pre ptr is 32 bits aligned.
         */
        void setPtr(PtrType *ptr)
        {
            bitptr_u uptr;
            uptr.ptr = ptr;
            value.uval = uptr.uval | (value.uval & 3);
        }

    private:
        bitptr_u value; /**< value & ~3 is the pointer
                           and value & 3 is the int part */
    };

} // namespace base

#endif // INCLUDE_BASE_BITPTR_H
