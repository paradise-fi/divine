// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <>
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

#include <divine/sim/cli.hpp>

#include <divine/ui/cli.hpp>
#include <divine/ui/logo.hpp>

#include <brick-string>
#include <brick-yaml>

#include <cstring>
#include <histedit.h>
#include <sys/stat.h>
#include <pwd.h>

namespace divine::sim
{

using namespace std::literals;
namespace cmd = brick::cmd;

bool *CLI::_sigint = nullptr;

char *prompt( EditLine *el )
{
    CLI *interp;
    el_get( el, EL_CLIENTDATA, &interp );
    return interp->prompt();
}

void sigint_handler( int raised )
{
    if ( raised != SIGINT )
        abort();
    if ( CLI::_sigint )
        *CLI::_sigint = true;
}

}

namespace divine::ui
{

void Sim::run()
{
    auto el = el_init( "divine", stdin, stdout, stderr );
    auto hist = history_init();
    HistEvent hist_ev;
    sim::CLI interp( bitcode() );
    interp._batch = _batch;

    std::string hist_path, init_path, dotfiles;

    if ( passwd *p = getpwuid( getuid() ) )
    {
        dotfiles = p->pw_dir;
        dotfiles += "/.divine/";
        mkdir( dotfiles.c_str(), 0777 );
        hist_path = dotfiles + "sim.history";
        init_path = dotfiles + "sim.init";
    }

    history( hist, &hist_ev, H_SETSIZE, 1000 );
    history( hist, &hist_ev, H_LOAD, hist_path.c_str() );
    el_set( el, EL_HIST, history, hist );
    el_set( el, EL_PROMPT, sim::prompt );
    el_set( el, EL_CLIENTDATA, &interp );
    el_set( el, EL_EDITOR, "emacs" );
    el_source( el, nullptr );
    signal( SIGINT, sim::sigint_handler );

    std::cerr << logo << std::endl;
    std::cerr << "Welcome to 'divine sim', an interactive debugger. Type 'help' to get started."
              << std::endl;

    sim::OneLineTokenizer tok;

    if ( !init_path.empty() && !_skip_init )
    {
        std::ifstream init( init_path.c_str() );
        std::string line;
        if ( init.is_open() )
            while ( std::getline( init, line ), !init.eof() )
                interp.command( tok.tokenize( line ) );
    }

    if ( _trace )
        interp.trace( *_trace, false, true, [&]() { interp.reach_error(); } );

    interp.finalize( sim::command::CastIron() );

    while ( !interp._exit )
    {
        interp.info();
        int sz;
        const char *cmd_ = el_gets( el, &sz );
        if ( !cmd_ || !sz ) /* EOF */
            break;

        std::string cmd = cmd_;

        if ( cmd == "\n" )
        {
            history( hist, &hist_ev, H_FIRST );
            cmd = hist_ev.str;
        }
        else
            history( hist, &hist_ev, H_ENTER, cmd_ );

        if ( _batch )
            std::cerr << "> " << cmd << std::flush;

        try
        {
            interp.command( tok.tokenize( cmd ) );
        }
        catch ( brick::except::Error &e )
        {
            std::cerr << "ERROR: " << e.what() << std::endl;
        }
    }

    history( hist, &hist_ev, H_SAVE, hist_path.c_str() );
    history_end( hist );
    el_end( el );
}

void Sim::process_options()
{
    if ( !_load_report )
        return WithBC::process_options();

    std::string yaml = brick::fs::readFile( _file );
    brick::yaml::Parser parsed( yaml );
    _file = parsed.get< std::string >( { "input file" } );

    if ( !_env.empty() || !_useropts.empty() || !_systemopts.empty() )
        throw brick::except::Error( "ERROR: --load-report is incompatible with passing "
                                    "options to the program" );

    std::vector< std::pair< std::string, std::string > > env;
    parsed.get( { "input options", "*" }, env );
    for ( auto p : env )
        _bc_env.emplace_back( p.first, std::vector< uint8_t >( p.second.begin(), p.second.end() ) );

    _ccopts_final = parsed.getOr( { "compile options", "*" }, _ccopts_final );
    _lartPasses = parsed.getOr( { "lart passes", "*" }, _lartPasses );
    _relaxed = parsed.getOr( { "relaxed memory" }, _relaxed );
    _symbolic = parsed.getOr( { "symbolic" }, _symbolic );
    _svcomp = parsed.getOr( { "svcomp" }, _svcomp );
    _sequential = parsed.getOr( { "sequential" }, _sequential );
    _synchronous = parsed.getOr( { "synchronous" }, _synchronous );
    _disableStaticReduction = parsed.getOr( { "disable static reduction" }, _disableStaticReduction );

    if ( parsed.get< std::string >( { "error found" } ) == "yes" )
    {
        _trace.reset( new sim::Trace );
        sim::OneLineTokenizer tok;
        using Lines = std::vector< std::string >;
        auto choices = parsed.get< Lines >( { "machine trace", "*", "choices" } );
        auto interrupts = parsed.get< Lines >( { "machine trace", "*", "interrupts" } );
        if ( choices.size() != interrupts.size() )
            throw brick::except::Error( "The machine trace is corrupt." );
        for ( unsigned i = 0; i < choices.size(); ++i )
        {
            _trace->steps.emplace_back();
            auto &step = _trace->steps.back();
            for ( auto c : tok.tokenize( choices[ i ] ) )
                step.choices.push_back( sim::parse_choice( c ) );
            for ( auto i : tok.tokenize( interrupts[ i ] ) )
                step.interrupts.push_back( sim::parse_interrupt( i ) );
        }
    }
    else
        std::cerr << "WARNING: There is no counterexample in the report." << std::endl;
}

}
