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
#include <divine/vm/eval.hpp>
#include <runtime/divine.h>

namespace divine {
namespace vm {

template< typename Context >
struct Setup
{
    using Heap = typename Context::Heap;
    using Program = typename Context::Program;

    Context &_ctx;

    void setup()
    {
        _ctx.program().setupRR();
        _ctx.program().computeRR();

        _ctx.program().computeStatic();
        auto data = _ctx.program().exportHeap( _ctx.heap() );
        _ctx.set( _VM_CR_Constants, data.first );
        _ctx.set( _VM_CR_Globals, data.second );

        auto ipc = _ctx.program().functionByName( "__boot" );
        auto envptr = _ctx.program().globalByName( "__sys_env" );
        _ctx.enter( ipc, nullPointerV(), value::Pointer( envptr ) );

        Eval< Program, Context, value::Void > eval( _ctx.program(), _ctx );
        _ctx.mask( true );
        eval.run();
    }

    Setup( Context &c )
        : _ctx( c )
    {}
};

template< typename Context >
void setup( Context &ctx )
{
    Setup< Context > setup( ctx );
    setup.setup();
}

template< typename Eval >
void schedule( Eval &eval )
{
    eval.context().enter( eval.context().get( _VM_CR_Scheduler ).pointer, nullPointerV() );
    eval.context().set( _VM_CR_IntFrame, eval.context().frame() );
    eval.context().mask( true );
}

}
}
