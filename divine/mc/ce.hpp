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

namespace divine::mc::machine
{
    struct graph_ce : tree_search
    {
        using pool = vm::CowHeap::Pool;

        pool _succ_pool;
        brick::mem::SlavePool< pool > _meta;

        struct path_info : brq::refcount_base<>
        {
            using ptr = brq::refcount_ptr< path_info >;
            ptr parent;
            int choice, total;
            path_info( ptr p, int c, int t ) : parent( p ), choice( c ), total( t ) {}
        };

        int _path_id = 0;
        std::map< int, path_info::ptr > _path;
        std::vector< Snapshot > _errors;

        struct meta_t
        {
            Snapshot parent;
            path_info::ptr path;
            bool visited;
        };

        meta_t &meta( Snapshot s )
        {
            return *_meta.template machinePointer< meta_t >( s );
        }

        using tree_search::run;
        void run( tq, task::start ) {}

        template< typename ctx_t >
        void run( tq q, const task::boot_sync< ctx_t > &bs )
        {
            _meta.attach( bs.state_pool );
        }

        void run( tq q, event::edge e )
        {
            if ( e.is_new )
            {
                _meta.materialise( e.to, sizeof( meta_t ) );
                meta( e.to ).parent = e.from;
            }
        }

        void run( tq q, event::error t )
        {
            if ( !t.from || t.msg_to >= 0 )
                return;

            auto &m = meta( t.to );
            TRACE( "ce error", t.from, "→", t.to, "visited =", m.visited );
            if ( !m.visited && m.parent )
            {
                _errors.push_back( t.to );
                task::schedule t( m.parent );
                t.msg_id = _path_id ++;
                this->push( q, t );
            }
            m.visited = true;
        }

        void run( tq q, task::choose t )
        {
            int parent_id = t.msg_id;
            t.msg_id = _path_id ++;
            _path[ t.msg_id ] = brq::make_refcount< path_info >( _path[ parent_id ], t.choice, t.total );
            TRACE( "ce choose", t.origin.snap, "msg_id =", t.msg_id, "path =", _path[ t.msg_id ] );
            this->reply( q, t, false );
        }

        void run( tq q, task::dedup_state t )
        {
            auto &m = meta( t.snap );
            TRACE( "ce dedup", t.snap, "msg_id =", t.msg_id, "path =", _path[ t.msg_id ],
                   "visited =", m.visited );
            if ( m.visited )
                m.path = _path[ t.msg_id ];
            _path.erase( t.msg_id );
        }

        void run( tq q, task::store_state s )
        {
            task::dedup_state t( s.snap );
            t.msg_id = s.msg_id;
            this->push( q, t );

            TRACE( "ce step", s.origin.snap, "→", s.snap, "msg_id =", s.msg_id );
            auto &m = meta( s.origin.snap );
            if ( !m.visited && m.parent )
            {
                task::schedule t( m.parent );
                t.msg_id = _path_id ++;
                this->push( q, t );
            }
            m.visited = true;
        }

        void dump()
        {
            TRACE( "error count", _errors.size() );
            for ( auto e : _errors )
            {
                for ( ; e; e = meta( e ).parent )
                {
                    TRACE( "state trace", e );
                    for ( auto pi = meta( e ).path; pi != nullptr; pi = pi->parent )
                        TRACE( "choice", pi->choice, "/", pi->total );
                }
            }
        }
    };
}
