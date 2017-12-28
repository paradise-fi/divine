
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
#include <divine/vm/explore.hpp>
#include <divine/mc/trace.hpp>

namespace divine {
namespace mc {

template< typename Next, typename Explore = vm::Explore >
struct Safety : Job
{
    using Builder = Explore;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;
    using StateTrace = mc::StateTrace< Explore >;

    Explore _ex;
    SlavePool _ext;
    Next _next;

    bool _error_found;
    typename Builder::State _error, _error_to;
    typename Builder::Label _error_label;

    auto make_search()
    {
        return ss::make_search(
            _ex, ss::listen(
                [&]( auto from, auto to, auto label, bool isnew )
                {
                    if ( isnew )
                    {
                        _ext.materialise( to.snap, sizeof( from ) );
                        Parent &parent = *_ext.machinePointer< Parent >( to.snap );
                        parent = from.snap;
                    }
                    if ( label.error )
                    {
                        _error_found = true;
                        _error = from; /* the error edge may not be the parent of 'to' */
                        _error_to = to;
                        _error_label = label;
                        return ss::Listen::Terminate;
                    }
                    return _next.edge( from, to, label, isnew );
                },
                [&]( auto st ) { return _next.state( st ); } ) );
    }

    Safety( vm::explore::BC bc, Next next )
        : _ex( bc ),
          _ext( _ex.pool() ),
          _next( next ),
          _error_found( false )
    {
        _ex.start();
    }

    void start( int threads ) override
    {
        using Search = decltype( make_search() );
        _search.reset( new Search( std::move( make_search() ) ) );
        Search *search = dynamic_cast< Search * >( _search.get() );
        statecount = [=]()
        {
            int64_t rv = _ex._states._s->used;
            search->ws_each( [&]( auto &bld, auto & ) { rv += bld._states._l.inserts; } );
            return rv;
        };
        queuesize = [=]() { return search->qsize(); };
        search->start( threads );
    }

    Trace ce_trace() override
    {
        if ( !_error_found )
            return Trace();

        StateTrace rv;
        rv.emplace_front( _error_to.snap, _error_label );
        auto i = _error.snap;
        while ( i != _ex._initial.snap )
        {
            rv.emplace_front( i, std::nullopt );
            i = *_ext.machinePointer< vm::CowHeap::Snapshot >( i );
        }
        rv.emplace_front( _ex._initial.snap, std::nullopt );
        return mc::trace( _ex, rv );
    }

    void dbg_fill( DbgCtx &dbg ) override { dbg.load( _ex._ctx ); }

    Result result() override
    {
        if ( !statecount() )
            return Result::BootError;
        return _error_found ? Result::Error : Result::Valid;
    }

    virtual PoolStats poolstats() override
    {
        return PoolStats{ { "snapshot memory", _ex._ctx.heap()._snapshots.stats() },
                          { "fragment memory", _ex._ctx.heap()._objects.stats() } };
    }
};

template< typename Next >
std::shared_ptr< Job > make_safety( vm::explore::BC bc, Next next )
{
    if ( bc->is_symbolic() )
        return std::make_shared< Safety< Next, vm::SymbolicExplore > >( bc, next );
    return std::make_shared< Safety< Next, vm::Explore > >( bc, next );
}

}

namespace t_mc {

struct TestSafety
{
    TEST( simple )
    {
        auto bc = t_vm::prog_int( "4", "*r - 1" );
        int edgecount = 0, statecount = 0;
        auto safe = mc::make_safety(
            bc, ss::passive_listen(
                [&]( auto, auto, auto ) { ++edgecount; },
                [&]( auto ) { ++statecount; } ) );
        safe->start( 1 );
        safe->wait();
        ASSERT_EQ( edgecount, 4 );
        ASSERT_EQ( statecount, 5 );
    }
};

}

}
