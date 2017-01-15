// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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
#include <divine/vm/debug.hpp>
#include <divine/vm/program.hpp>
#include <divine/mc/trace.hpp>
#include <brick-mem>
#include <brick-shmem>

namespace divine {
namespace mc {

struct Job : ss::Job
{
    std::shared_ptr< brick::shmem::ThreadBase > _monitor_loop;
    std::function< void( bool ) > _monitor;
    std::function< int64_t() > statecount = []() { return 0; };
    std::function< int64_t() > queuesize =  []() { return 0; };
    std::shared_ptr< ss::Job > _search;

    template< typename Monitor >
    void start( int threads, Monitor monit )
    {
        start( threads ); /* virtual */

        _monitor = monit;
        using msecs = std::chrono::milliseconds;
        auto do_monit = [=]() { monit( false ); std::this_thread::sleep_for( msecs( 500 ) ); };
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
            _monitor( true );
    }

    using PoolStats = std::map< std::string, brick::mem::Stats >;
    using DbgCtx = vm::DebugContext< vm::Program, vm::CowHeap >;

    virtual Trace ce_trace() { return Trace(); }
    virtual bool error_found() { return false; }
    virtual PoolStats poolstats() { return PoolStats(); }
    virtual void dbg_fill( DbgCtx & ) {}
    virtual void start( int ) override = 0;
};


}
}
