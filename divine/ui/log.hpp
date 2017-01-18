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

#include <divine/mc/job.hpp>
#include <brick-string>

namespace divine {
namespace ui {

enum class Phase { LART, RR, Constants, Done };

struct LogSink
{
    virtual void progress( int, int, bool ) {}
    virtual void memory( const mc::Job::PoolStats &, bool ) {}
    virtual void loader( Phase ) {}
    virtual void info( std::string ) {}
    virtual void result( mc::Result, const mc::Trace & ) {}
    virtual void start() {}
    virtual int log_id() { return 0; } // only useful for ODBC logs
    virtual ~LogSink() {}
};

using SinkPtr = std::shared_ptr< LogSink >;

SinkPtr nullsink(); /* a global null sink */
SinkPtr make_interactive();
SinkPtr make_yaml( bool detailed );
SinkPtr make_odbc( std::string connstr );
SinkPtr make_composite( std::vector< SinkPtr > );

struct TimedSink : LogSink
{
    using Clock = std::chrono::steady_clock;
    using MSecs = std::chrono::milliseconds;

    Clock::time_point _start;
    MSecs _interval;

    std::string interval_str()
    {
        std::stringstream t;
        t << int( _interval.count() / 60000 ) << ":"
          << std::setw( 2 ) << std::setfill( '0' ) << int( _interval.count() / 1000 ) % 60;
        return t.str();
    }

    double timeavg( double val )
    {
        return 1000 * val / _interval.count();
    }

    std::string timeavg_str( double val )
    {
        std::stringstream s;
        s << std::fixed << std::setprecision( 1 ) << timeavg( val );
        return s.str();
    }

    MSecs update_interval()
    {
        return _interval = std::chrono::duration_cast< MSecs >( Clock::now() - _start );
    }

    void start() override { _start = Clock::now(); }
};

}
}
