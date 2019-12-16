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
    virtual void check() = 0;
};

struct expect : brq::cmd_base, Expectation
{
    bool _ok = false, _found = true, _on_inactive = false;
    mc::Result _result = mc::Result::None;
    std::string _cmd, _location, _symbol, _trace;
    int _trace_count = 0, _trace_matches = 0;

    void check() override
    {
        if ( !_ok )
            throw Unexpected( _cmd );
        if ( _result == mc::Result::Error && !_found )
            throw Unmatched( _cmd );
        if ( _on_inactive )
            throw FoundInactive( _cmd );
        if ( !_trace.empty() )
            if ( !_trace_matches || ( _trace_count && _trace_count != _trace_matches ) )
                throw NoTrace( _trace, _trace_count );
    }

    virtual void backtrace( ui::DbgContext &ctx, int )
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

    virtual void result( mc::Result r, const mc::Trace &trace )
    {
        _ok = _result == mc::Result::None || r == _result;
        if ( !_trace.empty() )
            for ( auto l : trace.labels )
                if ( l.find( _trace ) != l.npos )
                    ++ _trace_matches;
    }

    void run() override {}
    void options( brq::cmd_options &c ) override
    {
        c.opt( "--result", _result );
        c.opt( "--symbol", _symbol );
        c.opt( "--location", _location );
        c.opt( "--trace-count", _trace_count );
        c.opt( "--trace", _trace );
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

    std::vector< std::shared_ptr< Expectation > > expectations;
    std::vector< std::pair< std::string, std::string > > files;

    for ( auto cmdstr : script )
    {
        auto split = brq::splitter( cmdstr, ' ' ); /* FIXME */
        std::vector< std::string_view > tok;
        std::copy( split.begin(), split.end(), std::back_inserter( tok ) );
        ui::CLI cli( "divine", tok );
        cli._check_files = false;

        auto check_expect = [&]( ui::verify &cmd )
        {
            std::vector< ui::SinkPtr > log( expectations.begin(), expectations.end() );
            log.push_back( cmd._log );
            cmd._log = ui::make_composite( log );
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
                       expectations.emplace_back( std::make_shared< expect >( std::move( e ) ) );
                   } );
        cmd.match( check_expect );
        cmd.match( [&]( ui::command &c ) { c.run(); } );
    }

    for ( auto & expect : expectations )
        expect->check();
}

}
