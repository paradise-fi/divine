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

#include <divine/mc/machine.hpp>

namespace divine::mc::task
{
    struct ce_step : base
    {
        Snapshot from, to;
        ce_step( Snapshot from, Snapshot to )
            : from( from ), to( to )
        {}
    };

    struct ce_choose : choose
    {
        using choose::choose;
    };
}

namespace divine::mc::machine
{
    template< typename next >
    struct ce_base_ : next /* overlays the standard base */
    {
        using tq = task_queue_extend< typename next::tq, task::ce_step, task::ce_choose >;
        using pool = vm::CowHeap::Pool;

        pool _succ_pool;
        brick::mem::SlavePool< pool > _meta;

        struct meta_t
        {
            Snapshot parent;
            pool::Pointer children;
        };

        meta_t &meta( Snapshot s )
        {
            return *_meta.template machinePointer< meta_t >( s );
        }

        using next::run;

        template< typename ctx_t >
        void run( tq q, const task::boot_sync< ctx_t > &bs )
        {
            next::run( q, bs );
            _meta.attach( this->_state_pool );
            TRACE( "ce_base boot_sync", this, this->_state_pool._s, _meta._m );
        }

        void queue_edge( tq q, const task::origin &o, State, Label, bool )
        {
            TRACE( "ce_base queue_edge", meta( o.snap ).parent, o.snap );
            push< task::ce_step >( q, meta( o.snap ).parent, o.snap );
        }

        void queue_choice( tq q, const task::origin &o, Snapshot snap, vm::State state, int i, int t )
        {
            push< task::ce_choose >( q, snap, o, state, i, t );
        }
    };

    template< typename next >
    struct ce_dispatch_ : next /* replaces search_dispatch */
    {
        using next::run;
        using typename next::tq;
        using next::_meta;

        void run( tq q, typename next::task_edge e )
        {
            if ( e.isnew )
            {
                TRACE( "ce_dispatch task_edge", this, e.to.snap, e.from.snap );
                _meta.materialise( e.to.snap, sizeof( typename next::meta_t ) );
                this->meta( e.to.snap ).parent = e.from.snap;
            }

            if ( e.label.error )
                push< task::ce_step >( q, e.from.snap, e.to.snap );
        }

        void run( tq q, task::ce_step s )
        {
            TRACE( "ce_step from", s.from, "to", s.to );
            if ( s.from )
                next::schedule( q, task::origin( s.from ) );
        }
    };

    using ce_base     = brq::module< ce_base_ >;
    using ce_dispatch = brq::module< ce_dispatch_ >;
    using ce_compute  = brq::compose< ce_dispatch, compute, tree_search, choose >;

    template< typename solver >
    using ce = brq::compose_stack< ce_compute, ce_base, common< solver > >;
}

namespace divine::mc
{
    struct CMachine : machine::ce< smt::NoSolver > {};
}
