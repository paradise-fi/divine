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
    void trace( std::string s ) { _trace.push_back( s ); }

    void trace( TraceSchedInfo ) {} /* noop */
    void trace( TraceSchedChoice ) {} /* noop */
    void trace( TraceStateType ) {}
    void trace( TraceInfo ti )
    {
        _info += heap().read_string( ti.text ) + "\n";
    }
    void trace( TraceTypeAlias ) {}
    void trace( TraceDebugPersist t ) { Super::trace( t ); }

    bool finished()
    {
        _level = 0;
        _trace.clear();
        while ( !_stack.empty() && _stack.back().taken + 1 == _stack.back().total )
            _stack.pop_back();
        return _stack.empty();
    }
};

template< typename Solver >
struct SymbolicContext : Context {
    using Context::Context;
    using SolverPtr = std::shared_ptr< Solver >;

    SymbolicContext( Program &program, SolverPtr solver )
        : Context( program ), _solver( solver )
    {}

    // note: for some reason compiler does not accept out-of-line definition of
    // trace here, so defer it to extra, non-overloaded version
    using Context::trace;
    void trace( TraceAlg ta ) { return traceAlg( ta ); }
    void traceAlg( TraceAlg ta )
    {
        ASSERT_EQ( ta.args.size(), 1 );

        if ( _solver->feasible( heap(), ta.args[0] ) == Solver::Result::False )
            set( _VM_CR_Flags, get( _VM_CR_Flags ).integer | _VM_CF_Cancel );
    }

    SolverPtr _solver;
};

struct Hasher
{
    using Snapshot = CowHeap::Snapshot;

    mutable CowHeap h1, h2;
    HeapPointer root;

    Hasher( const CowHeap &heap )
        : h1( heap ), h2( heap )
    {}

    bool equalFastpath( Snapshot a, Snapshot b ) const {
        if ( h1._snapshots.size( a ) == h1._snapshots.size( b ) )
            return std::equal( h1.snap_begin( a ), h1.snap_end( a ), h1.snap_begin( b ) );
        return false;
    }

    bool equal( Snapshot a, Snapshot b ) const
    {
        if ( equalFastpath( a, b ) )
            return true;
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

template< typename Solver >
struct SymbolicHasher : Hasher {
    using SolverPtr = std::shared_ptr< Solver >;

    SymbolicHasher( const CowHeap &heap, SolverPtr solver )
        : Hasher( heap ), _solver( solver )
    {
    }

    using SymPairs = std::vector< std::pair< HeapPointer, HeapPointer > >;

    bool equal( Snapshot a, Snapshot b ) const {
        if ( equalFastpath( a, b ) )
            return true;

        h1.restore( a );
        h2.restore( b );

        SymPairs sym_pairs;
        auto sym_pairs_extract = [&]( HeapPointer a, HeapPointer b ) {
            a.type( PointerType::Weak ); // unmark pointers so they are equal to their
            b.type( PointerType::Weak ); // weak equivalents inside the formula
            sym_pairs.emplace_back( a, b );
        };
        if ( heap::compare( h1, h2, root, root, sym_pairs_extract ) != 0 )
            return false;

        if ( sym_pairs.empty() )
            return true;

        return _solver->equal( sym_pairs, h1, h2 ) == Solver::Result::True;
    }

    SolverPtr _solver;
};

using BC = std::shared_ptr< BitCode >;

} // namespace explore

template< typename Hasher, typename Context >
struct Explore_
{
    using PointerV = value::Pointer;
    using Eval = vm::Eval< Context, value::Void >;

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

    BC _bc;

    Context _ctx;

    using HT = hashset::Concurrent< Snapshot, Hasher >;
    HT _states;
    explore::State _initial;
    bool _overwrite = false;

    auto &program() { return _bc->program(); }
    auto &pool() { return _ctx.heap()._snapshots; }

    Explore_( BC bc )
        : _bc( bc ), _ctx( _bc->program() ), _states( _ctx.heap(), 1024 )
    {
    }

    Explore_( BC bc, Context ctx, HT states )
        : _bc( bc ), _ctx( ctx ), _states( states )
    {
    }

    auto store( Snapshot snap )
    {
        auto r = _states.insert( snap, _overwrite );
        if ( *r != snap )
        {
            ASSERT( !_overwrite );
            pool().free( snap ), _ctx.load( *r );
        }
        else
            _ctx.flush_ptr2i();
        return r;
    }

    void start()
    {
        Eval eval( _ctx );
        setup::boot( _ctx );
        eval.run();
        _states.hasher.root = _ctx.get( _VM_CR_State ).pointer;
        if ( setup::postboot_check( _ctx ) )
            _initial.snap = *store( _ctx.snapshot() );
        _states.updateUsage();
        if ( !_ctx.finished() )
            UNREACHABLE( "choices encountered during start()" );
    }

