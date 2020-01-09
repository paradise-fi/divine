// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/sim/command.hpp>
#include <divine/sim/cli.hpp>
#include <brick-cmd>

namespace divine::sim
{
    auto parse( cmd_tokens tok )
    {
        brq::cmd_parser p( "", tok );
        using namespace command;
        return p.parse< exit, start, breakpoint, step, stepi, stepa, rewind, backtrace,
                        show, diff, dot, inspect, tamper, call, info,
                        up, down, set, thread, bitcode, source, setup >();
    }

    void CLI::finalize( command::cast_iron )
    {
        update();
        for ( auto t : _sticky_commands )
        {
            auto cmd = parse( t );
            cmd.match( [&] ( auto opt ){ prepare( opt ); go( opt ); } );
            _stream = &std::cerr;
        }
    }

void CLI::command( cmd_tokens tok )
{
    auto cmd = parse( tok );
    cmd.match( [&] ( brq::cmd_help h ) { h.run(); },
               [&] ( auto opt ) { prepare( opt ); go( opt ); finalize( opt ); } );
    _stream = &std::cerr; /* always revert to the main output */
}

void CLI::command_raw( cmd_tokens tok )
{
    auto cmd = parse( tok );
    cmd.match( [&] ( auto opt ) { go( opt ); } );
}

}
