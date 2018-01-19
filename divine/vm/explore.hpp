// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2018 Petr Ročkai <code@fixp.eu>
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
#include <divine/vm/solver.hpp>
#include <divine/ss/search.hpp> /* unit tests */

#include <set>
#include <memory>

namespace divine {
namespace vm {

using namespace std::literals;
namespace hashset = brick::hashset;

namespace explore {

struct State
{
    CowHeap::Snapshot snap;
    bool operator==( const State& o ) const { return snap.intptr() == o.snap.intptr(); }
};

struct Context : vm::Context< Program, CowHeap >
{
    using Super = vm::Context< Program, CowHeap >;
    using Program = Program;
    std::vector< std::string > _trace;
    std::string _info;
    std::vector< Choice > _stack;
    std::deque< Choice > _lock;
    HeapPointer _assume;
    GenericPointer _tid;

    int _level;

    Context( Program &p ) : Super( p ), _level( 0 ) {}

    template< typename I >
    int choose( int count, I, I )
    {
        ASSERT( !this->debug_mode() );
        if ( _lock.size() )
        {
            auto rv = _lock.front();
            ASSERT_EQ( count, rv.total );
            _lock.pop_front();
            _stack.push_back( rv );
            return rv.taken;
        }

        ASSERT_LEQ( _level, int( _stack.size() ) );
        _level ++;
        if ( _level < int( _stack.size() ) )
            return _stack[ _level - 1 ].taken;
        if ( _level == int( _stack.size() ) )
            return ++ _stack[ _level - 1 ].taken;
        _stack.emplace_back( 0, count );
        return 0;
    }

    using Super::trace;

    void trace( TraceSchedInfo ) {} /* noop */
    void trace( TraceSchedChoice ) {} /* noop */
    void trace( TraceStateType ) {}
    void trace( TraceTypeAlias ) {}

    void trace( std::string s ) { _trace.push_back( s ); }
    void trace( TraceDebugPersist t ) { Super::trace( t ); }
    void trace( TraceAssume ta ) { _assume = ta.ptr; }

    void trace( TraceTaskID tid )
    {
        ASSERT( _tid.null() );
        _tid = tid.ptr;
    }

    void trace( TraceInfo ti )
    {
        _info += heap().read_string( ti.text ) + "\n";
    }

    bool finished()
    {
        _level = 0;
        _tid = GenericPointer();
        _trace.clear();
        _assume = HeapPointer();
        while ( !_stack.empty() && _stack.back().taken + 1 == _stack.back().total )
            _stack.pop_back();
        return _stack.empty();
    }
};

using Snapshot = CowHeap::Snapshot;

template< typename Solver >
struct Hasher_
{
    mutable CowHeap _h1, _h2;
    HeapPointer _root;
    Solver *_solver = nullptr;

    void setup( const CowHeap &heap, Solver &solver )
    {
        _h1 = heap;
        _h2 = heap;
        _solver = &solver;
    }

    bool equal_fastpath( Snapshot a, Snapshot b ) const
    {
        bool rv = false;
        if ( _h1._snapshots.size( a ) == _h1._snapshots.size( b ) )
            rv = std::equal( _h1.snap_begin( a ), _h1.snap_end( a ), _h1.snap_begin( b ) );
        if ( !rv )
            _h1.restore( a ), _h2.restore( b );
        return rv;
    }

    bool equal_explicit( Snapshot a, Snapshot b ) const
    {
        if ( equal_fastpath( a, b ) )
            return true;
        else
            return heap::compare( _h1, _h2, _root, _root ) == 0;
    }

    bool equal_symbolic( Snapshot a, Snapshot b ) const
    {
        if ( equal_fastpath( a, b ) )
            return true;

        std::vector< std::pair< HeapPointer, HeapPointer > > sym_pairs;

        auto extract = [&]( HeapPointer a, HeapPointer b )
        {
            a.type( PointerType::Weak ); // unmark pointers so they are equal to their
            b.type( PointerType::Weak ); // weak equivalents inside the formula
            sym_pairs.emplace_back( a, b );
        };

        if ( heap::compare( _h1, _h2, _root, _root, extract ) != 0 )
            return false;

        if ( sym_pairs.empty() )
            return true;

        ASSERT( _solver );
        return _solver->equal( sym_pairs, _h1, _h2 ) == Solver::Result::True;
    }

