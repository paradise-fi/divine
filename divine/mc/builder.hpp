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

#include <divine/mc/bitcode.hpp>
#include <divine/smt/solver.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/mem-heap.tpp>
#include <divine/vm/context.hpp>
#include <divine/vm/setup.hpp>
#include <divine/vm/eval.hpp>
#include <divine/ss/search.hpp> /* unit tests */

#include <set>
#include <memory>

namespace divine::mc::builder
{

using namespace std::literals;
using Snapshot = vm::mem::CowHeap::Snapshot;

struct State
{
    Snapshot snap;
    bool operator==( const State& o ) const { return snap.intptr() == o.snap.intptr(); }
};

struct Context : vm::Context< vm::Program, vm::mem::CowHeap >
{
    using Super = vm::Context< vm::Program, vm::mem::CowHeap >;
    using MemMap = Super::MemMap;
    struct Critical { MemMap loads, stores; };

    std::vector< std::string > _trace;
    std::string _info;
    std::vector< vm::Choice > _stack;
    std::deque< vm::Choice > _lock;
    vm::HeapPointer _assume;
    vm::GenericPointer _tid;
    std::unordered_map< vm::GenericPointer, Critical > _critical;
    int _level;

    Context( vm::Program &p ) : Super( p ), _level( 0 ) {}

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

    void trace( vm::TraceSchedInfo ) {} /* noop */
    void trace( vm::TraceSchedChoice ) {} /* noop */
    void trace( vm::TraceStateType ) {}
    void trace( vm::TraceTypeAlias ) {}

    void trace( std::string s ) { _trace.push_back( s ); }
    void trace( vm::TraceDebugPersist t ) { Super::trace( t ); }
    void trace( vm::TraceAssume ta ) { _assume = ta.ptr; }

    void swap_critical()
    {
        if ( !this->_track_mem ) return;
        ASSERT( !_tid.null() );
        swap( _mem_loads, _critical[ _tid ].loads );
        swap( _mem_stores, _critical[ _tid ].stores );
    }

    void trace( vm::TraceTaskID tid )
    {
        ASSERT( _tid.null() );
        ASSERT( _mem_loads.empty() );
        ASSERT( _mem_stores.empty() );
        _tid = tid.ptr;
        swap_critical();
    }

    void trace( vm::TraceInfo ti )
    {
        _info += heap().read_string( ti.text ) + "\n";
    }

    bool finished()
    {
        if ( !_tid.null() )
            swap_critical();
        _stack.resize( _level, vm::Choice( 0, -1 ) );
        _level = 0;
        _tid = vm::GenericPointer();
        _trace.clear();
        _assume = vm::HeapPointer();
        while ( !_stack.empty() && _stack.back().taken + 1 == _stack.back().total )
            _stack.pop_back();
        return _stack.empty();
    }
};

template< typename Solver >
struct Hasher_
{
    mutable vm::mem::CowHeap _h1, _h2;
    vm::HeapPointer _root;
    Solver *_solver = nullptr;

    void setup( const vm::mem::CowHeap &heap, Solver &solver )
    {
        _h1 = heap;
        _h2 = heap;
        _solver = &solver;
    }

    bool equal_fastpath( Snapshot a, Snapshot b ) const
    {
        bool rv = false;
        if ( _h1.snapshots().size( a ) == _h1.snapshots().size( b ) )
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
            return vm::mem::heap::compare( _h1, _h2, _root, _root ) == 0;
    }

    bool equal_symbolic( Snapshot a, Snapshot b ) const
    {
        if ( equal_fastpath( a, b ) )
            return true;

        std::vector< std::pair< vm::HeapPointer, vm::HeapPointer > > sym_pairs;

        auto extract = [&]( vm::HeapPointer a, vm::HeapPointer b )
        {
            a.type( vm::PointerType::Weak ); // unmark pointers so they are equal to their
            b.type( vm::PointerType::Weak ); // weak equivalents inside the formula
            sym_pairs.emplace_back( a, b );
        };

        if ( vm::mem::heap::compare( _h1, _h2, _root, _root, extract ) != 0 )
            return false;

        if ( sym_pairs.empty() )
            return true;

        ASSERT( _solver );
        return _solver->equal( sym_pairs, _h1, _h2 );
    }

    brick::hash::hash128_t hash( Snapshot s ) const
    {
        _h1.restore( s );
        return vm::mem::heap::hash( _h1, _root );
    }
};

template< typename Solver >
struct Hasher : Hasher_< Solver >
{
    bool equal( Snapshot a, Snapshot b ) const { return this->equal_symbolic( a, b ); }
};

template<>
struct Hasher< smt::NoSolver > : Hasher_< smt::NoSolver >
{
    bool equal( Snapshot a, Snapshot b ) const { return this->equal_explicit( a, b ); }
};

using BC = std::shared_ptr< BitCode >;

}

