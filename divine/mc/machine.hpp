// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2019 Petr Ročkai <code@fixp.eu>
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
#include <divine/mc/context.hpp>
#include <divine/mc/hasher.hpp>
#include <divine/mc/search.hpp>
#include <divine/smt/solver.hpp>

#include <divine/vm/value.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/setup.hpp>
#include <divine/vm/eval.tpp> // FIXME

#include <brick-compose>

#include <set>
#include <memory>
#include <random>

namespace divine::mc
{
    using namespace std::literals;
    namespace ctx = vm::ctx;
    using BC = std::shared_ptr< BitCode >;
    using HT = brq::concurrent_hash_set< Snapshot >;

    struct State
    {
        Snapshot snap;
        explicit State( Snapshot s = Snapshot() ) : snap( s ) {}
        bool operator==( const State& o ) const { return snap.intptr() == o.snap.intptr(); }
    };

    struct Label
    {
        bool accepting:1;
        bool error:1;
    };

    struct TracingLabel : Label, brick::types::Ord
    {
        std::vector< std::string > trace;
        std::vector< vm::Choice > choices;
        std::vector< vm::Interrupt > interrupts;
        auto as_tuple() const
        {
            /* skip the text trace for comparison purposes */
            return std::make_tuple( choices, interrupts, accepting, error );
        }
    };

    struct MContext : vm::Context< vm::Program, vm::CowHeap >
    {
        int _choice_take, _choice_count;
        vm::GenericPointer _assume;

        template< typename I >
        int choose( int count, I, I )
        {
            if ( _choice_count )
                ASSERT_LT( _choice_take, _choice_count );
            _choice_count = count;
            return _choice_take;
        }

        using vm::Context< vm::Program, vm::CowHeap >::Context;
    };

    using StateTQ = GraphTQ< State, Label >;

    struct Origin
    {
        Snapshot snap;
        HT ht_loop;
        explicit Origin( Snapshot s ) : snap( s ) {}
    };

    struct Task
    {
        using GraphTs = StateTQ::Tasks;
        using Boot = GraphTs::Start;
        using Schedule = GraphTs::Expand;

        struct Compute
        {
            Snapshot snap;
            Origin origin;
            vm::State state;
            Compute( Snapshot snap, Origin orig, vm::State state )
                : snap( snap ), origin( orig ), state( state )
            {}
        };

        struct Choose : Compute
        {
            int choice, total;
            Choose( Snapshot snap, Origin orig, vm::State state, int c, int t )
                : Compute( snap, orig, state ), choice( c ), total( t )
            {}
        };

        using TQ = StateTQ::Extend< Task, Choose >;
        using Weaver = mc::Weaver< TQ >;
    };

    using TQ = Task::TQ;
}

namespace divine::mc::machine
{
    struct with_pools
    {
        using Pool = vm::CowHeap::Pool;

        Pool _snap_pool, _state_pool;
        brick::mem::RefPool< Pool, uint8_t > _snap_refcnt;

        with_pools() : _snap_refcnt( _snap_pool ) {}
    };

    struct with_counters
    {
        std::shared_ptr< std::atomic< int64_t > > _total_instructions;

        with_counters()
            : _total_instructions( new std::atomic< int64_t >( 0 ) )
        {}
    };

    struct with_bc
    {
        using Env = std::vector< std::string >;
        BC _bc;

        void bc( BC bc ) { _bc = bc; }
        auto &program() { return _bc->program(); }
        auto &debug() { return _bc->debug(); }
    };

    template< typename solver_ >
    struct with_solver
    {
        using solver_t = solver_;
        solver_t _solver;
        solver_t &solver();
    };

    template< typename solver, typename next >
    struct base_ : TQ::Skel, with_pools, with_counters, with_bc, with_solver< solver >, next
    {
        using task_choose   = Task::Choose;
        using task_schedule = Task::Schedule;
        using task_edge     = StateTQ::Tasks::Edge;

        void boot( TQ & ) {}
    };

