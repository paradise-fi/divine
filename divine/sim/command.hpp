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

#pragma once
#include <divine/sim/trace.hpp>
#include <divine/dbg/info.hpp>
#include <brick-cmd>
#include <string>

namespace divine::sim::argument
{
    struct function
    {
        std::string n;

        static brq::parse_result from_string( std::string_view s, function &f )
        {
            if ( !std::isalpha( s[ 0 ] ) && s[ 0 ] != '_' )
                return brq::no_parse( "function names must start with a letter or '_'" );
            f.n = s;
            return {};
        }
    };

    struct breakpoint
    {
        brick::types::Union< std::string, int > id;

        static brq::parse_result from_string( std::string_view s, breakpoint &bp )
        {
            int id;
            if ( brq::from_string( s, id ) )
                bp.id = id;
            else
                bp.id = std::string( s );
            return {};
        }
    };
}

namespace divine::sim::command
{
    struct cast_iron {}; // finalize with sticky commands
    struct teflon       // finalize without sticky commands
    {
        std::string output_to;
        bool clear_screen = false;
    };

    struct base : brq::cmd_base
    {
        void run() override {}
    };

    struct with_var : base
    {
        std::string var;
        with_var( std::string v = "$_" ) : var( v ) {}

        void options( brq::cmd_options &o ) override
        {
            o.collect( var );
        }
    };

    struct with_frame : with_var
    {
        with_frame() : with_var( "$frame" ) {}
    };

    struct set : cast_iron, base
    {
        std::vector< std::string > opt;
        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.collect( opt );
        }
    };

    struct with_steps : with_frame
    {
        brq::cmd_flag over, out, quiet, verbose;
        int count = 1;

        void options( brq::cmd_options &c ) override
        {
            c.section( "Stepping Options" );
            c.opt( "--over",    over )    << "execute calls as a single step";
            c.opt( "--quiet",   quiet )   << "suppress output";
            c.opt( "--verbose", verbose ) << "print individual instructions";
            c.opt( "--out",     out )     << "execute until the current function returns";
            c.opt( "--count",   count )   << "execute the given number of steps (default 1)";
            with_frame::options( c );
        }
    };

    struct start : base, cast_iron
    {
        brq::cmd_flag verbose, noboot;

        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.section( "Start Options" );
            c.opt( "--no-boot", noboot ) << "stop before booting";
            c.opt( "--verbose", verbose ) << "print each instruction as it is executed";
        }
    };

    struct breakpoint : teflon, base
    {
        brq::cmd_flag list;
        std::vector< std::string > where;
        argument::breakpoint del;

        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.section( "Breakpoint Options" );
            c.opt( "--list", list ) << "print currently active breakpoints";
            c.opt( "--delete", del ) << "delete the designated breakpoint(s)";
            c.collect( where );
        }
    };

    struct stepi : with_steps, cast_iron {};
    struct stepa : with_steps, cast_iron {};
    struct step  : with_steps, cast_iron {};

    struct rewind : with_var,  cast_iron
    {
        rewind() : with_var( "#last" ) {}
    };

    struct show : with_var, teflon
    {
        brq::cmd_flag raw;
        int depth = 10, deref = 0;

        void options( brq::cmd_options &c ) override
        {
            c.section( "Display Options" );
            c.opt( "--raw",   raw )   << "dump raw data";
            c.opt( "--depth", depth ) << "maximal component unfolding (default 10)";
            c.opt( "--deref", deref ) << "maximal pointer dereference unfolding (default 0)";
            with_var::options( c );
        }
    };

    struct inspect : show {};

    struct tamper : with_var, teflon
    {
        brq::cmd_flag lift;

        void options( brq::cmd_options &c ) override
        {
            c.section( "Tamper Options" );
            c.opt( "--lift", lift ) << "lift the original value instead of creating a nondet";
            with_var::options( c );
        }
    };

    struct diff : base, teflon
    {
        std::vector< std::string > vars;

        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.collect( vars );
        }
    };

    struct dot : with_var, teflon
    {
        std::string type = "none", output_file;

        void options( brq::cmd_options &c ) override
        {
            c.section( "Dot Options" );
            c.opt( "-T", type ) << "type of output (none, ps, svg, png, ...)";
            c.opt( "-o", output_file ) << "file to write the output to";
            with_var::options( c );
        }
    };

    struct draw : dot
    {
        draw() { type = "x11"; }
    };

    struct bitcode : with_frame, teflon
    {
        std::string filename;

        void options( brq::cmd_options &c ) override
        {
            c.opt( "--dump", filename ) << "save current bitcode into a file";
            with_frame::options( c );
        }
    };

    struct source : with_frame, teflon {};
    struct call : base, teflon
    {
        std::string function;

        void options( brq::cmd_options &c ) override
        {
            c.pos( function );
        }
    };

    struct info : base, teflon
    {
        std::string cmd, setup;

        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.opt( "--setup", setup ) << "set up a new source";
            c.pos( cmd );
        }
    };

    struct thread : base, cast_iron
    {
        std::string spec;
        brq::cmd_flag random;

        void options( brq::cmd_options &c ) override
        {
            base::options( c );
            c.section( "Scheduling Options" );
            c.opt( "--random", random ) << "pick the thread to run randomly";
            c.collect( spec );
        }
    };

    struct backtrace : with_var, teflon
    {
        backtrace() : with_var( "$top" ) {}
    };

    struct setup : base, teflon
    {
        brq::cmd_flag clear_sticky, pygmentize, debug_everything;
        std::string xterm;
        std::vector< std::string > sticky_commands;
        std::vector< dbg::component > debug_components, ignore_components;

        void options( brq::cmd_options &c ) override
        {
            c.section( "Component Options" );
            c.opt( "--debug", debug_components );
            c.opt( "--ignore", ignore_components );
            c.opt( "--debug-everything", debug_everything );

            c.section( "Output Options" );
            c.opt( "--xterm", xterm );
            c.opt( "--pygmentize", pygmentize );
            c.opt( "--clear-sticky", clear_sticky );
            c.opt( "--sticky", sticky_commands );
        }
    };

    struct down : base, cast_iron {};
    struct up   : base, cast_iron {};

    struct exit : base, teflon
    {
        static std::array< std::string, 2 > names()
        {
            return { { std::string( "exit" ), std::string( "quit" ) } };
        }
    };
}