namespace divine::mc
{

namespace hashset = brick::hashset;

template< typename Solver >
struct Builder
{
    using PointerV = vm::value::Pointer;
    using Context = builder::Context;
    using Eval = vm::Eval< Context >;
    using Hasher = builder::Hasher< Solver >;

    using BC = builder::BC;
    using Env = std::vector< std::string >;
    using State = builder::State;
    using Snapshot = vm::mem::CowHeap::Snapshot;

    struct Label : brick::types::Ord
    {
        std::vector< std::string > trace;
        std::vector< vm::Choice > stack;
        std::vector< vm::Interrupt > interrupts;
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
    auto &debug() { return _d.bc->debug(); }
    auto &heap() { return context().heap(); }
    auto &pool() { return _d.ctx.heap().snapshots(); }

    struct Data
    {
        BC bc;
        Context ctx;
        HT states;
        builder::State initial;
        Solver solver;

        bool overwrite = false;
        int64_t instructions = 0;
        std::shared_ptr< std::atomic< int64_t > > total_instructions;

        template< typename... Args >
        Data( BC bc, Args... solver_opts )
            : Data( bc, Context( bc->program() ), HT( Hasher(), 1024 ), solver_opts... )
        {}

        template< typename... Args >
        Data( BC bc, const Context &ctx, HT states, Args... solver_opts )
            : bc( bc ), ctx( ctx ), states( states ), solver( solver_opts... ),
              total_instructions( new std::atomic< int64_t >( 0 ) )
        {}

        ~Data() { *total_instructions += instructions; }
    } _d;

    Context &context() { return _d.ctx; }
    void enable_overwrite() { _d.overwrite = true; }

    auto &hasher() { return _d.states.hasher; }

    Builder( const Builder &e ) : _d( e._d )
    {
        hasher().setup( context().heap(), _d.solver );
    }

    template< typename... Args >
    Builder( BC bc, Args && ... args ) : _d( bc, args... )
    {
        hasher().setup( context().heap(), _d.solver );
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
        vm::setup::boot( context() );
        context().track_memory( false );
        eval.run();
        hasher()._root = context().get( _VM_CR_State ).pointer;

        if ( vm::setup::postboot_check( context() ) )
            _d.initial.snap = *store( context().snapshot() );
        _d.states.updateUsage();
        if ( !context().finished() )
            UNREACHABLE( "choices encountered during start()" );
    }

