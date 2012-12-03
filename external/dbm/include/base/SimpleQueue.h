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
// Filename : SimpleQueue.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: SimpleQueue.h,v 1.5 2004/09/08 09:02:57 johanb Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_SIMPLEQUEUE_H
#define INCLUDE_BASE_SIMPLEQUEUE_H

#include "base/linkable.h"
#include "base/inttypes.h"

namespace base
{
    /** Simple queue for linkable objects,
     * no allocation, very simple.
     */
    class SimpleQueue
    {
    public:
        virtual ~SimpleQueue();

        /** Kinds of queue.
         */
        enum { FIFO, LIFO };

        /** Get instances. You need to
         * delete them afterwards.
         * @param kind: kind of queue.
         */
        static SimpleQueue* newInstance(uint32_t kind);

        /** put an object in the queue.
         * @param o: object to put in
         */
        virtual void put(SingleLinkable_t *o) = 0;

        /** get an object from the queue.
         * @return NULL if queue is empty,
         * or !=NULL object that was put previously.
         */
        virtual SingleLinkable_t* get() = 0;

        /** reset the queue.
         * Resets the queue without deallocating any queue objects.
         */
        virtual void reset() = 0;
    };

} // namespace base

#endif // INCLUDE_BASE_SIMPLEQUEUE_H
