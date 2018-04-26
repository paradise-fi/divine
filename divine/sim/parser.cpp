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

namespace cmd = brick::cmd;
using namespace std::literals;

auto make_parser()
{
    auto v = cmd::make_validator()->
            add( "function", []( std::string s, auto good, auto bad )
                {
                    if ( isdigit( s[0] ) ) /* FIXME! zisit, kde sa to rozbije */
                        return bad( cmd::BadFormat, "function names cannot start with a digit" );
                    return good( s );
                } )->
            add( "step", []( std::string, auto, auto bad )
                {
                    return bad( cmd::BadFormat, "not implemented yet" );
                } )->
            add( "component", []( std::string s, auto good, auto bad )
                {
                    if ( s == "kernel" ) return good( dbg::Component::Kernel );
                    if ( s == "dios" ) return good( dbg::Component::DiOS );
                    if ( s == "libc" ) return good( dbg::Component::LibC );
                    if ( s == "libcxx" ) return good( dbg::Component::LibCxx );
                    if ( s == "librst" ) return good( dbg::Component::LibRst );
                    return bad( cmd::BadFormat, "unknown component" );
                } );

    auto teflopts = cmd::make_option_set< command::Teflon >( v )
        .option( "[--clear-screen]", &command::Teflon::clear_screen,
                    "clear screen before writing"s )
        .option( "[--output-to {string}]", &command::Teflon::output_to,
                    "write output to a given stream"s );
    auto varopts = cmd::make_option_set< command::WithVar >( v )
        .option( "[{string}]", &command::WithVar::var, "a variable reference"s );
    auto breakopts = cmd::make_option_set< command::Break >( v )
        .option( "[--delete {int}|--delete {function}]",
                &command::Break::del, "delete the designated breakpoint(s)"s )
        .option( "[--list]", &command::Break::list, "list breakpoints"s );
    auto showopts = cmd::make_option_set< command::Show >( v )
        .option( "[--raw]", &command::Show::raw, "dump raw data"s )
        .option( "[--depth {int}]", &command::Show::depth, "maximal component unfolding"s )
        .option( "[--deref {int}]", &command::Show::deref,
                    "maximal pointer dereference unfolding"s );
    auto callopts = cmd::make_option_set< command::Call >( v )
        .option( "{string}", &command::Call::function, "the function to call"s );
    auto stepopts = cmd::make_option_set< command::WithSteps >( v )
        .option( "[--over]", &command::WithSteps::over, "execute calls as one step"s )
        .option( "[--quiet]", &command::WithSteps::quiet, "suppress output"s )
        .option( "[--verbose]", &command::WithSteps::verbose, "increase verbosity"s )
        .option( "[--count {int}]", &command::WithSteps::count,
                    "execute {int} steps (default = 1)"s );
    auto startopts = cmd::make_option_set< command::Start >( v )
        .option( "[--verbose]", &command::Start::verbose, "increase verbosity"s )
        .option( "[--no-boot]", &command::Start::noboot, "stop before booting"s );
    auto stepoutopts = cmd::make_option_set< command::WithSteps >( v )
        .option( "[--out]", &command::WithSteps::out,
                    "execute until the current function returns"s );
    auto threadopts = cmd::make_option_set< command::Thread >( v )
        .option( "[--random]", &command::Thread::random, "pick the thread to run randomly"s )
        .option( "[{string}]", &command::Thread::spec, "stick to the given thread"s );
    auto setupopts = cmd::make_option_set< command::Setup >( v )
        .option( "[--debug {component}]", &command::Setup::debug_components,
                 "enable debugging of {component}"s )
        .option( "[--ignore {component}]", &command::Setup::ignore_components,
                 "disable debugging of {component}"s )
        .option( "[--debug-everything]", &command::Setup::debug_everything,
                 "enable debugging of all components"s )
        .option( "[--xterm {string}]", &command::Setup::xterm, "setup & name an X terminal"s )
        .option( "[--clear-sticky]", &command::Setup::clear_sticky, "remove sticky commands"s )
        .option( "[--pygmentize]", &command::Setup::pygmentize, "pygmentize source listings"s )
        .option( "[--sticky {string}]", &command::Setup::sticky_commands,
                    "run given commands after each step"s );

    auto o_trace = cmd::make_option_set< Trace >( v )
        .option( "[{step}+]", &Trace::steps, "step descriptions" );
    auto o_trace_cmd = cmd::make_option_set< command::Trace >( v )
        .option( "--choices {int}+", &command::Trace::simple_choices, "use simple choices" )
        .option( "[--from {string}]", &command::Trace::from,
                    "start in a given state, instead of initial"s );

    auto o_info = cmd::make_option_set< command::Info >( v )
        .option( "{string}", &command::Info::cmd, "what info to print" )
        .option( "[--setup {string}]", &command::Info::setup, "set up a new info source" );

    auto o_diff = cmd::make_option_set< command::Diff >( v )
        .option( "{string}", &command::Diff::vars, "what to diff" );

    auto dotopts = cmd::make_option_set< command::Dot >( v )
        .option( "[-T{string}|-T {string}]", &command::Dot::type,
                "type of output (none,ps,svg,png…)"s )
        .option( "[-o{string}|-o {string}]", &command::Dot::output_file,
                "file to write output to"s );

    return cmd::make_parser( v )
        .command< command::Exit >( "exit from divine"s )
        .command< command::Help >( "show this help or describe a particular command in more detail"s,
                                cmd::make_option( v, "[{string}]", &command::Help::_cmd ) )
        .command< command::Start >( "boot the system and stop at main()"s, startopts )
        .command< command::Break >( "insert a breakpoint"s, &command::Break::where, breakopts )
        .command< command::StepA >( "execute one atomic action"s, stepopts )
        .command< command::Step >( "execute source line"s, varopts, stepopts, stepoutopts )
        .command< command::StepI >( "execute one instruction"s, varopts, stepopts )
        .command< command::Rewind >( "rewind to a stored program state"s, varopts )
        .command< command::Set >( "set a variable "s, &command::Set::options )
        .command< command::BitCode >( "show the bitcode of the current function"s, varopts )
        .command< command::Source >( "show the source code of the current function"s,
                                        teflopts, varopts )
        .command< command::Thread >( "control thread scheduling"s, threadopts )
        .command< command::Trace >( "load a counterexample trace"s, o_trace, o_trace_cmd )
        .command< command::Show >( "show an object"s, teflopts, varopts, showopts )
        .command< command::Diff >( "diff two objects"s, teflopts, o_diff )
        .command< command::Call >( "run a custom information-gathering function"s,
                                    teflopts, callopts )
        .command< command::Info >( "print information"s, o_info )
        .command< command::Draw >( "draw a portion of the heap"s, varopts )
        .command< command::Dot >( "draw a portion of the heap to a file of given type"s,
                                    teflopts, varopts, dotopts )
        .command< command::Setup >( "set configuration options"s, setupopts )
        .command< command::Inspect >( "like show, but also set $_"s, teflopts, varopts, showopts )
        .command< command::BackTrace >( "show a stack trace"s, teflopts, varopts )
        .command< command::Up >( "move up the stack (towards caller)"s )
        .command< command::Down >( "move down the stack (towards callee)"s );
}

void CLI::finalize( command::CastIron )
{
    update();
    auto parser = make_parser();
    for ( auto t : _sticky_commands )
    {
        auto cmd = parser.parse( t.begin(), t.end() );
        cmd.match( [&] ( auto opt ){ prepare( opt ); go( opt ); } );
        _stream = &std::cerr;
    }
}

void CLI::command( cmd::Tokens tok )
{
    auto parser = make_parser();
    auto cmd = parser.parse( tok.begin(), tok.end() );
    cmd.match( [&] ( command::Help h ) { help( parser, h._cmd ); },
               [&] ( auto opt ) { prepare( opt ); go( opt ); finalize( opt ); } );
    _stream = &std::cerr; /* always revert to the main output */
}

void CLI::command_raw( cmd::Tokens tok )
{
    auto parser = make_parser();
    auto cmd = parser.parse( tok.begin(), tok.end() );
    cmd.match( [&] ( auto opt ) { go( opt ); } );
}

}
