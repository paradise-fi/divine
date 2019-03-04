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

#include <set>
#include <memory>

namespace divine::mc
{

    using namespace std::literals;
    namespace hashset = brick::hashset;
    using BC = std::shared_ptr< BitCode >;

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

    struct Task
    {
        using GraphTs = StateTQ::Tasks;
        using Boot = GraphTs::Start;
        using Schedule = GraphTs::Expand;

        struct Compute
        {
            Snapshot snap, origin;
            vm::State state;
            Compute( Snapshot snap, Snapshot orig, vm::State state )
                : snap( snap ), origin( orig ), state( state )
            {}
        };

        struct Choose : Compute
        {
            int choice, total;
            Choose( Snapshot snap, Snapshot orig, vm::State state, int c, int t )
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
    template< typename Solver >
    struct Tree : TQ::Skel
    {
        using Hasher = mc::Hasher< Solver >;
        using PointerV = vm::value::Pointer;
        using Context = MContext;
        using Eval = vm::Eval< Context >;
        using Env = std::vector< std::string >;
        using Pool = vm::CowHeap::Pool;

        auto &program() { return _bc->program(); }
        auto &debug() { return _bc->debug(); }
        auto &heap() { return context().heap(); }
        Context &context() { return _ctx; }

        Solver _solver;
        BC _bc;
        Context _ctx;

        Pool _snap_pool, _state_pool;
        brick::mem::RefPool< Pool, uint8_t > _snap_refcnt;

        std::shared_ptr< std::atomic< int64_t > > _total_instructions;

        void update_instructions()
        {
            *_total_instructions += context().instruction_count();
            context().instruction_count( 0 );
        }

        Tree( BC bc )
            : Tree( bc, Context( bc->program() ) )
        {}

        Tree( BC bc, const Context &ctx )
            : _bc( bc ), _ctx( ctx ), _snap_refcnt( _snap_pool ),
              _total_instructions( new std::atomic< int64_t >( 0 ) )
        {}

        ~Tree()
        {
            ASSERT_EQ( _snap_pool.stats().total.count.used, 0 );
            update_instructions();
        }

        void choose( TQ &tq, Snapshot origin )
        {
            auto snap = context().snapshot( _snap_pool );
            auto state = context()._state;

            _snap_refcnt.get( snap );

            Eval eval( context() );
            context()._choice_take = context()._choice_count = 0;
            eval.refresh();
            eval.dispatch();

            /* queue up all choices other than 0 */
            for ( int i = 1; i < context()._choice_count; ++i )
            {
                _snap_refcnt.get( snap );
                tq.add< Task::Choose >( snap, origin, state, i, context()._choice_count );
            }

            /* proceed with choice = 0 */
            compute( tq, origin, snap );
        }

        virtual std::pair< State, bool > store()
        {
            return { State( context().snapshot( _state_pool ) ), true };
        }

        virtual bool loop_closed()
        {
            return false;
        }

        void schedule( TQ &tq, Snapshot origin, Snapshot cont_from )
        {
            if ( context().frame().null() )
            {
                Label lbl;
                lbl.accepting = context().flags_any( _VM_CF_Accepting );
                lbl.error = context().flags_any( _VM_CF_Error );
                auto [ state, isnew ] = store();
                tq.add< StateTQ::Skel::Edge >( State( origin ), state, lbl, isnew );
            }
            else if ( !loop_closed() )
                return compute( tq, origin, cont_from );
        }

        bool feasible()
        {
            if ( context().flags_any( _VM_CF_Cancel ) )
                return false;
            if ( context()._assume.null() )
                return true;

            // TODO validate the path condition
            return _solver.feasible( context().heap(), context()._assume );
        }

        void compute( TQ &tq, Snapshot origin, Snapshot cont_from = Snapshot() )
        {
            auto destroy = [&]( auto p ) { heap().snap_put( _snap_pool, p ); };

            auto cleanup = [&]
            {
                if ( cont_from )
                    _snap_refcnt.put( cont_from, destroy );
            };

            brick::types::Defer _( cleanup );
            Eval eval( context() );
            bool choice = eval.run_seq( !!cont_from );

            if ( !feasible() )
                return;

            if ( choice )
                choose( tq, origin );
            else
                schedule( tq, origin, cont_from );

        }

        virtual void boot( TQ &tq )
        {
            Eval eval( context() );
            vm::setup::boot( context() );
            eval.run();
        }

        using TQ::Skel::run;

        void run( TQ &tq, Task::Boot )
        {
            boot( tq );

            if ( vm::setup::postboot_check( context() ) )
                tq.add< Task::Schedule >( store().first );
        }

        void run( TQ &tq, Task::Schedule e )
        {
            context().load( _state_pool, e.from.snap );
            vm::setup::scheduler( context() );
            compute( tq, e.from.snap );
        }

        void run( TQ &tq, Task::Choose c )
        {
            context().load( _snap_pool, c.snap );
            context()._state = c.state;
            context()._choice_take = c.choice;
            context()._choice_count = c.total;
            context().flush_ptr2i();
            context().load_pc();
            compute( tq, c.origin, c.snap );
        }

        auto make_hasher()
        {
            Hasher h( _state_pool, context().heap(), this->_solver );
            h._root = context().state_ptr();
            ASSERT( !h._root.null() );
            return h;
        }

        bool equal( Snapshot a, Snapshot b )
        {
            return make_hasher().equal_symbolic( a, b );
        }
    };

    template< typename Solver >
    struct Graph : Tree< Solver >
    {
        using Hasher = mc::Hasher< Solver >;
        using HT = hashset::Concurrent< Snapshot >;
        using Tree< Solver >::context;

        Hasher _hasher;
        struct Ext
        {
            HT states;
            bool overwrite = false;
        } _ext;

        Graph( BC bc ) : Tree< Solver >( bc ),
                         _hasher( this->_state_pool, context().heap(), this->_solver )
        {}

        auto &hasher() { return _hasher; }
        void enable_overwrite() { _ext.hasher.overwrite = true; }
        bool equal( Snapshot a, Snapshot b ) { return hasher().equal_symbolic( a, b ); }

        virtual std::pair< State, bool > store() override
        {
            auto snap = context().snapshot( this->_state_pool );
            auto r = _ext.states.insert( snap, hasher() );
            if ( *r != snap )
            {
                ASSERT( !_ext.overwrite );
                this->_state_pool.free( snap ), context().load( this->_state_pool, *r );
            }
            else
                context().flush_ptr2i();
            return { State( *r ), r.isnew() };
        }

        virtual void boot( TQ &tq ) override
        {
            Tree< Solver >::boot( tq );
            hasher()._root = context().state_ptr();
        }

    };
}

namespace divine::mc
{
    using TMachine = machine::Tree< smt::NoSolver >;
    using GMachine = machine::Graph< smt::NoSolver >;
}
