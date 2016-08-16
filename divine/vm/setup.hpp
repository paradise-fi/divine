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
#include <divine/vm/program.hpp>

namespace divine {
namespace vm {

template< typename Context >
struct Setup
{
    using Heap = typename Context::Heap;

    Program &_program;
    Context &_ctx;

    void setup()
    {
        _program.setupRR();
        _program.computeRR();

        _program.computeStatic();
        auto data = _program.exportHeap( _ctx.heap() );
        _ctx.constants( data.first );
        _ctx.globals( data.second );

        auto ipc = _program.functionByName( "__sys_init" );
        auto envptr = _program.globalByName( "__sys_env" );
        _ctx.enter( ipc, nullPointer(), value::Pointer( envptr ) );
    }

    Setup( Program &p, Context &c )
        : _program( p ), _ctx( c )
    {}
};

template< typename Context >
void setup( Program &p, Context &ctx )
{
    Setup< Context > setup( p, ctx );
    setup.setup();
}

}
}
