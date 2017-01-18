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

#include <divine/ui/log.hpp>
#include <divine/ui/version.hpp>
#include <divine/ui/sysinfo.hpp>

namespace divine::ui
{

struct CompositeSink : LogSink
{
    std::vector< SinkPtr > _slaves;

    template< typename L >
    void each( L l )
    {
        for ( auto sink: _slaves )
            l( sink );
    }

    void progress( int x, int y, bool l ) override
    { each( [&]( auto s ) { s->progress( x, y, l ); } ); }

    void memory( const mc::Job::PoolStats &st, bool l ) override
    { each( [&]( auto s ) { s->memory( st, l ); } ); }

    void info( std::string i ) override
    { each( [&]( auto s ) { s->info( i ); } ); }

    void loader( Phase p ) override
    { each( [&]( auto s ) { s->loader( p ); } ); }

    void result( mc::Result res, const mc::Trace &tr ) override
    { each( [&]( auto s ) { s->result( res, tr ); } ); }

    void start() override
    { each( [&]( auto s ) { s->start(); } ); }
};


template< typename S >
std::string printitem( S s )
{
    std::stringstream str;
    str << "{ items: " << s.count.used << ", used: " << s.bytes.used
        << ", held: " << s.bytes.held << " }";
    return str.str();
}

void printpool( std::string name, const brick::mem::Stats &s )
{
    std::cout << name << ":" << std::endl;
    std::cout << "  total: " << printitem( s.total ) << std::endl;
    for ( auto i : s )
        if ( i.count.held )
            std::cout << "  " << i.size << ": " << printitem( i ) << std::endl;
}

/* format a yaml report */
struct YamlSink : TimedSink
{
    bool _detailed;
    mc::Job::PoolStats latest;
    SysInfo _sysinfo;

    MSecs _lart, _rr, _constants, _search;

    YamlSink( bool detailed ) : _detailed( detailed ) {}

    void progress( int states, int, bool last ) override
    {
        if ( !last )
            return;

        _search = update_interval();
        std::cout << std::endl << "state count: " << states
                  << std::endl << "states per second: " << timeavg( states )
                  << std::endl << "version: " << version()
                  << std::endl << std::endl;
        _sysinfo.report( []( auto k, auto v )
                         { std::cout << k << ": " << v << std::endl; } );
        TimedSink::start();
    }

    void memory( const mc::Job::PoolStats &st, bool last ) override
    {
        if ( !last || !_detailed )
            return;
        for ( auto p : st )
            printpool( p.first, p.second );
    }

    void result( mc::Result result, const mc::Trace &trace ) override
    {
        std::cout << "timers:";
        std::cout << std::setprecision( 3 );
        std::cout << std::endl << "  search: " << double( _search.count() ) / 1000
                  << std::endl << "  lart: " << double( _lart.count() ) / 1000
                  << std::endl << "  loader: " << double( _rr.count() + _constants.count() ) / 1000
                  << std::endl << "  ce: " << double( _interval.count() ) / 1000 << std::endl;
        std::cout << result << std::endl;
        if ( result == mc::Result::None || result == mc::Result::Valid )
            return;
        std::cout << "error trace: |" << std::endl;
        for ( auto l : trace.labels )
            std::cout << "  " << l << std::endl;
        if ( !trace.bootinfo.empty() )
            std::cout << "boot info:" << trace.bootinfo << std::endl;
        std::cout << std::endl;
        std::cout << "choices made:" << trace.choices << std::endl;
    }

    void loader( Phase p ) override
    {
        switch ( p )
        {
            case Phase::LART:      TimedSink::start(); break;
            case Phase::RR:        _lart = update_interval(); TimedSink::start(); break;
            case Phase::Constants: _rr = update_interval();   TimedSink::start(); break;
            case Phase::Done:      _constants = update_interval(); break;
        }
    }

};

/* print progress updates to stderr */
struct InteractiveSink : TimedSink
{
    virtual void progress( int states, int queued, bool last ) override
    {
        update_interval();
        if ( last )
            std::cerr << "\rfound " << states
                      << " states in " << interval_str()
                      << ", averaging " << timeavg_str( states )
                      << "                             " << std::endl << std::endl;
        else
            std::cerr << "\rsearching: " << states
                      << " states found in " << interval_str()
                      << ", averaging " << timeavg_str( states )
                      << ", queued: " << queued << "      ";
    }

    void loader( Phase p ) override
    {
        switch ( p )
        {
            case Phase::LART:      std::cerr << "loading bitcode … LART … " << std::flush; break;
            case Phase::RR:        std::cerr << "RR … " << std::flush; break;
            case Phase::Constants: std::cerr << "constants … " << std::flush; break;
            case Phase::Done:      std::cerr << "done" << std::endl; break;
        }
    }

    virtual void info( std::string ) override {}
    virtual void start() override { TimedSink::start(); }
};

struct NullSink : LogSink {};

SinkPtr nullsink()
{
    static SinkPtr global;
    if ( !global )
        global = std::make_shared< NullSink >();
    return global;
}

SinkPtr make_yaml( bool d ) { return std::make_shared< YamlSink >( d ); }
SinkPtr make_interactive() { return std::make_shared< InteractiveSink >(); }

SinkPtr make_composite( std::vector< SinkPtr > s )
{
    auto rv = std::make_shared< CompositeSink >();
    rv->_slaves = s;
    return rv;
}

}
