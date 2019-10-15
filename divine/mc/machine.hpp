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
#include <divine/mc/ctx-assume.hpp>
#include <divine/smt/solver.hpp>

#include <divine/vm/value.hpp>
#include <divine/vm/memory.tpp>
#include <divine/vm/setup.hpp>
#include <divine/vm/eval.hpp>

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

    template< typename next >
    struct ctx_choice_ : next
    {
        int _choice_take, _choice_count;

        template< typename I >
        int choose( int count, I, I )
        {
            if ( _choice_count )
                ASSERT_LT( _choice_take, _choice_count );
            _choice_count = count;
            return _choice_take;
        }
    };

    using ctx_choice = brq::module< ctx_choice_ >;

    /* TODO get rid of the debug */
    using context = brq::compose_stack< ctx_choice, ctx_assume, ctx::legacy, ctx::fault, ctx::debug,
                                        ctx::with_tracking, ctx::common< vm::Program, vm::CowHeap > >;
}

namespace divine::mc::task
{
    using boot = start;

    template< typename ctx_t >
    struct boot_sync : base
    {
        using pool = vm::CowHeap::Pool;
        pool snap_pool, state_pool;
        ctx_t ctx;
        boot_sync( pool a, pool b, ctx_t c ) : state_pool( a ), snap_pool( b ), ctx( c ) {}
    };

    struct origin
    {
        Snapshot snap;
        HT ht_loop;
        explicit origin( Snapshot s ) : snap( s ) {}
    };

    struct compute : base
    {
        Snapshot snap;
        task::origin origin;
        vm::State state;
        compute( Snapshot snap, task::origin orig, vm::State state )
            : snap( snap ), origin( orig ), state( state )
        {}
    };

    struct choose : compute
    {
        int choice, total;
        void cancel() { total = 0; }
        bool valid() { return total; }
        choose( Snapshot snap, task::origin orig, vm::State state, int c, int t )
            : compute( snap, orig, state ), choice( c ), total( t )
        {}
    };
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
    struct base_ : with_pools, with_counters, with_bc, with_solver< solver >, next
    {
        using origin        = task::origin;
        using task_choose   = task::choose;
        using task_schedule = task::Expand< State >;
        using task_edge     = task::Edge< State, Label >;
        using tq            = task_queue< task_choose, task::start, task_schedule, task_edge >;

        void boot( tq & ) {}

        template< typename ctx_t >
        void run( tq &, const task::boot_sync< ctx_t > &bs )
        {
            this->_snap_pool = bs.snap_pool;
            this->_state_pool = bs.state_pool;
        }

        void queue_edge( tq &q, const origin &from, State to, Label lbl, bool isnew )
        {
            push< task_edge >( q, State( from.snap ), to, lbl, isnew );
        }

        void queue_choice( tq &q, const origin &o, Snapshot snap, vm::State state, int i, int t )
        {
            push< task_choose >( q, snap, o, state, i, t );
        }
    };

    template< typename next >
    struct choose_ : next
    {
        using typename next::origin;
        using typename next::tq;

        void queue_choices( tq &q, origin o, Snapshot snap, vm::State state, int take, int total )
        {
            for ( int i = 0; i < total; ++i )
                if ( i != take )
                {
                    this->_snap_refcnt.get( snap );
                    this->queue_choice( q, o, snap, state, i, total );
                }
        }

        int select_choice( tq &q, origin o, Snapshot snap, vm::State state, int count )
        {
            queue_choices( q, o, snap, state, 0, count );
            return 0;
        }
    };

    template< typename next >
    struct choose_random_ : next
    {
        using typename next::origin;
        using typename next::tq;
        std::mt19937 rand;

        int select_choice( tq &q, origin o, Snapshot snap, vm::State state, int count )
        {
            using dist = std::uniform_int_distribution< int >;
            int c = dist( 0, count - 1 )( rand );
            this->queue_choices( q, o, snap, state, c, count );
            return c;
        }
    };

    template< typename ctx, typename next >
    struct with_context_ : next
    {
        using tq = task_queue_extend< typename next::tq, task::boot_sync< ctx > >;
        using context_t = ctx;
        ctx _ctx;
        ctx &context() { return _ctx; }
        auto &heap() { return context().heap(); }

        using next::run;

        template< typename ctx_t >
        void run( typename next::tq &q, const task::boot_sync< ctx_t > &bs )
        {
            this->_ctx = bs.ctx;
            next::run( q, bs );
        }

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
        using typename next::origin;
        using typename next::tq;
        using Eval = vm::Eval< typename next::context_t >;

        void eval_choice( tq &q, origin o )
        {
            auto snap = this->context().snapshot( this->_snap_pool );
            auto state = this->context()._state;

            this->_snap_refcnt.get( snap );

            Eval eval( this->context() );
            this->context()._choice_take = this->context()._choice_count = 0;
            eval.refresh();
            eval.dispatch();

            this->context()._choice_take =
                this->select_choice( q, o, snap, state, this->context()._choice_count );
            compute( q, o, snap );
        }

        void eval_interrupt( tq &q, origin o, Snapshot cont_from )
        {
            if ( this->context().frame().null() )
            {
                Label lbl;
                lbl.accepting = this->context().flags_any( _VM_CF_Accepting );
                lbl.error = this->context().flags_any( _VM_CF_Error );
                auto [ state, isnew ] = this->store();
                this->queue_edge( q, o, state, lbl, isnew );
            }
            else if ( this->loop_closed( o ) )
            {
                /* TODO: check for error/accepting flags */
            }
            else
                return compute( q, o, cont_from );
        }

