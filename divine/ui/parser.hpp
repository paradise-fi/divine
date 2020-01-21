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
    brq::cmd_parser _parser;

    template< typename args_t >
    CLI( std::string_view argv_0, const args_t &args ) : _parser( argv_0, args, help() ) {}
    CLI( int argc, const char **argv ) : _batch( true ), _parser( argc, argv, help() ) {}

    std::string_view help()
    {
        return "DIVINE is an LLVM-based model checker. This means it can directly work with\n"
               "your C and C++ software. For small programs (those that fit in a single source\n"
               "file), you can use 'check' or 'verify' directly, without any intermediate steps.\n"
               "For larger projects, it is recommended that you use `dioscc` to build the project\n"
               "first: the resulting binaries can then be loaded into DIVINE, like this:\n\n"
               "    $ ./configure CC=dioscc\n"
               "    $ make # builds ./widget\n"
               "    $ divine check ./widget\n\n"
               "If `check` or `verify` find a problem in your program, they will print basic info\n"
               "about the error and a more detailed report into a file. You can use the `sim`\n"
               "command to analyse the detailed report, like this:\n\n"
               "    $ divine sim --load-report widget.report";
    }

    template< typename... extra_cmds >
    auto parse()
    {
        return _parser.parse< verify, check, exec, sim, draw, info, cc,
                              version, ltlc, refine, extra_cmds... >();
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
