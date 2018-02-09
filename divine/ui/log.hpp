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

#include <divine/mc/types.hpp>
#include <divine/ui/util.hpp>
#include <brick-string>

namespace divine::dbg { template< typename > struct Context; }

namespace divine::ui
{

enum class Phase { LART, RR, Constants, Done };
using DbgContext = dbg::Context< vm::CowHeap >;

struct LogSink
{
    virtual void progress( std::pair< int64_t, int64_t >, int, bool ) {}
    virtual void memory( const mc::PoolStats &, bool ) {}
    virtual void loader( Phase ) {}
    virtual void info( std::string, bool = false ) {}
    virtual void result( mc::Result, const mc::Trace & ) {}
    virtual void backtrace( DbgContext &, int ) {}
    virtual void start() {}
    virtual int log_id() { return 0; } // only useful for ODBC logs
    virtual ~LogSink() {}
};

using SinkPtr = std::shared_ptr< LogSink >;

SinkPtr nullsink(); /* a global null sink */
SinkPtr make_interactive();
SinkPtr make_yaml( std::ostream &output, bool detailed );
SinkPtr make_composite( std::vector< SinkPtr > );

struct TimedSink : LogSink
{
    Clock::time_point _start;
    MSecs _interval{ 0 };
    MSecs _time_lart{ 0 }, _time_rr{ 0 }, _time_const{ 0 }, _time_boot{ 0 },
          _time_search{ 0 }, _time_ce{ 0 };

    double timeavg( double val, MSecs timer )
    {
        return 1000 * val / timer.count();
    }

    std::string timeavg_str( double val, MSecs timer )
    {
        std::stringstream s;
        s << std::fixed << std::setprecision( 1 ) << timeavg( val, timer );
        return s.str();
    }

    MSecs msecs( Clock::time_point from, Clock::time_point to )
    {
        return std::chrono::duration_cast< MSecs >( to - from );
    }

    MSecs reset_interval()
    {
        auto start = _start;
        _start = Clock::now();
        return _interval = msecs( start, _start );
    }

    MSecs interval() { return msecs( _start, Clock::now() ); }
    void start() override { _time_boot = reset_interval(); }
    void result( mc::Result, const mc::Trace & ) override { _time_ce = reset_interval(); }

    void progress( std::pair< int64_t, int64_t >, int, bool last ) override
    {
        if ( last )
            _time_search = reset_interval();
    }

    void loader( Phase p ) override
    {
        switch ( p )
        {
            case Phase::LART:      reset_interval(); break;
            case Phase::RR:        _time_lart  = reset_interval(); break;
            case Phase::Constants: _time_rr    = reset_interval(); break;
            case Phase::Done:      _time_const = reset_interval(); break;
        }
    }

    virtual void set_result( mc::Result, long ) { }
};

}
