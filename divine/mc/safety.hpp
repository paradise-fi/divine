
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

#include <divine/ss/search.hpp>
#include <divine/vm/explore.hpp>
#include <divine/mc/trace.hpp>

namespace divine {
namespace mc {

struct Job : ss::Job
{
    std::shared_ptr< brick::shmem::ThreadBase > _monitor_loop;
    std::function< void() > _monitor;
    std::function< int64_t() > statecount = []() { return 0; };
    std::function< int64_t() > queuesize =  []() { return 0; };
    std::shared_ptr< ss::Job > _search;

    template< typename Monitor >
    void start( int threads, Monitor monit )
    {
        start( threads ); /* virtual */

        _monitor = monit;
        using msecs = std::chrono::milliseconds;
        auto do_monit = [=]() { monit(); std::this_thread::sleep_for( msecs( 500 ) ); };
        using MonitLoop = brick::shmem::AsyncLambdaLoop< decltype( do_monit ) >;
        _monitor_loop = std::make_shared< MonitLoop >( do_monit );
        _monitor_loop->start();
    }

    void wait() override
    {
        _search->wait();
        if ( _monitor_loop )
            _monitor_loop->stop();
        if ( _monitor )
            _monitor();
    }

    using PoolStats = std::map< std::string, brick::mem::Stats >;
    using DbgCtx = vm::DebugContext< vm::Program, vm::CowHeap >;

    virtual Trace ce_trace() { return Trace(); }
    virtual bool error_found() { return false; }
    virtual PoolStats poolstats() { return PoolStats(); }
    virtual void dbg_fill( DbgCtx & ) {}
    virtual void start( int ) override = 0;
};

template< typename Next >
struct Safety : Job
{
    using Builder = vm::Explore;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;
    using CEStates = std::deque< vm::CowHeap::Snapshot >;

    vm::Explore _ex;
    SlavePool _ext;
    Next _next;

    bool _error_found;
    Builder::State _error;

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
                    if ( to.error )
                    {
                        _error_found = true;
                        _error = to;
                        return ss::Listen::Terminate;
                    }
                    return _next.edge( from, to, label, isnew );
                },
                [&]( auto st ) { return _next.state( st ); } ) );
    }

    Safety( vm::Explore::BC bc, Next next, bool verbose )
        : _ex( bc ),
          _ext( _ex.pool() ),
          _next( next ),
          _error_found( false )
    {
        if ( verbose ) std::cerr << "booting... " << std::flush;
        _ex.start();
        if ( verbose ) std::cerr << "done" << std::endl;
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

        CEStates rv;
        auto i = _error.snap;
        while ( i != _ex._initial.snap )
        {
            rv.push_front( i );
            i = *_ext.machinePointer< vm::CowHeap::Snapshot >( i );
        }
        rv.push_front( _ex._initial.snap );
        return mc::trace( _ex, rv );
    }

    void dbg_fill( DbgCtx &dbg ) override { dbg.load( _ex._ctx ); }
    bool error_found() override { return _error_found; }

    virtual PoolStats poolstats() override
    {
        return PoolStats{ { "snapshot memory", _ex._ctx.heap()._snapshots.stats() },
                          { "fragment memory", _ex._ctx.heap()._objects.stats() } };
    }
};

template< typename Next >
std::shared_ptr< Job > make_safety( vm::Explore::BC bc, Next next, bool verbose = false )
{
    return std::make_shared< Safety< Next > >( bc, next, verbose );
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