    template< typename Ctx >
    Snapshot start( const Ctx &ctx, Snapshot snap )
    {
        context().load( ctx ); /* copy over registers */
        context().track_memory( false );
        hasher().setup( context().heap(), _d.solver );
        hasher()._root = context().get( _VM_CR_State ).pointer;

        if ( context().heap().valid( hasher()._root ) )
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

    bool equal( Snapshot a, Snapshot b ) { return hasher().equal( a, b ); }

    bool feasible()
    {
        if ( context().get( _VM_CR_Flags ).integer & _VM_CF_Cancel )
            return false;
        if ( context()._assume.null() )
            return true;
        return _d.solver.feasible( context().heap(), context()._assume );
    }

    template< typename Y >
    void edges( builder::State from, Y yield )
    {
        Eval eval( context() );
        ASSERT_EQ( context()._level, 0 );
        ASSERT( context()._stack.empty() );
        ASSERT( context()._trace.empty() );
        ASSERT( context()._lock.empty() );
        context()._critical.clear();

        struct Check
        {
            std::deque< vm::Choice > lock;
            Label lbl;
            Snapshot snap;
            bool free:1, feasible:1;
            vm::GenericPointer tid;
            Check() : free( false ), feasible( true ) {}
        };

        std::vector< Check > to_check;

        auto do_yield = [&]( Snapshot snap, Label lbl )
        {
            builder::State st;
            auto r = store( snap );
            st.snap = *r;

            yield( st, lbl, r.isnew() );
        };

        auto do_eval = [&]( Check &tc )
        {
            bool cont = false;
            do {
                cont = eval.run_seq( cont );
                tc.feasible = feasible();
            } while ( cont && tc.feasible );
        };

        context().track_memory( true );
        _d.solver.reset();

        do {
            context().load( from.snap );
            vm::setup::scheduler( context() );
            ASSERT_EQ( context()._level, 0 );

            to_check.emplace_back();
            auto &tc = to_check.back();

            do_eval( tc );
            _d.instructions += context()._instruction_counter;

            for ( int i = 0; i < context()._level; ++i )
                tc.lock.push_back( context()._stack[ i ] );

            if ( tc.feasible )
            {
                tc.snap = context().heap().snapshot();
                tc.free = !context().heap().is_shared( tc.snap );
                tc.lbl = label();
            }
            tc.tid = context()._tid;
        } while ( !context().finished() );

        context().track_memory( false );

        for ( auto &tc : to_check )
        {
            typename Context::MemMap l, s;

            for ( auto &c : context()._critical )
            {
                if ( tc.tid == c.first )
                    continue;
                auto &us = context()._critical[ tc.tid ];
                auto &them = c.second;
                for ( auto ol : them.loads )
                    if ( us.stores.intersect( ol.first, ol.second ) )
                        s.insert( ol.first, ol.second );
                for ( auto sl : them.stores )
                {
                    if ( us.loads.intersect( sl.first, sl.second ) )
                        l.insert( sl.first, sl.second );
                    if ( us.stores.intersect( sl.first, sl.second ) )
                        s.insert( sl.first, sl.second );
                }
            }

            if ( s.empty() && l.empty() )
            {
                if ( tc.feasible )
                    do_yield( tc.snap, tc.lbl );
            }
            else
            {
                if ( tc.feasible && tc.free )
                    pool().free( tc.snap );
                context()._lock = tc.lock;
                context().load( from.snap );
                _d.solver.reset();
                ASSERT( context()._stack.empty() );
                ASSERT_EQ( context()._level, 0 );
                context()._crit_loads = l;
                context()._crit_stores = s;
                vm::setup::scheduler( context() );

                do_eval( tc );
                ASSERT_EQ( tc.tid, context()._tid );
                _d.instructions += context()._instruction_counter;

                if ( tc.feasible )
                {
                    auto lbl = label();
                    do_yield( context().heap().snapshot(), lbl );

                    int i = 0;
                    for ( auto t : lbl.stack )
                        ASSERT( t == tc.lock[ i++ ] );
                }

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

using ExplicitBuilder = Builder< smt::NoSolver >;
using SMTLibBuilder = Builder< smt::SMTLibSolver >;

#if OPT_Z3
using Z3Builder = Builder< smt::Z3Solver >;
#endif

}

namespace divine::t_mc
{

using namespace std::literals;

namespace {

auto prog( std::string p )
{
    return t_vm::c2bc(
        "void *__vm_obj_make( int );"s +
        "void *__vm_ctl_get( int );"s +
        "void __vm_ctl_set( int, void * );"s +
        "long __vm_ctl_flag( long, long );"s +
        "void __vm_trace( int, const char * );"s +
        "int __vm_choose( int, ... );"s +
        "void __boot( void * );"s + p );
}

auto prog_int( std::string first, std::string next )
{
    return prog(
        R"(void __sched() {
            int *r = __vm_ctl_get( 5 ); __vm_trace( 0, "foo" );
            *r = )" + next + R"(;
            if ( *r < 0 ) __vm_ctl_flag( 0, 0b10000 );
            __vm_ctl_set( 2, 0 );
        }
        void __boot( void *environ ) {
            __vm_ctl_set( 4, __sched );
            void *e = __vm_obj_make( sizeof( int ) );
            __vm_ctl_set( 5, e );
            int *r = e; *r = )" + first + "; __vm_ctl_set( 2, 0 ); }"s );
}

}

struct TestBuilder
{
    TEST(instance)
    {
        auto bc = prog( "void __boot( void *e ) { __vm_ctl_flag( 0, 0b10000 ); }" );
        mc::ExplicitBuilder ex( bc );
    }

    TEST(simple)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex( bc );
        bool found = false;
        ex.start();
        ex.initials( [&]( auto ) { found = true; } );
        ASSERT( found );
    }

    TEST(hasher)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex( bc );
        ex.start();
        ex.initials( [&]( auto s )
        {
            ASSERT( ex.hasher()._h1.snapshots().size( s.snap ) );
        } );
    }

    TEST(hasher_copy)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex_( bc ), ex( ex_ );
        ex.start();
        ex.initials( [&]( auto s )
        {
            ASSERT( ex.hasher()._h1.snapshots().size( s.snap ) );
            ASSERT( ex_.hasher()._h1.snapshots().size( s.snap ) );
        } );
    }

    TEST(start_twice)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex( bc );
        mc::builder::State i1, i2;
        ex.start();
        ex.initials( [&]( auto s ) { i1 = s; } );
        ex.start();
        ex.initials( [&]( auto s ) { i2 = s; } );
        ASSERT( ex.equal( i1.snap, i2.snap ) );
    }

    TEST(copy)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex1( bc ), ex2( ex1 );
        mc::builder::State i1, i2;
        ex1.start();
        ex2.start();
        ex1.initials( [&]( auto i ) { i1 = i; } );
        ex2.initials( [&]( auto i ) { i2 = i; } );
        ASSERT( ex1.equal( i1.snap, i2.snap ) );
        ASSERT( ex2.equal( i1.snap, i2.snap ) );
    }

    TEST(succ)
    {
        auto bc = prog_int( "4", "*r - 1" );
        mc::ExplicitBuilder ex( bc );
        ex.start();
        mc::builder::State s1, s2;
        ex.initials( [&]( auto s ) { s1 = s; } );
        ex.edges( s1, [&]( auto s, auto, bool ) { s2 = s; } );
        ASSERT( !ex.equal( s1.snap, s2.snap ) );
    }

    void _search( std::shared_ptr< mc::BitCode > bc, int sc, int ec )
    {
        mc::ExplicitBuilder ex( bc );
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