    template< typename next >
    struct choose_ : next
    {
        void queue_choices( TQ &tq, Snapshot snap, Origin origin, vm::State state, int take, int total )
        {
            for ( int i = 0; i < total; ++i )
                if ( i != take )
                {
                    this->_snap_refcnt.get( snap );
                    tq.add< typename next::task_choose >( snap, origin, state, i, total );
                }
        }

        int select_choice( TQ &tq, Snapshot snap, Origin origin, vm::State state, int count )
        {
            queue_choices( tq, snap, origin, state, 0, count );
            return 0;
        }
    };

    template< typename next >
    struct choose_random_ : next
    {
        std::mt19937 rand;

        int select_choice( TQ &tq, Snapshot snap, Origin origin, vm::State state, int count )
        {
            using dist = std::uniform_int_distribution< int >;
            int c = dist( 0, count - 1 )( rand );
            this->queue_choices( tq, snap, origin, state, c, count );
        }
    };

    template< typename ctx, typename next >
    struct with_context_ : next
    {
        using context_t = ctx;
        ctx _ctx;
        ctx &context() { return _ctx; }
        auto &heap() { return context().heap(); }

        void update_instructions()
        {
            *this->_total_instructions += context().instruction_count();
            context().instruction_count( 0 );
        }

        ~with_context_()
        {
            update_instructions();
        }
    };

    template< typename next >
    struct compute_ : next
    {
        using Eval = vm::Eval< typename next::context_t >;

        void eval_choice( TQ &tq, Origin origin )
        {
            auto snap = this->context().snapshot( this->_snap_pool );
            auto state = this->context()._state;

            this->_snap_refcnt.get( snap );

            Eval eval( this->context() );
            this->context()._choice_take = this->context()._choice_count = 0;
            eval.refresh();
            eval.dispatch();

            this->context()._choice_take =
                this->select_choice( tq, snap, origin, state, this->context()._choice_count );
            compute( tq, origin, snap );
        }

        void eval_interrupt( TQ &tq, Origin origin, Snapshot cont_from )
        {
            if ( this->context().frame().null() )
            {
                Label lbl;
                lbl.accepting = this->context().flags_any( _VM_CF_Accepting );
                lbl.error = this->context().flags_any( _VM_CF_Error );
                auto [ state, isnew ] = this->store();
                tq.add< typename next::task_edge >( State( origin.snap ), state, lbl, isnew );
            }
            else if ( this->loop_closed( origin ) )
            {
                /* TODO: check for error/accepting flags */
            }
            else
                return compute( tq, origin, cont_from );
        }

        bool feasible()
        {
            if ( this->context().flags_any( _VM_CF_Cancel ) )
                return false;
            if ( this->context()._assume.null() )
                return true;

            return this->_solver.feasible( this->context().heap(), this->context()._assume );
        }

        void compute( TQ &tq, Origin origin, Snapshot cont_from = Snapshot() )
        {
            auto destroy = [&]( auto p ) { this->heap().snap_put( this->_snap_pool, p, false ); };
            auto cleanup = [&]
            {
                if ( cont_from )
                    this->_snap_refcnt.put( cont_from, destroy );
            };

            brick::types::Defer _( cleanup );
            Eval eval( this->context() );
            bool choice = eval.run_seq( !!cont_from );

            if ( !feasible() )
                return;

            if ( choice )
                eval_choice( tq, origin );
            else
                eval_interrupt( tq, origin, cont_from );
        }

        bool boot( TQ &tq )
        {
            this->context().program( this->program() );
            Eval eval( this->context() );
            vm::setup::boot( this->context() );
            eval.run();
            next::boot( tq );

            if ( vm::setup::postboot_check( this->context() ) )
                return tq.add< typename next::task_schedule >( this->store().first ), true;
            else
                return false;
        }

        void schedule( TQ &tq, Task::Schedule e )
        {
            TRACE( "task (schedule)", e );
            this->context().load( this->_state_pool, e.from.snap );
            vm::setup::scheduler( this->context() );
            compute( tq, Origin( e.from.snap ) );
        }

