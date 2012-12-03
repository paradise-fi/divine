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
// Filename : linkable.h (base)
//
// SingleLinkable_t
//  |
//  +-SingleLinkable<T>
//  |
//  +-DoubleLinkable_t
//     |
//     +-DoubleLinkable<T>
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: linkable.h,v 1.6 2004/04/19 14:47:40 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_LINKABLE_H
#define INCLUDE_BASE_LINKABLE_H

#include <assert.h>

/**
 * @file
 *
 * Defines base linkable structures for single and double linked lists.
 * struct is used (instead of class) for style reasons (though it's the
 * same conceptually): class refers to an object, struct refers to a
 * simple data structure made to be stored.
 * These structures are defined because it is far more readable to
 * have bla->link(somewhere) than the manual pointer link stuff every
 * time. This reduces errors. Furthermore the default lists of the
 * STL are double linked.
 */

namespace base
{
    /** Generic untyped structure
     * for generic manipulation.
     */
    struct SingleLinkable_t
    {
        SingleLinkable_t *next;
    };


    /** Generic untyped structure
     * for generic manipulation.
     */
    struct DoubleLinkable_t : public SingleLinkable_t
    {
        DoubleLinkable_t **previous;
    };


    /** Template for typed manipulation.
     */
    template<class T>
    struct SingleLinkable : public SingleLinkable_t
    {
        /** Link this linkable node from a given root.
         * @pre struct your_struct : publid SingleLinkable<your_struct>
         * @param fromWhere: from where the node
         * should be linked.
         */
        void link(T **fromWhere)
        {
            assert(fromWhere);

            next = *fromWhere;
            *fromWhere = static_cast<T*>(this);
        }
    
        /** Unlink this linkable node from a given root.
         * @pre struct your_struct : publid SingleLinkable<your_struct>
         * @param  fromWhere: from where the node
         * should be unlinked.
         */
        void unlink(T **fromWhere)
        {
            assert(fromWhere &&
                   *fromWhere == static_cast<T*>(this));

            *fromWhere = getNext();
        }

        /** Access to the (typed) next linkable element.
         * @return pointer to next element
         */
        T* getNext() const
        {
            return static_cast<T*>(next);
        }

        /** Access to the address of the next pointed element.
         * @return address of the pointer to the next element.
         */
        T** getAtNext()
        {
            return reinterpret_cast<T**>(&next);
        }
    };


    /** Template for typed manipulation.
     */
    template<class T>
    struct DoubleLinkable : public DoubleLinkable_t
    {
        /** Link this linkable node from a given root.
         * @pre struct your_struct : publid DoubleLinkable<your_struct>
         * @param fromWhere: from where the node
         * should be linked.
         */
        void link(T **fromWhere)
        {
            assert(fromWhere);

            previous = reinterpret_cast<DoubleLinkable_t**>(fromWhere);
            next = *fromWhere;
            if (next) getNext()->previous =
                          reinterpret_cast<DoubleLinkable_t**>(&next);
            *fromWhere = static_cast<T*>(this);
        }

        /** Unlink this linkable node from a
         * given root. We do not need the argument
         * for the root since it is a double linked
         * node.
         * @pre struct your_struct : publid DoubleLinkable<your_struct>
         */
        void unlink()
        {
            assert(previous &&
                   *previous == static_cast<T*>(this));

            *previous = getNext();
            if (next) getNext()->previous = previous;
        }

        /** Access to the (typed) next linkable element.
         * @return pointer to next element
         */
        T* getNext() const
        {
            return static_cast<T*>(next);
        }

        /** Access to the address of the next pointed element.
         * @return address of the pointer to the next element.
         */
        T** getAtNext()
        {
            return reinterpret_cast<T**>(&next);
        }

        /** Access to the previous element.
         * @pre there is a previous element: previous != NULL
         * @return address of previous element.
         */
        T* getPrevious() const
        {
            /* the structure has next* and previous**
             * where previous is the address of the previous
             * next* == beginning of the previous element.
             */
            return reinterpret_cast<T*>(previous);
        }

        /** Cast access to previous.
         * @return typed previous.
         */
        T** getPreviousPtr() const
        {
            return reinterpret_cast<T**>(previous);
        }
    };
}


#endif // INCLUDE_BASE_LINKABLE_H
