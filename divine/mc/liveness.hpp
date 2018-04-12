
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

#include <divine/mc/job.hpp>
#include <divine/mc/builder.hpp>
#include <divine/mc/trace.hpp>

namespace divine {
namespace mc {

struct EdgeHasher {
    template< typename Edge >
    size_t operator()( Edge edge ) const
    {
        uint32_t h1 = brick::bitlevel::mixdown( edge.first.snap.intptr() );
        uint32_t h2 = brick::bitlevel::mixdown( edge.second.snap.intptr() );
        return ( uint64_t( h1 ) << 32 ) | h2;
    }
};

template< typename Next, typename Builder >
struct NestedDFS : ss::Job
{
    using State = typename Builder::State;
    using Label = typename Builder::Label;
    using Edge = std::pair< State, State >;
    using EdgeMap = std::unordered_map< Edge, bool, EdgeHasher >;

    Builder _builder;
    EdgeMap _visited;
    Next _next;

    NestedDFS( Builder builder, Next next )
        : _builder( builder ),
          _next( next )
    {
    }

    struct DFSItem
    {
        Edge edge;
        enum Type { Pre, Post } type:1;
        bool label:1;

        DFSItem( Type t, Edge e, bool l = false )
            : edge( e )
            , type( t )
            , label( l )
        {
        }
    };

    std::stack< DFSItem > _inner, _outer;

    bool nested( Edge seed )
    {
        _inner.emplace( DFSItem::Pre, seed, false );
        bool out = false;

        while( !_inner.empty() && !out )
        {
            auto item = _inner.top(); _inner.pop();
            Edge from = item.edge;
            _builder.edges( from.second,
                [&]( State to, Label, bool )
                {
                    auto next = std::make_pair( from.second, to );
                    if( seed == next )
                    {
                        out = true;
                        _next( seed );
                    }
                    ASSERT( _visited.count( next ) );
                    if( !_visited[next] )
                    {
                        _inner.emplace( DFSItem::Pre, next, false );
                        _visited[next] = true;
                    }
                } );
        }
        return out;
    }

    bool dfs( Edge edge )
    {
        bool out = false;
        _outer.emplace( DFSItem::Pre, edge, false );
        _visited.emplace( edge, false );

        while( !_outer.empty() )
        {
            auto item = _outer.top(); _outer.pop();
            ASSERT( _visited.count( item.edge ) );
            if( item.type == DFSItem::Post ) // Post -> backtracking -> run detect cycle
            {
                if( item.label )
                    out = out || nested( item.edge );
            }
            else
            {
                item.type = DFSItem::Post;
                _outer.push( item );
                _builder.edges( item.edge.second,
                    [&]( State to, Label label, bool )
                    {
                        auto kid = std::make_pair( item.edge.second, to );
                        if( !_visited.count( kid ) ) {
                            _outer.emplace( DFSItem::Pre, kid, bool( label.accepting ) );
                            _visited.emplace( kid, false );
                        }
                    } );
            }
            if( out )
                return true;
        }
        return out;
    }

    void ndfs()
    {
        bool found = false;
        _builder.initials( [&] ( State state )
            {
                found = found || dfs( std::make_pair( State(), state ) );
            } );
    }

    std::future< void > _thread;

    void start( int thread_count ) override
    {
        if ( thread_count != 1 )
            throw new std::runtime_error( "Nested DFS only supports one thread." );
        _thread = std::async( [&]{ ndfs(); } );
    }

    void wait() override
    {
        _thread.get();
    }

    void stop() override {}
};

template< typename Next, typename Builder_ = ExplicitBuilder >
struct Liveness : Job
{
    using Builder = Builder_;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;
    using CEStates = std::deque< vm::CowHeap::Snapshot >;

    Builder _ex; //state space
    SlavePool _ext;
    Next _next;

    bool _error_found;

    using StateTrace = mc::StateTrace< Builder >;
    std::function< StateTrace() > _get_trace;

    template< typename... Args >
    Liveness( builder::BC bc, Next next, Args... builder_opts )
        : _ex( bc, builder_opts... ),
          _ext( _ex.pool() ),
          _next( next ),
          _error_found( false )
    {
        _ex.start();
    }

    void start( int threads ) override
    {
        auto found = [&]( auto ){ _error_found = true; };
        using NDFS = NestedDFS< decltype( found ), Builder >;

        _search.reset( new NDFS( _ex, found ) ); //Builder, Next
        NDFS *search = dynamic_cast< NDFS * >( _search.get() );
        stats = [=]() { return std::make_pair( _ex._d.states._s->used.load(), 0 ); };

        auto move = []( auto &stack, auto &trace )
        {
            while ( !stack.empty() )
            {
                if ( stack.top().type == NDFS::DFSItem::Post )
                    trace.emplace_front( stack.top().edge.second.snap, std::nullopt );
                stack.pop();
            }
        };

        _get_trace = [=]() mutable
        {
            StateTrace trace;
            move( search->_inner, trace );
            move( search->_outer, trace );
            return trace;
        };

        search->start( threads );
    }

    void dbg_fill( DbgCtx &dbg ) override { dbg.load( _ex.context() ); }

    Result result() override
    {
        if ( !stats().first )
            return Result::BootError;
        return _error_found ? Result::Error : Result::Valid;
    }

    Trace ce_trace() override
    {
        return _error_found ? mc::trace( _ex, _get_trace() ) : mc::Trace();
    }

    virtual PoolStats poolstats() override
    {
        return PoolStats{ { "snapshot memory", _ex.context().heap()._snapshots.stats() },
                          { "fragment memory", _ex.context().heap()._objects.stats() } };
    }
};

}
}
