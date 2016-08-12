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
    std::vector< std::pair< int, int > > _stack;
    int _level;

    Context( Program &p ) : vm::Context< MutableHeap >( p ), _level( 0 ) {}

    template< typename I >
    int choose( int count, I, I )
    {
        ASSERT_LEQ( _level, int( _stack.size() ) );
        _level ++;
        if ( _level < int( _stack.size() ) )
            return _stack[ _level - 1 ].first;
        if ( _level == int( _stack.size() ) )
            return ++ _stack[ _level - 1 ].first;
        _stack.emplace_back( 0, count );
        return 0;
    }

    void doublefault()
    {
        trace( "fatal double fault" );
        _t.frame = nullPointer();
    }

    void trace( std::string s )
    {
        _trace.push_back( s );
    }

    bool finished()
    {
        _level = 0;
        while ( !_stack.empty() && _stack.back().first + 1 == _stack.back().second )
            _stack.pop_back();
        return _stack.empty();
    }
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

        do {
            auto root = clone( *from.heap, _ctx.heap(), from.root );
            _ctx.enter( _ctx.sched(), nullPointer(),
                        Eval::IntV( eval.heap().size( root ) ), PointerV( root ) );
            _ctx.mask( true );
            eval.run();
            if ( eval._result.cooked() != nullPointer() )
            {
                explore::State st = snap( eval );
                auto r = _states.insert( st );
                yield( st, 0, r.second );
            }
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

using namespace std::literals;

struct TestExplore
{
    TEST(instance)
    {
        auto bc = c2bc( "void *__sys_init( void *e ) { return 0; }" );
        vm::Explore ex( bc );
    }

    auto prog( std::string p )
    {
        return c2bc(
            "void *__vm_make_object( int );"s +
            "void *__vm_set_sched( void *(*)( int, void * ) );"s +
            "int __vm_choose( int, ... );"s +
            "void *__sys_sched( int, void * );"s + p );
    }

    auto prog_int( std::string first, std::string next )
    {
        return prog(
            "void *__sys_sched( int sz, void *s ) { int *r = s; *r = "s
            + next + "; return *r < 0 ? 0 : s; }"s +
            "void *__sys_init( void *e ) {"s +
            "    __vm_set_sched( __sys_sched ); "s +
            "    int *r = __vm_make_object( 4 ); *r = "s + first + "; return r; }"s );
    }

    TEST(simple)
    {
        auto bc = prog_int( "4", "*r - 1" );
        vm::Explore ex( bc );
        bool found = false;
        ex.initials( [&]( auto ) { found = true; } );
        ASSERT( found );
    }

    void _search( std::shared_ptr< vm::BitCode > bc, int sc, int ec )
    {
        vm::Explore ex( bc );
        int edgecount = 0, statecount = 0;
        ss::search( ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
                        [&]( auto, auto, auto ) { ++edgecount; },
                        [&]( auto ) { ++statecount; } ) );
        ASSERT_EQ( statecount, sc );
        ASSERT_EQ( edgecount, ec );
    }

    TEST(search)
    {
        _search( prog_int( "4", "*r - 1" ), 5, 4 );
        _search( prog_int( "0", "( *r + 1 ) % 5" ), 5, 5 );
    }

    TEST(branching)
    {
        _search( prog_int( "4", "*r - __vm_choose( 2 )" ), 5, 9 );
        _search( prog_int( "4", "*r - 1 - __vm_choose( 2 )" ), 5, 7 );
        _search( prog_int( "0", "( *r + __vm_choose( 2 ) ) % 5" ), 5, 10 );
        _search( prog_int( "0", "( *r + 1 + __vm_choose( 2 ) ) % 5" ), 5, 10 );
    }
};

}

}