    template< typename Ctx >
    Snapshot start( const Ctx &ctx, Snapshot snap )
    {
        _ctx.load( ctx ); /* copy over registers */
        _states.hasher = Hasher( _ctx.heap() );
        _states.hasher.root = _ctx.get( _VM_CR_State ).pointer;
        if ( _ctx.heap().valid( _states.hasher.root ) )
            _initial.snap = *store( snap );
        _states.updateUsage();
        if ( !_ctx.finished() )
            UNREACHABLE( "choices encountered during start()" );
        return _initial.snap;
    }

    Label label()
    {
        Label lbl;
        lbl.trace = _ctx._trace;
        lbl.stack = _ctx._stack;
        lbl.accepting = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Accepting;
        lbl.error = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Error;
        lbl.interrupts = _ctx._interrupts;
        return lbl;
    }

    template< typename Y >
    void edges( explore::State from, Y yield )
    {
        Eval eval( _ctx );
        ASSERT_EQ( _ctx._level, 0 );
        ASSERT( _ctx._stack.empty() );
        ASSERT( _ctx._trace.empty() );
        ASSERT( _ctx._lock.empty() );

        struct Check
        {
            std::deque< Choice > lock;
            typename Context::MemMap loads, stores;
            Label lbl;
            Snapshot snap;
            bool free;
        };

        std::vector< Check > to_check;

        _ctx._crit_loads.clear();
        _ctx._crit_stores.clear();

        auto do_yield = [&]( Snapshot snap, Label lbl )
        {
            explore::State st;
            auto r = store( snap );
            st.snap = *r;

            yield( st, lbl, r.isnew() );
        };

        do {
            _ctx.load( from.snap );
            setup::scheduler( _ctx );
            eval.run();
            if ( !( _ctx.get( _VM_CR_Flags ).integer & _VM_CF_Cancel ) )
            {
                to_check.emplace_back();
                auto &tc = to_check.back();
                tc.loads = _ctx._mem_loads;
                tc.stores = _ctx._mem_stores;

                for ( auto c : _ctx._stack )
                    tc.lock.push_back( c );

                tc.snap = _ctx.heap().snapshot();
                tc.free = !_ctx.heap().is_shared( tc.snap );
                tc.lbl = label();
            }
        } while ( !_ctx.finished() );

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
                _ctx._lock = tc.lock;
                _ctx.load( from.snap );
                ASSERT( _ctx._stack.empty() );
                ASSERT_EQ( _ctx._level, 0 );
                _ctx._crit_loads = l;
                _ctx._crit_stores = s;
                setup::scheduler( _ctx );

                eval.run(); /* will not cancel since this is a prefix */
                auto lbl = label();
                do_yield( _ctx.heap().snapshot(), lbl );

                int i = 0;
                for ( auto t : lbl.stack )
                    ASSERT( t == tc.lock[ i++ ] );

                _ctx._stack.clear();
                _ctx._lock.clear();
                _ctx.finished();
            }
        }
    }

    template< typename Y >
    void initials( Y yield )
    {
        if ( _initial.snap.slab() ) /* fixme, better validity check */
            yield( _initial );
    }
};

template< typename Solver_,
          template< typename > class Hasher_,
          template< typename > class Context_ >
struct SymbolicExplore_ : Explore_< Hasher_< Solver_ > , Context_< Solver_ > >
{
    using Solver = Solver_;
    using Hasher = Hasher_< Solver >;
    using Context = Context_< Solver >;
    using Super = Explore_< Hasher, Context >;
    using BC = explore::BC;
    using Snapshot = CowHeap::Snapshot;
    using HashTable = hashset::Concurrent< Snapshot, Hasher >;
    using SolverPtr = std::shared_ptr< Solver >;

    SymbolicExplore_( BC bc )
        : SymbolicExplore_( bc, std::make_shared< Solver >() )
    {
    }

private:
    SymbolicExplore_( BC bc, Context ctx, SolverPtr solver )
        : Super( bc, ctx, HashTable( Hasher( ctx.heap(), solver ), 1024 ) ),
          _solver( solver )
    {
    }

    SymbolicExplore_( BC bc, SolverPtr solver )
        : SymbolicExplore_( bc, Context( bc->program(), solver ), solver )
    {
    }

    SolverPtr _solver;
};

using Explore = Explore_< explore::Hasher, explore::Context >;

template< typename Solver >
using SymbolicExplore = SymbolicExplore_< Solver, explore::SymbolicHasher, explore::SymbolicContext >;

using Z3Explore = SymbolicExplore< Z3SMTLibSolver >;
using BoolectorExplore = SymbolicExplore< BoolectorSMTLib >;

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
        vm::Explore ex( bc );
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