        void choose( TQ &tq, Task::Choose c )
        {
            TRACE( "task (choose)", c );
            if ( !c.total )
                return;
            this->context().load( this->_snap_pool, c.snap );
            this->context()._state = c.state;
            this->context()._choice_take = c.choice;
            this->context()._choice_count = c.total;
            this->context().flush_ptr2i();
            this->context().load_pc();
            compute( tq, c.origin, c.snap );
        }
    };

    template< typename next >
    struct tree_store_ : next
    {
        virtual std::pair< State, bool > store()
        {
            return { State( this->context().snapshot( this->_state_pool ) ), true };
        }

        virtual bool loop_closed( Origin & )
        {
            return false;
        }

        auto make_hasher()
        {
            Hasher h( this->_state_pool, this->context().heap(), this->_solver );
            h._root = this->context().state_ptr();
            ASSERT( !h._root.null() );
            return h;
        }

        bool equal( Snapshot a, Snapshot b )
        {
            return make_hasher().equal_symbolic( a, b );
        }

        ~tree_store_()
        {
            ASSERT_EQ( this->_snap_pool.stats().total.count.used, 0 );
        }

    };

    template< typename next >
    struct graph_store_ : next
    {
        using Hasher = mc::Hasher< typename next::solver_t >;

        Hasher _hasher;
        HT _ht_sched;

        auto &hasher() { return _hasher; }
        bool equal( Snapshot a, Snapshot b ) { return hasher().equal_symbolic( a, b ); }
        graph_store_() : _hasher( this->_state_pool, this->heap(), this->_solver ) {}

        std::pair< State, bool > store( HT &table )
        {
            auto snap = this->context().snapshot( this->_state_pool );
            auto r = table.insert( snap, hasher() );
            if ( r->load() != snap )
            {
                ASSERT( !_hasher.overwrite );
                ASSERT( !r.isnew() );
                this->heap().snap_put( this->_state_pool, snap );
            }
            return { State( *r ), r.isnew() };
        }

        std::pair< State, bool > store()
        {
            return store( _ht_sched );
        }

        bool loop_closed( Origin &origin )
        {
            auto [ state, isnew ] = store( origin.ht_loop );
            if ( isnew )
                this->context().flush_ptr2i();
            else
            {
                this->context().load( this->_state_pool, state.snap );
                TRACE( "loop closed at", state.snap );
            }

            return !isnew;
        }

        void boot( TQ &tq )
        {
            next::boot( tq );
            hasher()._root = this->context().state_ptr();
        }
    };

    template< typename next >
    struct dispatch_ : next
    {
        using next::run;

        void run( TQ &tq, Task::Boot ) { next::boot( tq ); }
        void run( TQ &tq, Task::Schedule e ) { next::schedule( tq, e ); }
        void run( TQ &tq, Task::Choose c ) { next::choose( tq, c ); }
    };

    template< typename solver >
    using base = brq::module< base_, solver >;

    template< typename ctx >
    using with_context = brq::module< with_context_, ctx >;

    using compute = brq::module< compute_ >;
    using choose = brq::module< choose_ >;
    using choose_random = brq::module< choose_random_ >;
    using tree_store = brq::module< tree_store_ >;
    using graph_store = brq::module< graph_store_ >;
    using dispatch = brq::module< dispatch_ >;

    template< typename solver >
    using tree = brq::compose_stack< dispatch, compute, tree_store, choose,
                                     with_context< context >, base< solver > >;

    template< typename solver >
    using graph = brq::compose_stack< dispatch, compute, graph_store, choose,
                                      with_context< context >, base< solver > >;
}

namespace divine::mc
{
    using TMachine = machine::tree< smt::NoSolver >;
    using GMachine = machine::graph< smt::NoSolver >;

    template< typename M >
    auto weave( M &machine )
    {
        return mc::Task::Weaver().extend_ref( machine );
    }
}
