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

#include <divine/vm/explore.hpp>
#include <divine/vm/debug.hpp>
#include <divine/ui/cli.hpp>
#include <brick-string>
#include <cstring>

#include <histedit.h>
#include <pwd.h>

namespace divine {
namespace ui {

using namespace std::literals;
namespace cmd = brick::cmd;

namespace sim {

struct Command {};
struct WithDirection { std::string _where; };
struct Step : Command {};
struct Walk : WithDirection {};
struct Look : WithDirection { bool _raw; };
struct Jump : WithDirection {};
struct BitCode : Command {};
struct Trace : Command {};
struct Exit : Command {};
struct Help : Command { std::string _cmd; };

struct Interpreter
{
    using BC = std::shared_ptr< vm::BitCode >;
    using DN = vm::DebugNode< vm::explore::Context >;
    using PointerV = vm::explore::Context::PointerV;

    bool _exit;
    BC _bc;

    std::vector< std::string > _env;
    std::vector< vm::explore::State > _states;
    vm::MutableHeap::Pointer _last;

    vm::explore::Context _ctx;
    std::vector< DN > _dbg;
    char *_prompt;
    std::set< vm::CodePointer > _breaks;

    void command( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    void info()
    {
        if ( _dbg.empty() )
            std::cerr << "# you are standing nowhere and there is nowhere to go" << std::endl;
        else
            std::cerr << "# you are standing at " << _dbg.back().kind() << " "
                    << attr( _dbg.back(), "address" ) << std::endl;
        DN frame( _ctx, _ctx.frame().cooked(), vm::DNKind::Frame, nullptr );
        auto sym = attr( frame, "symbol" ), loc = attr( frame, "location" );
        std::cerr << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 )
            std::cerr << std::endl << "#        at ";
        else
            std::cerr << " at ";
        std::cerr << loc << std::endl;
    }

    Interpreter( BC bc ) : _exit( false ), _bc( bc ), _ctx( _bc->program() )
    {
        setup( _bc->program(), _ctx );
        _ctx.mask( true );
        _prompt = strdup( "> " );
    }

    std::string attr( DN dn, std::string key )
    {
        std::string res = "-";
        dn.attributes( [&]( auto k, auto v )
            {
                if ( k == key )
                    res = v;
            } );
        return res;
    }

    void look( DN dn, bool detailed = false )
    {
        dn.attributes(
            [&]( auto k, auto v )
            {
                if ( k[0] != '_' || detailed )
                    std::cerr << k << ": " << v << std::endl;
            } );
    }

    using Eval = vm::Eval< vm::Program, vm::explore::Context, PointerV >;

    bool schedule( Eval &eval )
    {
        if ( _ctx.frame().cooked() != vm::nullPointer() )
            return false; /* nothing to be done */

        _last = eval._result.cooked();

        if ( _last.null() )
            return true;

        _states.push_back( _ctx.snap( _last ) );
        _ctx.enter( _ctx.sched(), vm::nullPointer(),
                    Eval::IntV( eval.heap().size( _last ) ), PointerV( _last ) );

        return true;
    }

    void check_running()
    {
        if ( _ctx.frame().cooked() == vm::nullPointer() )
            throw brick::except::Error( "the program has already terminated" );
    }

    void go( Exit ) { _exit = true; }

    void go( Step )
    {
        check_running();
        Eval eval( _bc->program(), _ctx );
        eval.advance();
        DN frame( _ctx, _ctx.frame().cooked(), vm::DNKind::Frame, nullptr );
        eval.dispatch();
        std::cerr << attr( frame, "instruction" ) << std::endl;
        schedule( eval );
    }

    void go( Run )
    {
        check_running();
        Eval eval( _bc->program(), _ctx );
        while ( true )
        {
            eval.advance();
            eval.dispatch();
            if ( schedule( eval ) )
                break;
        }
    }

    void go( Trace )
    {
        auto fr = _ctx.frame();
        while ( fr.cooked() != vm::nullPointer() )
        {
            look( DN( _ctx, fr.cooked(), vm::DNKind::Frame, nullptr ) );
            std::cerr << std::endl;
            _ctx.heap().skip( fr, vm::PointerBytes );
            _ctx.heap().read( fr.cooked(), fr );
        }
    }

    void go( Jump j )
    {
        _dbg.clear();
        if ( j._where == "state" )
            _dbg.emplace_back( _ctx, _last, vm::DNKind::Object, nullptr );
        else if ( j._where == "frame" )
            _dbg.emplace_back( _ctx, _ctx.frame().cooked(), vm::DNKind::Frame, nullptr );
        else
            throw brick::except::Error( "unknown jump destination '" + j._where + "'" );
    }

