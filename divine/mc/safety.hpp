
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
#include <divine/mc/trace.hpp>
#include <divine/mc/bitcode.hpp>

namespace divine::mc
{

template< typename Next, typename Builder >
struct Safety : Job
{
    using Parent = std::atomic< vm::CowHeap::Snapshot >;
    using MasterPool = typename vm::CowHeap::SnapPool;
    using SlavePool = brick::mem::SlavePool< MasterPool >;
    using StateTrace = mc::StateTrace< Builder >;

    Builder _ex;
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

    template< typename... Args >
    Safety( std::shared_ptr< BitCode > bc, Next next, Args... builder_opts )
        : _ex( bc, builder_opts... ),
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

        stats = [=]()
        {
            int64_t st = _ex._d.total_states->load();
            int64_t mip = _ex._d.total_instructions->load();
            search->ws_each( [&]( auto &bld, auto & )
            {
                st += bld._d.local_states;
                mip += bld._d.local_instructions;
            } );
            return std::make_pair( st, mip );
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
        while ( i != _ex._d.initial.snap )
        {
            rv.emplace_front( i, std::nullopt );
            i = *_ext.machinePointer< vm::CowHeap::Snapshot >( i );
        }
        rv.emplace_front( _ex._d.initial.snap, std::nullopt );
        return mc::trace( _ex, rv );
    }

    void dbg_fill( DbgCtx &dbg ) override { dbg.load( _ex.pool(), _ex.context() ); }

    Result result() override
    {
        if ( !stats().first )
            return Result::BootError;
        return _error_found ? Result::Error : Result::Valid;
    }

    virtual PoolStats poolstats() override
    {
        return PoolStats{ { "snapshot memory", _ex.pool().stats() },
                          { "fragment memory", _ex.context().heap().mem_stats() } };
    }

    template< typename HT >
    std::pair< int64_t, int64_t > hashuse( HT ht )
    {
        int64_t used = 0, capacity = ht.capacity();
        for ( auto i = 0; i < capacity; ++i )
            if ( !ht.cellAt( i ).empty() )
                ++ used;
        return { used, capacity };
    }

    virtual HashStats hashstats() override
    {
        return HashStats{ { "snapshot table", _ex._d.states.stats() },
                          { "fragment table", _ex.context().heap().ht_stats() } };
    }
};

}

#ifdef BRICK_UNITTEST_REG
#include <divine/mc/job.tpp>
#include <divine/mc/builder.hpp>

namespace divine::t_mc
{

struct TestSafety
{
    TEST( simple )
    {
        auto bc = prog_int( "4", "*r - 1" );
        int edgecount = 0, statecount = 0;
        auto safe = mc::make_job< mc::Safety >(
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

#endif