    brick::hash::hash128_t hash( Snapshot s ) const
    {
        _h1.restore( s );
        return heap::hash( _h1, _root );
    }
};

template< typename Solver >
struct Hasher : Hasher_< Solver >
{
    bool equal( Snapshot a, Snapshot b ) const { return this->equal_symbolic( a, b ); }
};

template<>
struct Hasher< NoSolver > : Hasher_< NoSolver >
{
    bool equal( Snapshot a, Snapshot b ) const { return this->equal_explicit( a, b ); }
};

using BC = std::shared_ptr< BitCode >;

} // namespace explore

template< typename Solver >
struct Explore
{
    using PointerV = value::Pointer;
    using Context = explore::Context;
    using Eval = vm::Eval< Context >;
    using Hasher = explore::Hasher< Solver >;

    using BC = explore::BC;
    using Env = std::vector< std::string >;
    using State = explore::State;
    using Snapshot = CowHeap::Snapshot;

    struct Label : brick::types::Ord
    {
        std::vector< std::string > trace;
        std::vector< Choice > stack;
        std::vector< Interrupt > interrupts;
        bool accepting:1;
        bool error:1;
        auto as_tuple() const
        {
            /* skip the text trace for comparison purposes */
            return std::make_tuple( stack, interrupts, accepting, error );
        }
    };

    using HT = hashset::Concurrent< Snapshot, Hasher >;

    auto &program() { return _d.bc->program(); }
    auto &pool() { return _d.ctx.heap()._snapshots; }

    struct Data
    {
        BC bc;
        Context ctx;
        HT states;
        explore::State initial;
        Solver solver;

        bool overwrite = false;
        int64_t instructions = 0;
        std::shared_ptr< std::atomic< int64_t > > total_instructions;

        Data( BC bc )
            : Data( bc, Context( bc->program() ), HT( Hasher(), 1024 ) )
        {}

        Data( BC bc, const Context &ctx, HT states )
            : bc( bc ), ctx( ctx ), states( states ),
              total_instructions( new std::atomic< int64_t >( 0 ) )
        {}

        ~Data() { *total_instructions += instructions; }
    } _d;

    Context &context() { return _d.ctx; }
    void enable_overwrite() { _d.overwrite = true; }

    Explore( const Explore &e ) : _d( e._d )
    {
        _d.states.hasher.setup( context().heap(), _d.solver );
    }

    template< typename... Args >
    Explore( BC bc, Args && ... args ) : _d( bc, args... )
    {
        _d.states.hasher.setup( context().heap(), _d.solver );
    }

    auto store( Snapshot snap )
    {
        auto r = _d.states.insert( snap, _d.overwrite );
        if ( *r != snap )
        {
            ASSERT( !_d.overwrite );
            pool().free( snap ), context().load( *r );
        }
        else
            context().flush_ptr2i();
        return r;
    }

    void start()
    {
        Eval eval( context() );
        setup::boot( context() );
        eval.run();
        _d.states.hasher._root = context().get( _VM_CR_State ).pointer;

        if ( setup::postboot_check( context() ) )
            _d.initial.snap = *store( context().snapshot() );
        _d.states.updateUsage();
        if ( !context().finished() )
            UNREACHABLE( "choices encountered during start()" );
    }

    template< typename Ctx >
    Snapshot start( const Ctx &ctx, Snapshot snap )
    {
        context().load( ctx ); /* copy over registers */
        _d.states.hasher.setup( context().heap(), _d.solver );
        _d.states.hasher._root = context().get( _VM_CR_State ).pointer;

        if ( context().heap().valid( _d.states.hasher._root ) )
            _d.initial.snap = *store( snap );
        _d.states.updateUsage();
        if ( !context().finished() )
            UNREACHABLE( "choices encountered during start()" );
        return _d.initial.snap;
    }

    Label label()
    {
        Label lbl;
        lbl.trace = context()._trace;
        lbl.stack = context()._stack;
        lbl.accepting = context().get( _VM_CR_Flags ).integer & _VM_CF_Accepting;
        lbl.error = context().get( _VM_CR_Flags ).integer & _VM_CF_Error;
        lbl.interrupts = context()._interrupts;
        return lbl;
    }

    bool equal( Snapshot a, Snapshot b )
    {
        return _d.states.hasher.equal( a, b );
    }

