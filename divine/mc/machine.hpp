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
    struct boot : base {};

    template< typename ctx_t >
    struct boot_sync : base
    {
        using pool = vm::CowHeap::Pool;
        pool snap_pool, state_pool;
        ctx_t ctx;
        boot_sync( pool a, pool b, ctx_t c ) : base( -2 ), state_pool( a ), snap_pool( b ), ctx( c ) {}
    };

    struct origin
    {
        Snapshot snap;
        HT ht_loop;
        origin() = default;
        explicit origin( Snapshot s ) : snap( s ) {}
    };

    struct schedule : base
    {
        Snapshot snap;
        schedule( Snapshot s ) : snap( s ) {}
    };

    struct with_snap : base
    {
        task::origin origin;
        Snapshot snap;
        with_snap( task::origin o, Snapshot s ) : origin( o ), snap( s ) {}
    };

    struct with_state : with_snap
    {
        vm::State state;
        with_state( task::origin o, Snapshot snap, vm::State state )
            : with_snap( o, snap ), state( state )
        {}
    };

    struct store_state : with_snap  { using with_snap::with_snap; };
    struct check_loop  : with_state { using with_state::with_state; };
    struct compute     : with_state { using with_state::with_state; };

    struct choose : with_state
    {
        int choice, total;

        choose( task::origin o, Snapshot snap, vm::State state, int c, int t )
            : with_state( o, snap, state ), choice( c ), total( t )
        {}
    };
}

namespace divine::mc::event
{
    struct base : task::base
    {
        base() : task::base( -2 ) {}
    };

    struct edge : base
    {
        Snapshot from, to;
        bool is_new;
        edge( Snapshot f, Snapshot t, bool n ) : from( f ), to( t ), is_new( n ) {}
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

    template< typename solver, typename tq_, typename next >
    struct base_ : machine_base, with_pools, with_counters, with_bc, with_solver< solver >, next
    {
        using tq = tq_;

        template< typename ctx_t >
        void run( tq, const task::boot_sync< ctx_t > &bs )
        {
            this->_snap_pool = bs.snap_pool;
            this->_state_pool = bs.state_pool;
        }
    };

    template< typename ctx, typename next >
    struct with_context_ : next
    {
        using context_t = ctx;
        ctx _ctx;
        ctx &context() { return _ctx; }
        auto &heap() { return context().heap(); }

        using next::run;

        template< typename ctx_t >
        void run( typename next::tq q, const task::boot_sync< ctx_t > &bs )
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
        using typename next::context_t;
        using tq = task_queue_extend< typename next::tq, task::boot_sync< context_t >,
                                      task::store_state, task::check_loop, task::choose >;
        using Eval = vm::Eval< context_t >;
        using next::context;

        void eval_choice( tq q, task::origin o )
        {
            auto snap = context().snapshot( this->_snap_pool );
            auto state = context()._state;

            Eval eval( this->context() );
            this->context()._choice_take = this->context()._choice_count = 0;
            eval.refresh();
            eval.dispatch();
            int total = this->context()._choice_count;

            for ( int i = 0; i < total; ++i )
            {
                this->_snap_refcnt.get( snap );
                this->reply( q, task::choose( o, snap, state, i, total ) );
            }
        }

        void eval_interrupt( tq q, task::origin o, Snapshot cont_from )
        {
            if ( this->context().frame().null() )
            {
                auto snap = this->context().snapshot( this->_state_pool );
                this->reply( q, task::store_state( o, snap ) );
            }
            else
            {
                auto snap = this->context().snapshot( this->_snap_pool );
                this->reply( q, task::check_loop( o, snap, this->context()._state ) );
            }
        }

