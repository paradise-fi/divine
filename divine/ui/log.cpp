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
#include <divine/ui/util.hpp>

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

void printpool( std::ostream &ostr, std::string name, const brick::mem::Stats &s )
{
    ostr << name << ":" << std::endl;
    ostr << "  total: " << printitem( s.total ) << std::endl;
    for ( auto i : s )
        if ( i.count.held )
            ostr << "  " << i.size << ": " << printitem( i ) << std::endl;
}

/* format a yaml report */
struct YamlSink : TimedSink
{
    bool _detailed;
    mc::Job::PoolStats latest;
    SysInfo _sysinfo;
    std::ostream &_out;

    YamlSink( std::ostream &o, bool detailed ) : _detailed( detailed ), _out( o ) {}

    void progress( int states, int q, bool last ) override
    {
        TimedSink::progress( states, q, last );

        if ( !last )
            return;

        _out << std::endl << "state count: " << states
             << std::endl << "states per second: " << timeavg( states, _time_search )
             << std::endl << "version: " << version()
             << std::endl << std::endl;
        _sysinfo.report( [this]( auto k, auto v )
                         { _out << k << ": " << v << std::endl; } );
    }

    void memory( const mc::Job::PoolStats &st, bool last ) override
    {
        if ( !last || !_detailed )
            return;
        for ( auto p : st )
            printpool( _out, p.first, p.second );
    }

    void result( mc::Result result, const mc::Trace &trace ) override
    {
        TimedSink::result( result, trace );
        _out << "timers:";
        _out << std::setprecision( 3 );
        _out << std::endl << "  lart: " << double( _time_lart.count() ) / 1000
             << std::endl << "  loader: " << double( _time_rr.count() + _time_const.count() ) / 1000
             << std::endl << "  boot: " << double( _time_boot.count() ) / 1000
             << std::endl << "  search: " << double( _time_search.count() ) / 1000
             << std::endl << "  ce: " << double( _time_ce.count() ) / 1000 << std::endl;

        _out << result << std::endl;
        if ( result == mc::Result::None || result == mc::Result::Valid )
            return;
        _out << "error trace: |" << std::endl;
        for ( auto l : trace.labels )
            _out << "  " << l << std::endl;
        if ( !trace.bootinfo.empty() )
            _out << "boot info:\n" << trace.bootinfo << std::endl;
        _out << std::endl;
        _out << "choices made:" << trace.choices << std::endl;
    }

};

/* print progress updates to stderr */
struct InteractiveSink : TimedSink
{
    virtual void progress( int states, int queued, bool last ) override
    {
        TimedSink::progress( states, queued, last );
        if ( last )
            std::cerr << "\rfound " << states
                      << " states in " << interval_str( _time_search )
                      << ", averaging " << timeavg_str( states, _time_search )
                      << "                             " << std::endl;
        else
            std::cerr << "\rsearching: " << states
                      << " states found in " << interval_str( interval() )
                      << ", averaging " << timeavg_str( states, interval() )
                      << ", queued: " << queued << "      ";
    }

    void loader( Phase p ) override
    {
        switch ( p )
        {
            case Phase::LART:      std::cerr << "loading bitcode … LART … " << std::flush; break;
            case Phase::RR:        std::cerr << "RR … " << std::flush; break;
            case Phase::Constants: std::cerr << "constants … " << std::flush; break;
            case Phase::Done:      std::cerr << "done" << std::endl << "booting … "; break;
        }
    }

    virtual void start() override
    {
        TimedSink::start();
        std::cerr << "done" << std::endl;
    }

    virtual void info( std::string ) override {}
};

struct NullSink : LogSink {};

SinkPtr nullsink()
{
    static SinkPtr global;
    if ( !global )
        global = std::make_shared< NullSink >();
    return global;
}

SinkPtr make_yaml( std::ostream &o, bool d ) { return std::make_shared< YamlSink >( o, d ); }
SinkPtr make_interactive() { return std::make_shared< InteractiveSink >(); }

SinkPtr make_composite( std::vector< SinkPtr > s )
{
    auto rv = std::make_shared< CompositeSink >();
    rv->_slaves = s;
    return rv;
}

}
