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
// Filename : Enumerator.h (base)
//
// LinkableEnumerator
//  |
//  +-Enumerator<T>
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Enumerator.h,v 1.5 2005/01/25 18:30:22 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_ENUMERATOR_H
#define INCLUDE_BASE_ENUMERATOR_H

#include <cstddef>
#include "base/inttypes.h"
#include "base/linkable.h"

namespace base
{
    /** Generic enumerator for linkables: enumerate on a
     * table of linkables. Typically, this is used in
     * hash tables.
     */
    class LinkableEnumerator
    {
    public:

        /** Constructor:
         * @param sizeOfTable: size of the table
         * of linkables
         * @param theTable: table of linkables
         * @pre theTable is a Linkable_t*[sizeOfTable]
         */
        LinkableEnumerator(size_t sizeOfTable,
                           SingleLinkable_t **theTable)
            : size(sizeOfTable), table(theTable), current(*theTable)
            {}
        
        /** Enumerate all the nodes in the table. The
         * order is not specified. In practice we start
         * from the beginning of the table and we enumerate
         * all nodes in natural order.
         * @return next linkable, or NULL if there are no
         * linkable left.
         * @post internal state is changed to return the
         * next linkable everytime the method is called.
         */
        SingleLinkable_t *getNextLinkable();
        
    private:
        size_t size;          /**< size of the table           */
        SingleLinkable_t **table;  /**< table of linkables          */
        SingleLinkable_t *current; /**< current node in enumeration */
    };


    /** Template for typed enumerators:
     * assume T is a kind of SingleLinkable_t.
     * This is basically a wrapper for typed
     * linkables.
     */
    template<class T>
    class Enumerator : public LinkableEnumerator
    {
    public:
        
        /** Constructor:
         * @see LinkableEnumerator
         */
        Enumerator(size_t sizeOfTable, T **theTable)
            : LinkableEnumerator(sizeOfTable,
                                 reinterpret_cast<SingleLinkable_t**>(theTable))
            {}

        /** Enumeration:
         * Enumerates all nodes in the table in an unspecified order.
         * @return next node or NULL if there are no nodes left.
         * @see LinkableEnumerator
         */
        T* getNext()
        {
            return static_cast<T*>(getNextLinkable());
        }
    };
}


#endif // INCLUDE_BASE_ENUMERATOR_H
