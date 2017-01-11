
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

namespace divine {
namespace mc {

template< typename Next >
struct Safety : ss::Job
{
    using Builder = vm::Explore;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;
    using CEStates = std::deque< vm::CowHeap::Snapshot >;

    vm::Explore _ex;
    SlavePool _ext;
    std::shared_ptr< ss::Job > _search;
    std::shared_ptr< brick::shmem::ThreadBase > _monitor_loop;
    std::function< void() > _monitor;
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

    template< typename Monitor >
    void start( int threads, Monitor monit )
    {
        using Search = decltype( make_search() );
        _search.reset( new Search( std::move( make_search() ) ) );
        Search *search = dynamic_cast< Search * >( _search.get() );
        search->start( threads );

        _monitor = [=]() { monit( *search ); };
        using msecs = std::chrono::milliseconds;
        auto do_monit = [=]() { monit( *search ); std::this_thread::sleep_for( msecs( 500 ) ); };
        using MonitLoop = brick::shmem::AsyncLambdaLoop< decltype( do_monit ) >;
        _monitor_loop = std::make_shared< MonitLoop >( do_monit );
        _monitor_loop->start();
    }

    void start( int threads ) override { start( threads, []( auto & ) {} ); }

    void wait() override
    {
        _search->wait();
        _monitor_loop->stop();
        _monitor();
    }

    CEStates ce_states()
    {
        CEStates rv;
        if ( !_error_found )
            return rv;
        auto i = _error.snap;
        while ( i != _ex._initial.snap )
        {
            rv.push_front( i );
            i = *_ext.machinePointer< vm::CowHeap::Snapshot >( i );
        }
        rv.push_front( _ex._initial.snap );
        return rv;
    }
};

template< typename Next >
auto make_safety( vm::Explore::BC bc, Next next, bool verbose = false )
{
    return Safety< Next >( bc, next, verbose );
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
        safe.start( 1 );
        safe.wait();
        ASSERT_EQ( edgecount, 4 );
        ASSERT_EQ( statecount, 5 );
    }
};

}

}
