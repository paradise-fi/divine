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
#include <divine/vm/setup.hpp>
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
    CowHeap::Snapshot snap;
    bool accepting:1;
    bool error:1;
};

struct Context : vm::Context< Program, CowHeap >
{
    using Program = Program;
    std::vector< std::string > _trace;
    std::vector< std::pair< int, int > > _stack;
    int _level;

    Context( Program &p ) : vm::Context< Program, CowHeap >( p ), _level( 0 ) {}

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
        set( _VM_CR_Frame, nullPointer() );
    }

    void trace( TraceText tt )
    {
        _trace.push_back( heap().read_string( tt.text ) );
    }

    void trace( TraceSchedInfo ) { NOT_IMPLEMENTED(); }
    void trace( TraceSchedChoice ) { NOT_IMPLEMENTED(); }
    void trace( TraceStateType ) {}

    bool finished()
    {
        _level = 0;
        _trace.clear();
        while ( !_stack.empty() && _stack.back().first + 1 == _stack.back().second )
            _stack.pop_back();
        return _stack.empty();
    }
};

}

struct Explore
{
    using PointerV = value::Pointer;
    using Eval = vm::Eval< Program, explore::Context, value::Void >;

    using BC = std::shared_ptr< BitCode >;
    using Env = std::vector< std::string >;
    using State = explore::State;
    using Snapshot = CowHeap::Snapshot;

    BC _bc;

    explore::Context _ctx;

    struct Hasher
    {
        mutable CowHeap h1, h2;
        HeapPointer root;

        Hasher( const CowHeap &heap )
            : h1( heap ), h2( heap )
        {}

        bool equal( Snapshot a, Snapshot b ) const
        {
            h1.restore( a );
            h2.restore( b );
            return heap::compare( h1, h2, root, root ) == 0;
        }

        brick::hash::hash128_t hash( Snapshot s ) const
        {
            h1.restore( s );
            return heap::hash( h1, root );
        }
    };

    hashset::Fast< Snapshot, Hasher > _states;
    explore::State _initial;

    auto &program() { return _bc->program(); }
    auto &pool() { return _ctx.heap()._snapshots; }

    Explore( BC bc )
        : _bc( bc ), _ctx( _bc->program() ), _states( Hasher( _ctx.heap() ) )
    {
        _initial.error = _initial.accepting = 0;
    }

    void start()
    {
        Eval eval( program(), _ctx );
        setup::boot( _ctx );
        eval.run();
        _states.hasher.root = _ctx.get( _VM_CR_State ).pointer;
        if ( !(_ctx.get( _VM_CR_Flags ).integer & _VM_CF_Cancel ) )
        {
            _initial.snap = _ctx.heap().snapshot();
            _states.insert( _initial.snap );
        }
    }

    template< typename Ctx >
    void start( const Ctx &ctx, State st )
    {
        _ctx.load( ctx ); /* copy over registers */
        _states.hasher.root = _ctx.get( _VM_CR_State ).pointer;
        _initial = st;
        _states.insert( st.snap );
    }

    template< typename Y >
    void edges( explore::State from, Y yield )
    {
        Eval eval( program(), _ctx );

        do {
            _ctx.heap().restore( from.snap );
            setup::scheduler( _ctx );
            eval.run();
            if ( !( _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Cancel ) )
            {
                explore::State st;
                auto r = _states.insert( _ctx.heap().snapshot() );
                st.snap = *r;
                st.accepting = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Accepting;
                st.error = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error;
                yield( st, _ctx._trace, r.isnew() );
            }
        } while ( !_ctx.finished() );
    }

    template< typename Y >
    void initials( Y yield )
    {
        if ( _initial.snap.slab() ) /* fixme, better validity check */
            yield( _initial );
    }
};

}

namespace t_vm {

using namespace std::literals;

struct TestExplore
{
    auto prog( std::string p )
    {
        return c2bc(
            "void *__vm_obj_make( int );"s +
            "void *__vm_control( int, ... );"s +
            "int __vm_choose( int, ... );"s +
            "void __boot( void * );"s + p );
    }

    TEST(instance)
    {
        auto bc = prog( "void __boot( void *e ) { __vm_control( 2, 7, 0b10000ull, 0b10000ull ); }" );
        vm::Explore ex( bc );
    }

    auto prog_int( std::string first, std::string next )
    {
        return prog(
            R"(void __sched() {
                int *r = __vm_control( 1, 5 );
                *r = )" + next + R"(;
                if ( *r < 0 ) __vm_control( 2, 7, 0b10000ull, 0b10000ull );
            }
            void __boot( void *environ ) {
                __vm_control( 0, 4, __sched );
                void *e = __vm_obj_make( sizeof( int ) );
                __vm_control( 0, 5, e );
                int *r = e; *r = )" + first + "; }"s );
    }

    TEST(simple)
    {
        auto bc = prog_int( "4", "*r - 1" );
        vm::Explore ex( bc );
        bool found = false;
        ex.start();
        ex.initials( [&]( auto ) { found = true; } );
        ASSERT( found );
    }

    void _search( std::shared_ptr< vm::BitCode > bc, int sc, int ec )
    {
        vm::Explore ex( bc );
        int edgecount = 0, statecount = 0;
        ex.start();
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
