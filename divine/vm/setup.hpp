// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/value.hpp>
#include <divine/vm/divm.h>
#include <brick-except>

namespace divine::vm::setup
{

    template< typename Context >
    void boot( Context &ctx )
    {
        ctx.reset();
        auto data = ctx.program().exportHeap( ctx.heap() );
        ctx.set( _VM_CR_Constants, data.first );
        ctx.set( _VM_CR_Globals, data.second );
        auto ipc = ctx.program().functionByName( "__boot" );
        auto &fun = ctx.program().function( ipc );
        if ( fun.argcount != 1 )
            throw brick::except::Error( "__boot must take exactly 1 argument" );
        auto envptr = ctx.program().globalByName( "__sys_env" );
        ctx.enter( ipc, nullPointerV(), value::Pointer( envptr ) );
        ctx.ref( _VM_CR_Flags ).integer = _VM_CF_KernelMode | _VM_CF_Booting |
                                          _VM_CF_IgnoreLoop | _VM_CF_IgnoreCrit;
    }

    template< typename Context >
    bool postboot_check( Context &ctx )
    {
        if ( ctx.ref( _VM_CR_Flags ).integer & _VM_CF_Error )
            return false; /* can't schedule if boot failed */
        if ( ctx.get( _VM_CR_Flags ).integer & _VM_CF_Cancel )
            return false;
        if ( ctx.ref( _VM_CR_Scheduler ).pointer.type() != PointerType::Code )
            return false;
        if ( !ctx.heap().valid( ctx.get( _VM_CR_State ).pointer ) )
            return false;

        return true;
    }

    template< typename Context >
    void scheduler( Context &ctx )
    {
        ctx.enter( ctx.get( _VM_CR_Scheduler ).pointer, nullPointerV() );
        ctx.set( _VM_CR_IntFrame, ctx.frame() );
        ctx.ref( _VM_CR_Flags ).integer = _VM_CF_KernelMode | _VM_CF_IgnoreLoop | _VM_CF_IgnoreCrit;
        ctx.flush_ptr2i();
    }

}
