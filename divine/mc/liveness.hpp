
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

enum class StackItemType { DfsStack, Successors };

template< typename Builder >
struct NestedDFS : ss::Job
{
    using State = typename Builder::State;
    using Label = typename Builder::Label;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;

    Builder _builder;
    SlavePool _flagPool;

    struct StateFlags {
        bool outer_visited:1 = false;
        bool inner_visited:1 = false;
        bool in_outer_stack:1 = false;
    };

    explicit NestedDFS( Builder builder ) :
        _builder( builder ),
        _flagPool( _builder.pool() )
    { }

    struct StackItem
    {
        State state;
        bool accepting:1;
        bool error:1;
        StackItemType type:1;

        template< typename Label >
        StackItem( State s, const Label &l ) :
            state( s ), type( StackItemType::Successors ), accepting( l.accepting ), error( l.error )
        { }

        StackItem( State s ) :
            state( s ), type( StackItemType::Successors ), accepting( false ), error( false )
        { }
    };

    using Stack = std::deque< StackItem >;
    Stack inner_stack, outer_stack;

    using StackRange = brick::query::Range< typename Stack::iterator >;
    struct CeDescription {
        StackRange tail_fragment;
        StackRange lasso_inner_fragment;
        StackRange lasso_outer_fragment;
        std::optional< State > goal;
    } counterexample;

    auto init_state( State &s )
    {
        _flagPool.materialise( s.snap, sizeof( StateFlags ) );
        new ( _flagPool.machinePointer< StateFlags >( s.snap ) ) StateFlags();
    };

    bool outer( State from )
    {
        outer_stack.emplace_back( from );
        init_state( from );

        while ( !outer_stack.empty() )
        {
            const auto item = outer_stack.back();
            auto &flags = *_flagPool.machinePointer< StateFlags >( item.state.snap );

            if ( item.type == StackItemType::DfsStack ) // backtracking
            {
                ASSERT( flags.outer_visited );
                flags.in_outer_stack = false;
                outer_stack.pop_back();

                if ( item.accepting && inner( item.state ) ) {
                    counterexample.tail_fragment = StackRange( outer_stack.begin(), outer_stack.end() );
                    return true;
                }
            }
            else
            {
                outer_stack.back().type = StackItemType::DfsStack;
                if ( !flags.outer_visited )
                {
                    flags.outer_visited = true;
                    flags.in_outer_stack = true;
                    if ( item.error ) {
                        counterexample.goal = item.state;
                        counterexample.tail_fragment = StackRange( outer_stack.begin(), outer_stack.end() - 1 );
                        return true;
                    }

                    _builder.edges( item.state, [&]( State to, const Label &l, bool isnew )
                        {
                            if ( isnew )
                                init_state( to );
                            outer_stack.emplace_back( to, l );
                        } );
                }
                else
                {
                    outer_stack.pop_back();
                    if ( item.accepting )
                    {
                        if ( flags.in_outer_stack ) // fastpath
                        {
                            counterexample.goal = item.state;
                            counterexample.tail_fragment = StackRange( outer_stack.begin(), outer_stack.end() );
                            return true;
                        }
                        else if ( inner( item.state ) ) {
                            counterexample.tail_fragment = StackRange( outer_stack.begin(), outer_stack.end() );
                            return true;
                        }
                    }
                }
            }
            _builder._d.sync();
        }
        return false;
    }

