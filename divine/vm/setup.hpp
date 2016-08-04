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
    using PointerV = value::Pointer< typename Heap::Pointer >;

    std::vector< std::tuple< std::string, Program::SlotRef > > _env;
    Program &_program;
    Context &_ctx;

    /* must be called before setup() */
    void putenv( std::string str )
    {
        Program::Slot s( str.size() + 1, Program::Slot::Constant );
        s.type = Program::Slot::Aggregate;
        auto sref = _program.allocateSlot( s );
        _env.push_back( std::make_tuple( str, sref ) );
    }

    void setup_env()
    {
        auto &heap = _program._ccontext.heap();
        for ( auto e : _env )
        {
            auto str = std::get< 0 >( e );
            auto ptr = PointerV( _program.s2hptr( std::get< 1 >( e ).slot ) );
            std::for_each( str.begin(), str.end(),
                           [&]( char c )
                           {
                               heap.write_shift( ptr, value::Int< 8 >( c ) );
                           } );
            heap.write_shift( ptr, value::Int< 8 >( 0 ) );
        }
    }

    template< typename C >
    void setup( const C &env )
    {
        _program.setupRR();
        _program.computeRR();

        for ( auto e : env )
            putenv( e );

        _program.computeStatic();
        setup_env();
        auto data = _program.exportHeap( _ctx.heap() );
        _ctx.constants( data.first );
        _ctx.globals( data.second );

        auto ipc = _program.functionByName( "__sys_init" );
        auto envptr = _program.globalByName( "__sys_env" );
        ipc.instruction( 1 );
        _ctx.enter( ipc, nullPointer(), PointerV( envptr ) );
    }

    Setup( Program &p, Context &c )
        : _program( p ), _ctx( c )
    {}
};

template< typename Context, typename Env >
void setup( Program &p, Context &ctx, Env &env )
{
    Setup< Context > setup( p, ctx );
    setup.setup( env );
}

}
}