    bool feasible()
    {
        if ( context().get( _VM_CR_Flags ).integer & _VM_CF_Cancel )
            return false;
        if ( context()._assume.null() )
            return true;
        return _d.solver.feasible( context().heap(), context()._assume ) == Solver::Result::True;
    }

    template< typename Y >
    void edges( explore::State from, Y yield )
    {
        Eval eval( context() );
        ASSERT_EQ( context()._level, 0 );
        ASSERT( context()._stack.empty() );
        ASSERT( context()._trace.empty() );
        ASSERT( context()._lock.empty() );

        struct Check
        {
            std::deque< Choice > lock;
            typename Context::MemMap loads, stores;
            Label lbl;
            Snapshot snap;
            bool free;
        };

        std::vector< Check > to_check;

        context()._crit_loads.clear();
        context()._crit_stores.clear();

        auto do_yield = [&]( Snapshot snap, Label lbl )
        {
            explore::State st;
            auto r = store( snap );
            st.snap = *r;

            yield( st, lbl, r.isnew() );
        };

        do {
            context().load( from.snap );
            setup::scheduler( context() );
            eval.run();
            _d.instructions += context()._instruction_counter;
            if ( feasible() )
            {
                to_check.emplace_back();
                auto &tc = to_check.back();
                tc.loads = context()._mem_loads;
                tc.stores = context()._mem_stores;

                for ( auto c : context()._stack )
                    tc.lock.push_back( c );

                tc.snap = context().heap().snapshot();
                tc.free = !context().heap().is_shared( tc.snap );
                tc.lbl = label();
            }
        } while ( !context().finished() );

        for ( auto &tc : to_check )
        {
            typename Context::MemMap l, s;

            for ( auto &other : to_check )
            {
                if ( &tc == &other )
                    continue;
                for ( auto ol : other.loads )
                    if ( tc.stores.intersect( ol.first, ol.second ) )
                        s.insert( ol.first, ol.second );
                for ( auto sl : other.stores )
                    if ( tc.loads.intersect( sl.first, sl.second ) )
                        l.insert( sl.first, sl.second );
            }

            if ( s.empty() && l.empty() )
                do_yield( tc.snap, tc.lbl );
            else
            {
                if ( tc.free )
                    pool().free( tc.snap );
                context()._lock = tc.lock;
                context().load( from.snap );
                ASSERT( context()._stack.empty() );
                ASSERT_EQ( context()._level, 0 );
                context()._crit_loads = l;
                context()._crit_stores = s;
                setup::scheduler( context() );

                eval.run(); /* will not cancel since this is a prefix */
                _d.instructions += context()._instruction_counter;
                auto lbl = label();
                do_yield( context().heap().snapshot(), lbl );

                int i = 0;
                for ( auto t : lbl.stack )
                    ASSERT( t == tc.lock[ i++ ] );

                context()._stack.clear();
                context()._lock.clear();
                context().finished();
            }
        }
    }

    template< typename Y >
    void initials( Y yield )
    {
        if ( _d.initial.snap.slab() ) /* fixme, better validity check */
            yield( _d.initial );
    }
};

using ExplicitExplore = Explore< NoSolver >;
using Z3Explore = Explore< Z3SMTLibSolver >;
using BoolectorExplore = Explore< BoolectorSMTLib >;

} // namespace vm

namespace t_vm {

using namespace std::literals;

namespace {

auto prog( std::string p )
{
    return c2bc(
        "void *__vm_obj_make( int );"s +
        "void *__vm_control( int, ... );"s +
        "int __vm_choose( int, ... );"s +
        "void __boot( void * );"s + p );
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

}

struct TestExplore
{
    TEST(instance)
    {
        auto bc = prog( "void __boot( void *e ) { __vm_control( 2, 7, 0b10000ull, 0b10000ull ); }" );
        vm::ExplicitExplore ex( bc );
    }

    TEST(simple)
    {
        auto bc = prog_int( "4", "*r - 1" );
        vm::ExplicitExplore ex( bc );
        bool found = false;
        ex.start();
        ex.initials( [&]( auto ) { found = true; } );
        ASSERT( found );
    }

    void _search( std::shared_ptr< vm::BitCode > bc, int sc, int ec )
    {
        vm::ExplicitExplore ex( bc );
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
