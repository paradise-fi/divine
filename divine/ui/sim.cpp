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

namespace divine {
namespace ui {

using namespace std::literals;
namespace cmd = brick::cmd;

namespace sim {

struct Command {};
struct WithDirection { std::string _where; };
struct Step : Command {};
struct Walk : WithDirection {};
struct Look : WithDirection { bool _detailed; };
struct Jump : WithDirection {};
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
        {
            std::cerr << "you are standing nowhere and there is nowhere to go" << std::endl;
            return;
        }

        _dbg.back().attributes(
            [&]( std::string k, std::string v )
            {
                if ( k == "address" )
                    std::cerr << "you are standing at " << _dbg.back().kind() << " " << v << std::endl;
            } );
    }

    Interpreter( BC bc ) : _exit( false ), _bc( bc ), _ctx( _bc->program() )
    {
        setup( _bc->program(), _ctx, _env );
        _ctx.mask( true );
        _prompt = strdup( "> " );
    }

    void frame( vm::GenericPointer fr )
    {
        _dbg.clear();
        _dbg.emplace_back( _ctx, fr, vm::DNKind::Frame, nullptr );
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

        vm::explore::State st;
        st.heap = std::make_shared< vm::MutableHeap >();
        st.root = vm::clone( _ctx.heap(), *st.heap, _last );
        _states.push_back( st );

        _ctx.enter( _ctx._sched, vm::nullPointer(),
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
        for ( int i = 1; i < eval.instruction().values.size(); ++i )
            eval.op< Eval::Any >( i, [&]( auto v )
                                  { std::cerr << " op[" << i << "] = " << v.get( i ) << std::endl; } );
        eval.instruction().op->dump();
        eval.dispatch();
        /*
        eval.op< Eval::Any >( 0, [&]( auto v )
                              { std::cerr << " result = " << v.get( 0 ) << std::endl; } );
        */
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
        _dbg.back().related( [&]( auto n, auto dn )
                             {
                                 if ( w._where == n )
                                     _dbg.push_back( dn ), found = true;
                             } );
        if ( found )
            return;
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
        look( dn, l._detailed );
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
                    .option( "[--details]", &Look::_detailed, "show minutae"s );

    auto parser = cmd::make_parser( v )
                  .command< Exit >( "exit from divine"s )
                  .command< Help >( cmd::make_option( v, "[{string}]", &Help::_cmd ) )
                  .command< Step >( "execute one instruction"s )
                  .command< Run >( "execute the program until interrupted"s )
                  .command< Walk >( "move to a different memory location"s, diropts )
                  .command< Jump >( "jump to a different memory location"s, diropts )
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

    el_set( el, EL_HIST, history, hist );
    el_set( el, EL_PROMPT, sim::prompt );
    el_set( el, EL_CLIENTDATA, &interp );
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

    history_end( hist );
    el_end( el );
}

}
}
