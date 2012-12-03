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
///////////////////////////////////////////////////////////////////////////////
//
// Filename: Object.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2000, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Object.h,v 1.6 2005/09/15 12:39:22 adavid Exp $
//
///////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_OBJECT_H
#define INCLUDE_BASE_OBJECT_H

#include <stdlib.h>
#include <assert.h>
#include <iostream>

namespace base
{
    /** Base class to derive from if you want
     * to use Pointer<of something>. The reference
     * counting is done by Object.
     */
    class Object 
    {
    public:
        /** Constructor: initialize the reference counter.
         */
        Object() : refCounter(0) {}

        /** Destructor.
         */
        virtual ~Object() {}
        
        /** Add a reference to this object.
         * Use carefully because this will
         * affect deallocation.
         */
        void addReference()
        {
            ++refCounter;
        }

        /** Remove a reference to this object.
         * Use carefully because this will
         * affect deallocation.
         */
        void dropReference()
        {
            assert(refCounter > 0);
            if (--refCounter == 0) destroy();
        }

        /** This is useful to know if this object
         * may be modified safely.
         * @return true if there is only one reference
         * to this object.
         */
        bool isMutable() const
        {
            assert(refCounter > 0);
            return refCounter == 1;
        }

    protected:

        /** Wrapper to delete. It is virtual in
         * case we have objects that are not deallocated
         * by delete, in which case, the method is
         * overriden.
         */
        virtual void destroy()
        {
            assert(refCounter == 0);
            delete this;
        }
        
    private:
        unsigned int refCounter; /**< reference counter */
    };


    /** Pointer<of something> allows automatic
     * reference counting and automatic deallocation
     * when the reference drops to 0.
     * To use it, the referenced object must be
     * an Object.
     */
    template <class O>
    class Pointer 
    {
    public:

        /** Default constructor:
         * pointer to NULL
         */
        Pointer() : object(NULL) {}

        /** Copy constructor:
         * add reference
         * @param ptr: pointer to copy.
         */
        Pointer(const Pointer<O> &ptr)
            : object(ptr.object)
        {
            if (object) object->addReference();
        }

        /** Pointer constructor:
         * add reference
         * @param obj: pointed object
         */
        Pointer(O *obj)
            : object(obj)
        {
            if (object) object->addReference();
        }

        /** Destructor:
         * drop a reference
         */
        ~Pointer()
        {
            if (object) object->dropReference();
        }

        /** Dereference pointed object
         */
        O &operator *()  const { return *object; }

        /** Different wrappers to pointed object,
         * WITHOUT reference counting.
         */
        O *operator ->() const { return object; }
        operator O *()   const { return object; }
        O * getPtr()     const { return object; }

        /** @return true if pointer == NULL
         */
        bool null() const { return object == NULL; }

        /** @return true if pointer == NULL
         * This is a convenience operator to get
         * tests of the form if (!ptr) doThis();
         */
        bool operator !() const { return object == NULL; }
        
        /** Copy operator:
         * - add reference to new object
         * - drop reference to current object
         * - set new pointed object
         * Note: add 1st and then drop in case
         * it is an assignment a = a;
         */
        Pointer<O> &operator = (const Pointer<O> &ptr)
        {
            if (ptr.object) ptr.object->addReference();
            if (object) object->dropReference();
            object = ptr.object;
            return *this;
        }

        /** Cast operator. class To is the cast type.
         */
        template <class To>
        operator Pointer<To>() const
        {
            return Pointer<To>(static_cast<To*>(object));
        }

        /** Make sure this pointer is mutable.
         * The object class has to implement copy().
         */
        void setMutable()
        {
            if (object && !object->isMutable())
            {
                object->dropReference();
                object = object->copy();
                object->addReference();
            }
        }

        /** Make sure the object is big enough.
         * The object type must have:
         * size_t size() const;      -> gives its current size.
         * static O* create(size_t); -> create a new object with given size.
         * O* copy(size_t) const;    -> create & copy (to smaller or larger).
         */
        void ensure(size_t newSize)
        {
            if (object)
            {
                size_t size = object->size();
                if (size < newSize)
                {
                    size <<= 1;
                    O* larger = object->copy(size < newSize ? newSize : size);
                    object->dropReference();
                    object = larger;
                    object->addReference();
                }
            }
            else
            {
                object = O::create(newSize);
                object->addReference();
            }
        }
            
    protected:
        ///< This is for child classes with special needs.
        void setPtr(O* obj) { object = obj; }

    private:
        O *object; /**< pointed object, must be an Object */
    };

    /** Simple overload of operator <<
     */
    template <class O> static inline
    std::ostream &operator << (std::ostream &out, const Pointer<O> &p)
    {
        return (p.null()) ? (out << "(NULL)") : (out << (*p));
    }

} // namespace base

#endif // INCLUDE_BASE_OBJECT_H
