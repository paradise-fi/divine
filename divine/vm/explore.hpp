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

#include <divine/vm/heap.hpp>
#include <divine/vm/context.hpp>
#include <divine/vm/eval.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/ss/search.hpp> /* unit tests */

#include <set>
#include <memory>

namespace divine {
namespace vm {

using namespace brick;

namespace explore {

struct State
{
    std::shared_ptr< MutableHeap > heap;
    MutableHeap::Pointer root;
    bool operator<( State s ) const { return compare( *heap, *s.heap, root, s.root ) < 0; }
};

struct Context : vm::Context< MutableHeap >
{
    std::vector< std::string > _trace;
    std::stack< State > _stack;

    using vm::Context< MutableHeap >::Context;

    template< typename I >
    int choose( int o, I, I )
    {
        _stack.emplace();
        // auto &st = _stack.top();
        // st.heap = std::make_shared< MutableHeap >();
        // st.root = freeze();
    }

    void doublefault()
    {
        trace( "fatal double fault" );
        _frame = nullPointer();
    }

    void trace( std::string s )
    {
        _trace.push_back( s );
    }

    bool finished() { return true; }
};

}

struct Explore
{
    using Heap = MutableHeap;
    using PointerV = value::Pointer< Heap::Pointer >;
    using Eval = vm::Eval< Program, explore::Context, PointerV >;

    using BC = std::shared_ptr< BitCode >;
    using Env = std::vector< std::string >;
    using State = explore::State;

    BC _bc;

    explore::Context _ctx;

    std::set< explore::State > _states;

    auto &program() { return _bc->program(); }

    Explore( BC bc )
        : _bc( bc ), _ctx( _bc->program() )
    {
    }

    explore::State snap( Eval &ev )
    {
        explore::State st;
        st.heap = std::make_shared< MutableHeap >();
        st.root = clone( _ctx.heap(), *st.heap, ev._result.cooked() );
        return st;
    }

    template< typename Y >
    void edges( explore::State from, Y yield )
    {
        Eval eval( program(), _ctx );
        auto root = clone( *from.heap, _ctx.heap(), from.root );

        do {
            std::cerr << "sched = " << _ctx._sched << std::endl;
            _ctx.enter( _ctx._sched, nullPointer(),
                        Eval::IntV( eval.heap().size( root ) ), PointerV( root ) );
            _ctx.mask( true );
            eval.run();
            explore::State st = snap( eval );
            auto r = _states.insert( st );
            if ( !st.root.null() )
                yield( st, 0, r.second );
        } while ( !_ctx.finished() );
    }

    template< typename Y >
    void initials( Y yield )
    {
        Eval eval( program(), _ctx );

        setup( program(), _ctx );
        _ctx.mask( true );
        eval.run(); /* run __sys_init */

        auto st = snap( eval );
        _states.insert( st );
        yield( st );
    }
};

}

namespace t_vm {

struct TestExplore
{
    TEST(instance)
    {
        auto bc = c2bc( "void *__sys_init( void *e ) { return 0; }" );
        vm::Explore ex( bc );
    }

    static constexpr const char * const lin5 =
        "void *__vm_make_object( int );"
        "void *__vm_set_sched( void *(*)( int, void * ) );"
        "void *__sys_sched( int sz, void *s ) { int *r = s; *r = *r - 1; return *r > 0 ? s : 0; }"
        "void *__sys_init( void *e ) { __vm_set_sched( __sys_sched ); "
        "int *r = __vm_make_object( 4 ); *r = 5; return r; }";

    static constexpr const char * const cir5 =
        "void *__vm_make_object( int );"
        "void *__vm_set_sched( void *(*)( int, void * ) );"
        "void *__sys_sched( int sz, void *s ) { int *r = s; *r = ( *r + 1 ) % 5; return s; }"
        "void *__sys_init( void *e ) { __vm_set_sched( __sys_sched ); "
        "int *r = __vm_make_object( 4 ); *r = 5; return r; }";

    TEST(simple)
    {
        auto bc = c2bc( lin5 );
        vm::Explore ex( bc );
        bool found = false;
        ex.initials( [&]( auto st ) { found = true; } );
        ASSERT( found );
    }

    TEST(search)
    {
        auto bc = c2bc( lin5 );
        vm::Explore ex( bc );
        int edgecount = 0, statecount = 0;
        ss::search( ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
                        [&]( auto, auto, auto ) { ++edgecount; },
                        [&]( auto ) { ++statecount; } ) );
        ASSERT_EQ( statecount, 5 );
        ASSERT_EQ( edgecount, 5 );
    }
};

}

}
