// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <sys/divm.h>
#include <stdlib.h>   /* size_t */

namespace __dios
{

    /* All classes that are dynamically allocated in the kernel should inherit
     * from KObject. Use new/delete to manage the allocations like normal.
     * Bypasses out-of-memory simulation and all the overhead associated with
     * new/malloc and delete/free. */

    struct KObject
    {
        static void operator delete( void *p ) { __vm_obj_free( p ); }
        static void *operator new( size_t s ) { return __vm_obj_make( s, _VM_PT_Heap ); }
    };

    /* Like above, but for objects which track debug metadata. See `dios/sys/debug.hpp`. */

    struct DbgObject : KObject
    {
        static void *operator new( size_t s ) { return __vm_obj_make( s, _VM_PT_Weak ); }
    };

}
