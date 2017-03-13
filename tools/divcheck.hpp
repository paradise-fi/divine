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
#include <divine/cc/compile.hpp>

#include <iostream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-cmd>

namespace divcheck
{

using namespace divine;

std::vector< std::string > parse( std::string txt )
{
    std::vector< std::string > script;
    brick::string::Splitter split( "\n", std::regex::extended );
    for( auto it = split.begin( txt ); it != split.end(); ++ it )
    {
        auto line = it->str();
        if ( line.empty() || line.at( 0 ) == '#' )
            continue;
        if ( line.at( 0 ) == ' ' && !script.empty() )
            script.back() += line;
        else
            script.push_back( line );
    }
    return script;
}

template< typename P, typename F >
void execute( std::string script_txt, P prepare_cc, F execute )
{
    auto script = parse( script_txt );
    brick::string::Splitter split( "[ \t\n]+", std::regex::extended );

    for ( auto cmdstr : script )
    {
        std::vector< std::string > tok;
        std::copy( split.begin( cmdstr ), split.end(), std::back_inserter( tok ) );
        ui::CLI cli( tok );

        auto cmd = cli.parse( cli.commands() );
        cmd.match( [&]( ui::Cc &cc ) { prepare_cc( cc ); } );
        cmd.match( [&]( ui::Command &c ) { c.setup(); } );
        cmd.match( [&]( ui::Verify &v ) { execute( v ); },
                   [&]( ui::Run &r ) { execute( r ); },
                   [&]( ui::Cc &cc ) { cc.run(); },
                   [&]( ui::Command & )
                   {
                       throw brick::except::Error( "unsupported command " + cmdstr );
                   } );
    }
}

}
