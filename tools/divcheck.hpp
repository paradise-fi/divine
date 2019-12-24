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

#include <divine/ui/cli.hpp>
#include <divine/ui/parser.hpp>
#include <divine/mc/trace.hpp>

#include <iostream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-cmd>
#include <brick-fs>

namespace divcheck
{

using namespace divine;

struct Wrong : std::exception
{
    Wrong( std::string s = "" ) : _err( s ) {}
    std::string _err;
    const char *what() const noexcept { return _err.c_str(); }
};

struct Unexpected : Wrong
{
    Unexpected( std::string s ) { _err = "unexpected result from: " + s; }
};

struct Unmatched : Wrong
{
    Unmatched( std::string s ) { _err = "the expected error does not match: " + s; }
};

struct FoundInactive : Wrong
{
    FoundInactive( std::string s ) { _err = "error location found, but on an inactive stack: " + s; }
};

struct NoTrace : Wrong
{
    NoTrace( std::string t, int c )
        : Wrong( "'" + t + "' not found in the trace" +
                 ( c ? " exactly " + std::to_string( c ) + " time(s)" : "" ) )
    {}
};

struct Expectation : ui::LogSink
{
    // Throw if expectation has not been met
    virtual void check( const std::string & ) = 0;
    // Revert inner state so that the same expectation can be checked for a new command
    virtual void setup() {};
};

struct ResultExpectation : Expectation
{
    bool _ok;
    mc::Result _result = mc::Result::None;

    void setup() override
    {
        _ok = false;
    }

    void check( const std::string & cmd ) override
    {
        if ( !_ok )
            throw Unexpected( cmd );
    }

    void result( mc::Result r, const mc::Trace & ) override
    {
        _ok = _result == mc::Result::None || r == _result;
    }
};

struct BacktraceExpectation : Expectation
{
    bool _found, _on_inactive;
    std::string _location, _symbol;

    void setup() override
    {
        _found = true;
        _on_inactive = false;
    }

    void check( const std::string & cmd ) override
    {
        if ( _location.empty() || _symbol.empty() )
            return;
        if ( !_found )
            throw Unmatched( cmd );
        if ( _on_inactive )
            throw FoundInactive( cmd );
    }

    void backtrace( ui::DbgContext &ctx, int ) override
    {
        if ( _location.empty() && _symbol.empty() )
            return;

        _found = false;
        bool active = true;
        auto bt = [&]( int ) { active = false; };
        auto chk = [&]( auto dn )
        {
            if ( dn.attribute( "location" ) == _location ||
                 !_symbol.empty() && dn.attribute( "symbol" ).find( _symbol ) != std::string::npos )
            {
                if ( !active && !_found )
                    _on_inactive = true;
                _found = true;
            }
        };
        mc::backtrace( bt, chk, ctx, ctx.snapshot(), 10000 );
    }
};

struct TraceExpectation : Expectation
{
    int _trace_count = 0, _trace_matches;
    std::string _trace;

    void setup() override
    {
        _trace_matches = 0;
    }

    void check( const std::string & ) override
    {
        if ( !_trace.empty() )
            if ( !_trace_matches || ( _trace_count && _trace_count != _trace_matches ) )
                throw NoTrace( _trace, _trace_count );
    }

    void result( mc::Result, const mc::Trace &trace ) override
    {
        if ( !_trace.empty() )
            for ( auto l : trace.labels )
                if ( l.find( _trace ) != l.npos )
                    ++ _trace_matches;
    }

};

struct expect : ui::CompositeMixin< expect >, brq::cmd_base
{
    std::string _cmd;
    ResultExpectation _result;
    BacktraceExpectation _backtrace;
    TraceExpectation _trace;
    bool _warn = false;

    std::array< Expectation *, 3 > expectations()
    {
        return { &_result, &_backtrace, &_trace };
    }

    void setup()
    {
        for ( auto e : expectations() )
            e->setup();
    }

    void check()
    {
        try
        {
            for ( auto e : expectations() )
                e->check( _cmd );
        }
        catch ( Wrong &e )
        {
            if ( _warn )
                std::cerr << "W: " << e.what();
            else
                throw;
        }
    }

    template< typename L >
    void each( L l )
    {
        for ( auto e: expectations() )
            l( e );
    }

    void run() override {}
    void options( brq::cmd_options &c ) override
    {
        c.opt( "--result", _result._result );
        c.opt( "--symbol", _backtrace._symbol );
        c.opt( "--location", _backtrace._location );
        c.opt( "--trace-count", _trace._trace_count );
        c.opt( "--trace", _trace._trace );
    }
};

struct load : brq::cmd_base
{
    brq::cmd_file from;
    brq::cmd_path to;

    void run() override {}
    void options( brq::cmd_options &c ) override
    {
        c.pos( from );
        c.pos( to );
    }
};

std::vector< std::string > parse( std::string txt )
{
    std::vector< std::string > script;

    for ( auto line : brq::splitter( txt, '\n' ) )
    {
        if ( line.empty() || line.at( 0 ) == '#' )
            continue;
        if ( line.at( 0 ) == ' ' && !script.empty() )
            script.back() += line;
        else
            script.emplace_back( line );
    }
    return script;
}

template< typename... F >
void execute( std::string script_txt, F... prepare )
{
    auto script = parse( script_txt );

    std::vector< std::shared_ptr< expect > > expects;
    std::vector< std::pair< std::string, std::string > > files;

    for ( auto cmdstr : script )
    {
        auto split = brq::splitter( cmdstr, ' ' ); /* FIXME */
        std::vector< std::string_view > tok;
        std::copy( split.begin(), split.end(), std::back_inserter( tok ) );
        ui::CLI cli( "divine", tok );
        cli._check_files = false;

        auto prepare_expects = [&]( ui::with_bc &cmd )
        {
            std::vector< ui::SinkPtr > log( expects.begin(), expects.end() );
            log.push_back( cmd._log );
            cmd._log = ui::make_composite( log );
            for ( auto & expect : expects )
                expect->setup();
        };

        auto check_expects = [&]()
        {
            for ( auto & expect : expects )
                expect->check();
        };

        auto cmd = cli.parse< expect, load >();
        auto load_file = [&]( load &l )
        {
            files.emplace_back( l.to.name, brq::read_file( l.from.name ) );
            brq::create_file( l.to.name ); /* FIXME trick the CLI parser */
        };

        cmd.match( prepare..., load_file,
                   [&]( ui::cc &cc ) { cc._driver.setupFS( files ); },
                   [&]( ui::with_bc &wbc ) { wbc._cc_driver.setupFS( files ); } );
        cmd.match( [&]( ui::command &c ) { c.setup(); },
                   [&]( expect e )
                   {
                       e._cmd = cmdstr;
                       expects.emplace_back( std::make_shared< expect >( std::move( e ) ) );
                   } );
        cmd.match( [&]( ui::with_bc &cmd ) { prepare_expects( cmd ); } );
        cmd.match( [&]( ui::command &c ) { c.run(); } );
        cmd.match( [&]( ui::with_bc &cmd ) { check_expects(); } );
    }
}

}
