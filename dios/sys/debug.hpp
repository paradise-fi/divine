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

#include <cstdint>
#include <cstdarg>

#include <util/map.hpp>
#include <dios.h>
#include <dios/sys/kobject.hpp>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

/* Data that is not part of the execution of the system, but instead only
 * serves to generate human-readable outputs during counterexample generation.
 * None of this is instantiated during standard execution, since the code that
 * uses the data is hidden in `dbg.call` instructions, which are in turn
 * generated from the `__debugfn` annotations in the source code. A function
 * that is annotated with `__debugfn` switches the VM into a ‘debug mode’ in
 * which the program is not allowed to make any persistent changes to data
 * outside of objects are allocated as ‘weak’ (see also `DbgObject`). */

namespace __dios
{

    struct Debug : DbgObject
    {
        AutoIncMap< __dios_task, int, _VM_PT_Weak > hids;
        ArrayMap< __dios_task, short, _VM_PT_Weak > trace_indent;
        ArrayMap< __dios_task, short, _VM_PT_Weak > trace_inhibit;
        ArrayMap< __dios_task, Array< char, _VM_PT_Weak >, _VM_PT_Weak > trace_buf;
        short kernel_indent = 0;
        void persist();
        void persist_buffers();
    };

    static inline bool debug_mode() noexcept
    {
        return __vm_ctl_flag( 0, 0 ) & _VM_CF_DebugMode;
    }

    static inline bool have_debug() noexcept
    {
        return debug_mode() && __vm_ctl_get( _VM_CR_User3 );
    }

    static inline Debug &get_debug() noexcept
    {
        if ( debug_mode() && !have_debug() )
        {
            __vm_trace( _VM_T_Text, "have_debug() failed in debug mode" );
            __vm_ctl_set( _VM_CR_Flags, 0 ); /* fault & force abandonment of the debug call */
        }
        __dios_assert( have_debug() );
        void *dbg = __vm_ctl_get( _VM_CR_User3 );
        return *static_cast< Debug * >( dbg );
    }

}
