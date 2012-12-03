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
// Filename : SimpleQueue.cpp
//
// Implementations of SimpleQueue
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: SimpleQueue.cpp,v 1.4 2005/01/28 20:24:21 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include "base/SimpleQueue.h"
#include "debug/new.h"

// Since SimpleQueue is very simple we have the
// implemenations here.

namespace base
{
    class SimpleLIFOQueue : public SimpleQueue
    {
    public:
        SimpleLIFOQueue()
            : head(NULL) {}

        virtual ~SimpleLIFOQueue() {}

        /* put the object at the head
         * of the queue
         */
        virtual void put(SingleLinkable_t *obj)
        {
            obj->next = head;
            head = obj;
        }

        /* get an object from the head of
         * the queue
         */
        virtual SingleLinkable_t* get()
        {
            SingleLinkable_t *obj = head;
            if (head) head = head->next;
            return obj;
        }

        virtual void reset()
        {
            head = NULL;
        }

    protected:
        SingleLinkable_t *head;
    };

    /* reuse code from LIFO queue
     */
    class SimpleFIFOQueue : public SimpleLIFOQueue
    {
    public:
        SimpleFIFOQueue()
            : tail(NULL) {}

        virtual ~SimpleFIFOQueue() {}

        /* put the object at the tail
         * of the queue
         */
        virtual void put(SingleLinkable_t *obj)
        {
            obj->next = NULL;
            if (head)
            {
                tail->next = obj;
                tail = obj;
            }
            else
            {
                head = tail = obj;
            }
        }

        virtual void reset()
        {
            tail = head = NULL;
        }

    private:
        SingleLinkable_t *tail;
    };

    
    // do nothing
    SimpleQueue::~SimpleQueue()
    {}

    // simple switch
    SimpleQueue* SimpleQueue::newInstance(uint32_t kind)
    {
        switch(kind)
        {
        default:
        case FIFO: return new SimpleFIFOQueue();
        case LIFO: return new SimpleLIFOQueue();
        }
    }
}