    void directions( std::string bad )
    {
        std::cerr << "available directions:" << std::endl;
        _dbg.back().related( [&]( auto n, auto ) { std::cerr << "  " << n << std::endl; } );

        if ( !bad.empty() )
            throw brick::except::Error( "unknown direction '" + bad + "'" );
    }

    void go( Walk w )
    {
        if ( _dbg.empty() )
            throw brick::except::Error( "need a starting point" );
        bool found = false;
        DN found_dn;
        _dbg.back().related( [&]( auto n, auto dn )
                             {
                                 if ( w._where == n )
                                    found_dn = dn, found = true;
                             } );
        if ( found )
        {
            _dbg.push_back( found_dn );
            return;
        }
        directions( w._where );
    }

    void go( Look l )
    {
        if ( _dbg.empty() )
            throw brick::except::Error( "need to be somewhere first" );
        DN dn;
        if ( l._where.empty() )
            dn = _dbg.back();
        else _dbg.back().related( [&] ( auto n, auto _dn )
                                  {
                                      if ( n == l._where )
                                          dn = _dn;
                                  } );
        if ( dn._address == vm::nullPointer() )
            return directions( l._where );
        if ( l._raw )
            std::cerr << attr( dn, "_raw" ) << std::endl;
        else
            look( dn, false );
    }

    void go( BitCode )
    {
        if ( _dbg.empty() || _dbg.back().kind() != vm::DNKind::Frame )
            throw brick::except::Error( "this command only works when you stand in a frame" );
        auto fr = _ctx.frame();
        _ctx.frame( _dbg.back()._address );
        _dbg.back().bitcode( std::cerr );
        _ctx.frame( fr );
    }

    void go( Help ) { UNREACHABLE( "impossible case" ); }
};

char *prompt( EditLine *el )
{
    Interpreter *interp;
    el_get( el, EL_CLIENTDATA, &interp );
    return interp->prompt();
}

void Interpreter::command( cmd::Tokens tok )
{
    auto v = cmd::make_validator();

    auto diropts = cmd::make_option_set< WithDirection >( v )
                   .option( "[{string}]", &WithDirection::_where, "direction"s );
    auto lookopts = cmd::make_option_set< Look >( v )
                    .option( "[--raw]", &Look::_raw, "dump raw data"s );

    auto parser = cmd::make_parser( v )
                  .command< Exit >( "exit from divine"s )
                  .command< Help >( cmd::make_option( v, "[{string}]", &Help::_cmd ) )
                  .command< Step >( "execute one instruction"s )
                  .command< Run >( "execute the program until interrupted"s )
                  .command< Walk >( "move to a different memory location"s, diropts )
                  .command< Jump >( "jump to a different memory location"s, diropts )
                  .command< BitCode >( "show the bitcode of the current function"s )
                  .command< Look >( "look at a memory location without moving"s, diropts, lookopts )
                  .command< Trace >( "show the stack trace of current thread"s );

    try {
        auto cmd = parser.parse( tok.begin(), tok.end() );
        cmd.match( [&] ( Help h ) { std::cerr << parser.describe( h._cmd ) << std::endl; },
                   [&] ( auto opt ) { go( opt ); } );
    }
    catch ( brick::except::Error &e )
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return;
    }
}

}

void Sim::run()
{
    auto el = el_init( "divine", stdin, stdout, stderr );
    auto hist = history_init();
    HistEvent hist_ev;
    sim::Interpreter interp( _bc );;

    std::string hist_path;

    if ( passwd *p = getpwuid( getuid() ) )
    {
        hist_path = p->pw_dir;
        hist_path += "/.divine.history";
    }

    history( hist, &hist_ev, H_SETSIZE, 1000 );
    history( hist, &hist_ev, H_LOAD, hist_path.c_str() );
    el_set( el, EL_HIST, history, hist );
    el_set( el, EL_PROMPT, sim::prompt );
    el_set( el, EL_CLIENTDATA, &interp );
    el_set( el, EL_EDITOR, "emacs" );
    el_source( el, nullptr );

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

        /* TODO use tok_* for quoting support */
        brick::string::Splitter split( "[ \t\n]+", REG_EXTENDED );
        cmd::Tokens tok;
        std::copy( split.begin( cmd ), split.end(), std::back_inserter( tok ) );
        interp.command( tok );
    }

    history( hist, &hist_ev, H_SAVE, hist_path.c_str() );
    history_end( hist );
    el_end( el );
}

}
}