        virtual bool feasible( tq & )
        {
            if ( this->context().flags_any( _VM_CF_Cancel ) )
                return false;
            if ( this->context()._assume.null() )
                return true;

            return this->_solver.feasible( this->context().heap(), this->context()._assume );
        }

        void snap_put( Snapshot s )
        {
            auto destroy = [&]( auto p, int cnt )
            {
                if ( cnt == 0 )
                    this->heap().snap_put( this->_snap_pool, p );
                return false;
            };

            this->_snap_refcnt.put( s, destroy );
        }

        void compute( tq &q, origin o, Snapshot cont_from = Snapshot() )
        {
            auto cleanup = [&]
            {
                if ( cont_from )
                    this->snap_put( cont_from );
            };

            brick::types::Defer _( cleanup );
            Eval eval( this->context() );
            bool choice = eval.run_seq( !!cont_from );

            if ( !feasible( q ) )
                return;

            if ( choice )
                eval_choice( q, o );
            else
                eval_interrupt( q, o, cont_from );
        }

        bool boot( tq &q )
        {
            this->context().program( this->program() );
            Eval eval( this->context() );
            vm::setup::boot( this->context() );
            eval.run();
            next::boot( q );

            if ( vm::setup::postboot_check( this->context() ) )
            {
                auto snap = this->store().first;
                push< task::boot_sync< typename next::context_t > >
                    ( q, this->_state_pool, this->_snap_pool, this->_ctx );
                push< typename next::task_schedule >( q, snap );
                return true;
            }
            else
                return false;
        }

        void schedule( tq &q, typename next::origin o )
        {
            this->context().load( this->_state_pool, o.snap );
            vm::setup::scheduler( this->context() );
            compute( q, o );
        }

        void choose( tq &q, typename next::task_choose c )
        {
            this->context().load( this->_snap_pool, c.snap );
            this->context()._state = c.state;
            this->context()._choice_take = c.choice;
            this->context()._choice_count = c.total;
            this->context().flush_ptr2i();
            this->context().load_pc();
            compute( q, c.origin, c.snap );
        }
    };

    template< typename next >
    struct tree_search_ : next
    {
        virtual std::pair< State, bool > store()
        {
            return { State( this->context().snapshot( this->_state_pool ) ), true };
        }

        virtual bool loop_closed( typename next::origin & )
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

        ~tree_search_()
        {
            this->_snap_pool.sync();
            ASSERT_LEQ( this->_snap_pool.stats().total.count.used, 1 );
        }

    };

    template< typename next >
    struct graph_search_ : next
    {
        using typename next::origin;
        using Hasher = mc::Hasher< typename next::solver_t >;

        Hasher _hasher;
        HT _ht_sched;

        auto &hasher() { return _hasher; }
        bool equal( Snapshot a, Snapshot b ) { return hasher().equal_symbolic( a, b ); }
        graph_search_() : _hasher( this->_state_pool, this->heap(), this->_solver ) {}

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

        bool loop_closed( origin &o )
        {
            auto [ state, isnew ] = store( o.ht_loop );
            if ( isnew )
                this->context().flush_ptr2i();
            else
            {
                this->context().load( this->_state_pool, state.snap );
                TRACE( "loop closed at", state.snap );
            }

            return !isnew;
        }

        void boot( typename next::tq &q )
        {
            next::boot( q );
            hasher()._root = this->context().state_ptr();
        }
    };

    template< typename next >
    struct search_dispatch_ : next
    {
        using typename next::tq;
        using next::run;

        void run( tq &q, task::boot )
        {
            next::boot( q );
        }

        void run( tq &q, typename next::task_choose c ) { next::choose( q, c ); }

        void run( tq &q, typename next::task_schedule e )
        {
            next::schedule( q, task::origin( e.from.snap ) );
        }
    };

    template< typename solver >
    struct base : brq::module< base_, solver > {};

    template< typename ctx >
    struct with_context : brq::module< with_context_, ctx > {};

    template< typename solver >
    using common = brq::compose< with_context< context >, base< solver > >;

    using choose = brq::module< choose_ >;
    using choose_random = brq::module< choose_random_ >;

    using tree_search = brq::module< tree_search_ >;
    using graph_search = brq::module< graph_search_ >;

    using compute = brq::module< compute_ >;
    using search_dispatch = brq::module< search_dispatch_ >;

    using tree_compute  = brq::compose< search_dispatch, compute,  tree_search, choose >;
    using graph_compute = brq::compose< search_dispatch, compute, graph_search, choose >;

    template< typename solver >
    using tree = brq::compose_stack< tree_compute, common< solver > >;

    template< typename solver >
    using graph = brq::compose_stack< graph_compute, common< solver > >;
}

namespace divine::mc
{
    struct TMachine : machine::tree< smt::NoSolver > {};
    struct GMachine : machine::graph< smt::NoSolver > {};

    template< typename... M > struct MW;

    template< typename M >
    struct MW< M >
    {
        struct TQ : M::tq {};
        struct W : Weaver< TQ > {};
    };

    template< typename M, typename... Ms >
    struct MW< M, Ms... > : MW< Ms... >
    {
        struct TQ : task_queue_join< typename MW< Ms... >::TQ, typename M::tq > {};
        struct W : Weaver< TQ > {};
    };

    template< typename... M >
    auto weave( M &... machine )
    {
        return typename MW< M... >::W().extend_ref( machine... );
    }
}