        virtual bool feasible( tq )
        {
            if ( this->context().flags_any( _VM_CF_Cancel ) )
                return false;
            if ( this->context()._assume.null() )
                return true;

            if ( this->_solver.feasible( this->context().heap(), this->context()._assume ) )
                return true;
            else
                return this->context().flags_set( 0, _VM_CF_Cancel ), false;
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

        void compute( tq q, task::origin o, Snapshot cont_from = Snapshot() )
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

        void run( tq q, task::boot )
        {
            this->context().program( this->program() );
            Eval eval( this->context() );
            vm::setup::boot( this->context() );
            eval.run();

            if ( vm::setup::postboot_check( this->context() ) )
            {
                auto snap = this->context().snapshot( this->_state_pool );
                task::boot_sync< context_t > sync
                    ( this->_state_pool, this->_snap_pool, this->_ctx );
                this->push( q, sync );
                this->reply( q, task::store_state( task::origin(), snap ) );
            }
        }

        void run( tq q, task::schedule t )
        {
            this->context().load( this->_state_pool, t.snap );
            vm::setup::scheduler( this->context() );
            compute( q, task::origin( t.snap ) );
        }

        void run( tq q, task::choose c )
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

    struct tree_search : machine_base
    {
        using tq = task_queue< task::compute, task::schedule, task::boot, task::choose, event::edge >;

        void run( tq q, task::check_loop t )  { push( q, task::compute( t.origin, t.snap, t.state ) ); }
        void run( tq q, task::start )         { push( q, task::boot() ); }
        void run( tq q, task::choose t )      { reply( q, t ); } /* bounce right back */
        void run( tq q, task::store_state t )
        {
            push( q, task::schedule( t.snap ) );
            push( q, event::edge( t.origin.snap, t.snap, true ) );
        }
    };

    template< typename solver_t >
    struct graph_search : machine_base
    {
        using tq = task_queue< task::compute, event::edge, task::schedule, task::boot, task::choose >;
        using hasher_t = mc::Hasher< solver_t >;

        solver_t _solver;
        hasher_t _hasher;
        HT _ht_sched;
        vm::CowHeap::Pool _pool;
        vm::CowHeap _heap;

        void run( tq q, task::start )
        {
            push( q, task::boot() );
        }

        template< typename ctx_t >
        void run( tq, const task::boot_sync< ctx_t > &bs )
        {
            _pool = bs.state_pool;
            _heap = bs.ctx.heap();
            _hasher.attach( _heap );
            _hasher._root = bs.ctx.state_ptr();
        }

        auto &hasher() { return _hasher; }
        graph_search() : _hasher( _pool, _solver ) {}

        std::pair< Snapshot, bool > store( Snapshot snap, HT &table )
        {
            auto r = table.insert( snap, hasher() );

            if ( r->load() != snap )
            {
                ASSERT( !_hasher.overwrite );
                ASSERT( !r.isnew() );
                _heap.snap_put( _pool, snap );
            }

            return { *r, r.isnew() };
        }

        void run( tq q, task::store_state t )
        {
            auto [ ns, isnew ] = store( t.snap, _ht_sched );
            if ( t.origin.snap )
                this->push( q, event::edge( t.origin.snap, ns, isnew ) );
            if ( isnew )
                this->push( q, task::schedule( ns ) );
        }

        void run( tq q, task::check_loop t )
        {
            auto [ ns, isnew ] = store( t.snap, t.origin.ht_loop );
            if ( isnew )
                push( q, task::compute( t.origin, ns, t.state ) );
            else
                TRACE( "loop closed at", ns );
        }

        void run( tq q, task::choose t ) { reply( q, t ); } /* bounce right back */
    };

    template< typename solver, typename tq = task_queue<> >
    struct base : brq::module< base_, solver, tq > {};

    template< typename ctx >
    struct with_context : brq::module< with_context_, ctx > {};

    template< typename solver, typename tq = task_queue<> >
    using common = brq::compose< with_context< context >, base< solver, tq > >;

    using compute = brq::module< compute_ >;

    template< typename solver, typename tq = task_queue<> >
    using compute_stack = brq::compose_stack< compute, with_context< context >, base< solver, tq > >;
}

namespace divine::mc
{
    struct computer : machine::compute_stack< smt::NoSolver > {};
    struct gmachine : machine::graph_search< smt::NoSolver > {};
    using  tmachine = machine::tree_search;

    template< typename... M > struct MW;

    template<>
    struct MW<>
    {
        using TQ = task_queue< task::start >;
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