    bool inner( State seed ) {
        inner_stack.emplace_back( seed );

        while ( !inner_stack.empty() )
        {
            auto &item = inner_stack.back();
            auto &flags = *_flagPool.machinePointer< StateFlags >( item.state.snap );

            if ( item.type == StackItemType::DfsStack ) // backtracking
                inner_stack.pop_back();
            else
            {
                item.type = StackItemType::DfsStack;
                if ( item.accepting && item.state == seed || flags.in_outer_stack )
                {
                    counterexample.goal = seed;
                    if ( item.state == seed )
                        counterexample.lasso_inner_fragment = StackRange( inner_stack.begin(), inner_stack.end() - 1 );
                    else {
                        auto it = outer_stack.begin(),
                             oend = outer_stack.end();
                        for ( ; it != oend && !(it->state == item.state && it->type == StackItemType::DfsStack); ++it )
                        { }
                        counterexample.lasso_inner_fragment = StackRange( inner_stack.begin(), inner_stack.end() );
                        if ( it + 1 < oend )
                            counterexample.lasso_outer_fragment = StackRange( it + 1, oend );
                    }
                    return true;
                }
                if ( !flags.inner_visited )
                {
                    flags.inner_visited = true;
                    _builder.edges( item.state, [&]( State to, const Label &l, bool isnew )
                        {
                            if ( isnew )
                                init_state( to );
                            inner_stack.emplace_back( to, l );
                        } );
                } else
                    inner_stack.pop_back();
            }
            _builder._d.sync();
        }

        inner_stack.clear();
        return false;
    }

    void run()
    {
        _builder.initials( [found = false, this] ( State state ) mutable
            {
                if ( !found )
                    found = outer( state );
            } );
    }

    std::future< void > _thread;

    void start( int thread_count ) override
    {
        if ( thread_count != 1 )
            throw new std::runtime_error( "Nested DFS only supports one thread." );
        _thread = std::async( [&]{ run(); } );
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

    Builder _ex;
    Next _next;
    using StateTrace = mc::StateTrace< Builder >;
    std::function< StateTrace() > _get_trace;
    std::function< bool() > _error_found;

    template< typename... Args >
    Liveness( builder::BC bc, Next next, Args... builder_opts )
        : _ex( bc, builder_opts... ),
          _next( next )
    {
        _ex.start();
    }

    void start( int threads ) override
    {
        auto *search = new NestedDFS( _ex );
        _search.reset( search );
        stats = [=] { return std::pair( _ex._d.total_states->load(), _ex._d.total_instructions->load() ); };
        queuesize = [=] { return search->outer_stack.size() + search->inner_stack.size(); };

        _get_trace = [=]() mutable
        {
            using State = typename std::remove_reference_t< decltype( *search ) >::State;
            auto &ce = search->counterexample;
            StateTrace trace;

            for ( auto &i : ce.lasso_outer_fragment )
                if ( i.type == StackItemType::DfsStack )
                    trace.emplace_back( i.state.snap, std::nullopt );
            trace.emplace_back( ce.goal->snap, std::nullopt );

            auto move_to_trace = [&]( auto r, auto &stack )
            {
                for ( auto it = std::reverse_iterator( r.end() ), end = std::reverse_iterator( r.begin() );
                      it != end; ++it )
                {
                    if ( it->type == StackItemType::DfsStack )
                        trace.emplace_front( it->state.snap, std::nullopt );
                    stack.erase( it.base() - 1, stack.end() );
                }
                stack.clear();
            };

            move_to_trace( ce.lasso_inner_fragment, search->inner_stack );
            move_to_trace( ce.tail_fragment, search->outer_stack );

            return trace;
        };

        _error_found = [=]() { return search->counterexample.goal.has_value(); };

        search->start( threads );
    }

    void dbg_fill( DbgCtx &dbg ) override { dbg.load( _ex.pool(), _ex.context() ); }

    Result result() override
    {
        if ( !stats().first )
            return Result::BootError;
        return _error_found() ? Result::Error : Result::Valid;
    }

    Trace ce_trace() override
    {
        return _error_found() ? mc::trace( _ex, _get_trace() ) : mc::Trace();
    }

    virtual PoolStats poolstats() override
    {
        return PoolStats{ { "snapshot memory", _ex.pool().stats() },
                          { "fragment memory", _ex.context().heap().mem_stats() } };
    }
};

}
}
