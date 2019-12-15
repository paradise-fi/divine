// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <vikiv@mail.muni.cz>
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
#include <divine/ui/cli.hpp>

namespace divine::ui
{

struct CLI : Interface
{
    bool _batch, _check_files = true;
    int _argc;
    const char **_argv;

    CLI( int argc, const char **argv ) :
        _batch( true ), _argc( argc ), _argv( argv )
    {}

    auto parse()
    {
        if ( _argc > 1 && ( _argv[ 1 ] == "--help" || _argv[ 1 ] == "--version" ) )
            _argv[ 1 ] += 2;

        brq::cmd_parser p( _argc, _argv );
        return p.parse< verify, check, exec, sim, draw, info, cc, version, ltlc >();
    }

    std::shared_ptr< Interface > resolve() override
    {
        if ( _batch )
            return Interface::resolve();
        else
            return std::make_shared< ui::Curses >( /* ... */ );
    }

    virtual int main() override try
    {
        auto cmd = parse();

        cmd.match( [&]( brq::cmd_help &help )
                   {
                       help.run();
                   },
                   [&]( command &c )
                   {
                       c.setup();
                       c.run();
                       c.cleanup();
                   } );
        return 0;
    }
    catch ( brq::error &e )
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        exception( e );
        return e._exit;
    }
};

}
