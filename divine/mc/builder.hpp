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
#include <divine/mc/hasher.hpp>
#include <divine/mc/context.hpp>
#include <divine/smt/solver.hpp>
#include <divine/vm/value.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/context.hpp>
#include <divine/vm/setup.hpp>
#include <divine/vm/eval.hpp>

#include <set>
#include <memory>

namespace divine::mc::builder
{

using namespace std::literals;

struct State
{
    Snapshot snap;
    bool operator==( const State &o ) const { return snap.intptr() == o.snap.intptr(); }
    bool operator!=( const State &o ) const { return !(*this == o); }
};

using BC = std::shared_ptr< BitCode >;

}

namespace divine::mc
{

template< typename Solver >
struct Builder
{
    using PointerV = vm::value::Pointer;
    using Context = mc::Context;
    using Eval = vm::Eval< Context >;
    using Hasher = mc::Hasher< Solver >;

    using BC = builder::BC;
    using Env = std::vector< std::string >;
    using State = builder::State;
    using Snapshot = vm::CowHeap::Snapshot;

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

    using HT = brq::concurrent_hash_set< Snapshot >;

    auto &program() { return _d.bc->program(); }
    auto &debug() { return _d.bc->debug(); }
    auto &heap() { return context().heap(); }
    auto &pool() { return _d.pool; }

    struct Data
    {
        BC bc;
        Context ctx;
        HT states;
        builder::State initial;
        Solver solver;
        vm::CowHeap::Pool pool;

        int64_t local_instructions = 0, local_states = 0;
        std::shared_ptr< std::atomic< int64_t > > total_instructions, total_states;

        template< typename... Args >
        Data( BC bc, Args... solver_opts )
            : Data( bc, Context( bc->program() ), HT(), solver_opts... )
        {}

        template< typename... Args >
        Data( BC bc, const Context &ctx, HT states, Args... solver_opts )
            : bc( bc ), ctx( ctx ), states( states ), solver( solver_opts... ),
              total_instructions( new std::atomic< int64_t >( 0 ) ),
              total_states( new std::atomic< int64_t >( 0 ) )
        {}

        void sync()
        {
            *total_instructions += local_instructions;
            *total_states += local_states;
            local_instructions = local_states = 0;
        }

        ~Data() { sync(); }
    } _d;
    Hasher _hasher;

    Context &context() { return _d.ctx; }
    void enable_overwrite() { _hasher.overwrite = true; }

    auto &hasher() { return _hasher; }

    Builder( const Builder &e ) : _d( e._d ), _hasher( e._hasher, _d.pool, _d.solver )
    {}

    template< typename... Args >
    Builder( BC bc, Args && ... args ) : _d( bc, args... ), _hasher( _d.pool, _d.ctx.heap(), _d.solver )
    {}

    std::pair< Snapshot, bool > store( Snapshot snap )
    {
        _hasher.prepare( snap );
        auto r = _d.states.insert( snap, hasher() );
        if ( r->load() != snap )
        {
            heap().snap_put( pool(), snap );
            context().load( pool(), *r );
            return { *r, false };
        }
        else
        {
            ++ _d.local_states;
            context().flush_ptr2i();
            return { snap, true };
        }
    }

    void start()
    {
        Eval eval( context() );
        vm::setup::boot( context() );
        context().track_memory( false );
        eval.run();
        hasher()._root = context().state_ptr();
        hasher()._path = context().constraint_ptr();

        auto s = context().snapshot( pool() );
        if ( vm::setup::postboot_check( context() ) )
            std::tie( _d.initial.snap, std::ignore ) = store( s );
        _d.sync();
        if ( !context().finished() )
            UNREACHABLE( "choices encountered during start()" );
    }

    template< typename Ctx >
    Snapshot start( const Ctx &ctx, Snapshot snap )
    {
        context().load( ctx ); /* copy over registers */
        context().track_memory( false );
        hasher()._h1 = ctx.heap();
        hasher()._h2 = ctx.heap();
        hasher()._root = context().state_ptr();
        hasher()._path = context().constraint_ptr();

        if ( context().heap().valid( hasher()._root ) )
            std::tie( _d.initial.snap, std::ignore ) = store( snap );
        _d.sync();
        if ( !context().finished() )
            UNREACHABLE( "choices encountered during start()" );
        return _d.initial.snap;
    }

    Label label()
    {
        Label lbl;
        lbl.trace = context()._trace;
        lbl.stack = context()._stack;
        lbl.accepting = context().flags_any( _VM_CF_Accepting );
        lbl.error = context().flags_any( _VM_CF_Error );
        lbl.interrupts = context()._interrupts;
        return lbl;
    }

    bool equal( Snapshot a, Snapshot b ) { return hasher().equal_symbolic( a, b ); }

    bool feasible()
    {
        if ( context().flags_any( _VM_CF_Cancel ) )
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
            bool feasible:1;
            vm::GenericPointer tid;
            Check() : feasible( true ) {}
        };

        std::vector< Check > to_check;

        auto do_yield = [&]( Snapshot snap, Label lbl )
        {
            builder::State st;
            bool isnew;

            std::tie( st.snap, isnew ) = store( snap );
            yield( st, lbl, isnew );
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
            context().load( pool(), from.snap );
            vm::setup::scheduler( context() );
            ASSERT_EQ( context()._level, 0 );

            to_check.emplace_back();
            auto &tc = to_check.back();

            do_eval( tc );
            _d.local_instructions += context().instruction_count();

            for ( int i = 0; i < context()._level; ++i )
                tc.lock.push_back( context()._stack[ i ] );

            if ( tc.feasible )
            {
                tc.snap = context().heap().snapshot( pool() );
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
                if ( tc.feasible )
                    context().heap().snap_put( pool(), tc.snap );
                context()._lock = tc.lock;
                context().load( pool(), from.snap );
                _d.solver.reset();
                ASSERT( context()._stack.empty() );
                ASSERT_EQ( context()._level, 0 );
                context()._crit_loads = l;
                context()._crit_stores = s;
                vm::setup::scheduler( context() );

                do_eval( tc );
                ASSERT_EQ( tc.tid, context()._tid );
                _d.local_instructions += context().instruction_count();

                if ( tc.feasible )
                {
                    auto lbl = label();
                    do_yield( context().heap().snapshot( pool() ), lbl );

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

#if OPT_STP
using STPBuilder = Builder< smt::STPSolver >;
#endif

}
