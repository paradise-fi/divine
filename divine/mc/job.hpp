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
#include <divine/dbg/context.hpp>
#include <divine/vm/program.hpp>
#include <divine/mc/trace.hpp>
#include <divine/mc/types.hpp>
#include <brick-mem>
#include <brick-shmem>

namespace divine::mc
{

struct Job : ss::Job
{
    std::shared_ptr< brick::shmem::ThreadBase > _monitor_loop;
    std::function< void( bool ) > _monitor;
    std::function< std::pair< int64_t, int64_t >() > stats = []() { return std::make_pair( 0, 0 ); };
    std::function< int64_t() > queuesize = []() { return 0; };
    std::shared_ptr< ss::Job > _search;

    template< typename Monitor >
    void start( int threads, Monitor monit )
    {
        start( threads ); /* virtual */
        _monitor = monit;
    }

    void wait() override
    {
        auto clock = std::chrono::steady_clock::now();
        auto search = std::async( [&] { _search->wait(); } );
        auto cleanup = [&] { _search->stop(); if ( _monitor ) _monitor( true ); };
        using brick::shmem::wait;

        while ( wait( &search, &search + 1, cleanup, clock ) == std::future_status::timeout )
        {
            clock += std::chrono::milliseconds( 500 );
            if ( _monitor )
                try { _monitor( false ); } catch ( ... ) { cleanup(); throw; };
        }
    }

    void stop() override
    {
        _search->stop();
    }

    using DbgCtx = dbg::Context< vm::CowHeap >;

    virtual Trace ce_trace() { return Trace(); }
    virtual Result result() { return Result::None; }
    virtual PoolStats poolstats() { return PoolStats(); }
    virtual HashStats hashstats() { return HashStats(); }
    virtual void dbg_fill( DbgCtx & ) {}
    virtual void start( int ) override = 0;
    virtual ~Job() = default;
};

template< template< typename, typename > class Job_, typename Next >
std::shared_ptr< Job > make_job( std::shared_ptr< BitCode > bc, Next next );

}
