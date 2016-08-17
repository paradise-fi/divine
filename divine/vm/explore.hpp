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
    MutableHeap *_heap; /* for operator< */
    HeapPointer root, globals;
    bool operator<( State s ) const
    {
        ASSERT_EQ( _heap, s._heap );
        return compare( *_heap, *_heap, root, s.root ) < 0;
    }
};

struct Context : vm::Context< MutableHeap >
{
    std::vector< std::string > _trace;
    std::vector< std::pair< int, int > > _stack;
    int _level;

    Context( Program &p ) : vm::Context< MutableHeap >( p ), _level( 0 ) {}

    explore::State snap( HeapPointer root )
    {
        explore::State st;
        std::map< HeapPointer, HeapPointer > pmap;
        st.globals = clone( heap(), heap(), globals().cooked(), pmap );
        st.root = clone( heap(), heap(), root, pmap );
        st._heap = &heap();
        return st;
    }

    HeapPointer load( explore::State st )
    {
        _t.entry_frame = nullPointer();
        std::map< HeapPointer, HeapPointer > pmap;
        globals( clone( heap(), heap(), st.globals, pmap ) );
        return clone( heap(), heap(), st.root, pmap );
    }

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
        _trace.clear();
        _t.entry_frame = nullPointer();
        while ( !_stack.empty() && _stack.back().first + 1 == _stack.back().second )
            _stack.pop_back();
        return _stack.empty();
    }
};

struct StContext
{
    using Heap = Context::Heap;
    using PointerV = value::Pointer;

    State _state;
    Context &_ctx;

    Program &program() { return _ctx.program(); }
    PointerV globals() { return _state.globals; }
    PointerV constants() { return _ctx.constants(); }
    bool mask( bool = false ) { return false; }
    bool set_interrupted( bool = false ) { return false; }
    void check_interrupt() {}
    void cfl_interrupt( CodePointer ) {}

    template< typename I >
    int choose( int, I, I ) { return 0; }

    PointerV frame() { NOT_IMPLEMENTED(); }
    void frame( PointerV p ) { NOT_IMPLEMENTED(); }
    bool isEntryFrame( HeapPointer fr ) { NOT_IMPLEMENTED(); }

    void fault( Fault f, PointerV, CodePointer ) { NOT_IMPLEMENTED(); }
    void trace( std::string s ) { std::cerr << "T: " << s << std::endl; }

    template< typename... Args >
    void enter( CodePointer pc, PointerV parent, Args... args )
    {
        NOT_IMPLEMENTED();
    }

    bool sched( CodePointer ) { return true; }
    bool fault_handler( CodePointer ) { return true; }
    void setIfl( PointerV ) {}

    Heap &heap() { return _ctx.heap(); }
    StContext( Context &ctx, State &st )
        : _ctx( ctx ), _state( st )
    {}
};

}

struct Explore
{
    using Heap = MutableHeap;
    using PointerV = value::Pointer;
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
        setup( program(), _ctx );
    }

    template< typename Y >
    void edges( explore::State from, Y yield )
    {
        Eval eval( program(), _ctx );

        do {
            auto root = _ctx.load( from );
            _ctx.enter( _ctx.sched(), nullPointer(),
                        Eval::IntV( eval.heap().size( root ) ), PointerV( root ) );
            _ctx.mask( true );
            eval.run();
            if ( !eval._result.cooked().null() )
            {
                explore::State st = _ctx.snap( eval._result.cooked() );
                auto r = _states.insert( st );
                yield( st, _ctx._trace, r.second );
            }
        } while ( !_ctx.finished() );
    }

    template< typename Y >
    void initials( Y yield )
    {
        Eval eval( program(), _ctx );
        _ctx.mask( true );
        eval.run(); /* run __sys_init */

        auto st = _ctx.snap( eval._result.cooked() );
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
