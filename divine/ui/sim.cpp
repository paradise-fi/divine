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

struct WithVar : Command
{
    std::string var;
    WithVar( std::string v = "$_" ) : var( v ) {}
};

struct WithFrame : WithVar
{
    WithFrame() : WithVar( "$frame" ) {}
};

struct Set : WithVar { std::string value; };
struct Step : WithFrame {};
struct Show : WithVar { bool raw; };
struct BitCode : WithFrame {};

struct BackTrace : WithVar
{
    BackTrace() : WithVar( "$top" ) {}
};

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

    std::map< std::string, DN > _dbg;

    vm::explore::Context _ctx;
    std::set< vm::CodePointer > _breaks;
    char *_prompt;

    void command( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    void set( std::string n, DN dn ) { _dbg.erase( n ); _dbg.emplace( n, dn ); }
    void set( std::string n, vm::GenericPointer p, vm::DNKind k, llvm::Type *t )
    {
        set( n, DN( _ctx, _ctx.heap().snapshot(), p, k, t ) );
    }

    DN nullDN() { return DN( _ctx, _ctx.heap().snapshot(), vm::nullPointer(), vm::DNKind::Object, nullptr ); }

    DN get( std::string n, bool silent = false )
    {
        brick::string::Splitter split( "\\.", REG_EXTENDED );
        auto comp = split.begin( n );

        auto i = _dbg.find( *comp );
        if ( i == _dbg.end() )
            throw brick::except::Error( "variable " + *comp + " is not defined" );

        auto dn = i->second;
        switch ( n[0] )
        {
            case '$': dn.relocate( _ctx.heap().snapshot() ); break;
            case '#': break;
            default: throw brick::except::Error( "variable names must start with $ or #" );
        }

        auto _dn = std::make_unique< DN >( dn );

        for ( ++comp ; comp != split.end(); ++comp )
        {
            bool found = false;
            dn.related( [&]( auto n, auto rel )
                         {
                             if ( *comp == n )
                                found = true, _dn = std::make_unique< DN >( rel );
                         } );
            if ( silent && !found )
                return nullDN();
            if ( !found )
                throw brick::except::Error( "lookup failed at " + *comp );
        }
        return *_dn;
    }

    void set( std::string n, std::string value, bool silent = false )
    {
        set( n, get( value, silent ) );
    }

    void update()
    {
        set( "$top", _ctx.frame().cooked(), vm::DNKind::Frame, nullptr );
        auto dn = get( "$_" );
        if ( dn.kind() == vm::DNKind::Frame )
            set( "$frame", "$_" );
        else
            set( "$data", "$_" );
    }

    void directions( std::string bad )
    {

        if ( !bad.empty() )
            throw brick::except::Error( "unknown direction '" + bad + "'" );
    }

    void show( DN dn, bool detailed = false )
    {
        dn.attributes(
            [&]( auto k, auto v )
            {
                if ( k[0] != '_' || detailed )
                    std::cerr << k << ": " << v << std::endl;
            } );
        std::cerr << "related:" << std::flush;
        int col = 0;
        dn.related( [&]( std::string n, auto )
                    {
                        if ( col + n.size() >= 68 )
                            col = 0, std::cerr << std::endl << "        ";
                        std::cerr << " " << n;
                        col += n.size();
                    } );
        std::cerr << std::endl;
    }

    void info()
    {
        auto top = get( "$top" ), frame = get( "$frame" );
        auto sym = attr( top, "symbol" ), loc = attr( top, "location" );
        std::cerr << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 )
            std::cerr << std::endl << "#        at ";
        else
            std::cerr << " at ";
        std::cerr << loc << std::endl;
        if ( frame._address != top._address )
            std::cerr << "# NOTE: $frame in " << attr( frame, "symbol" ) << std::endl;
    }

    Interpreter( BC bc ) : _exit( false ), _bc( bc ), _ctx( _bc->program() )
    {
        setup( _bc->program(), _ctx );
        _ctx.mask( true );
        _prompt = strdup( "> " );
        set( "$_", nullDN() );
        update();
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

    using Eval = vm::Eval< vm::Program, vm::explore::Context, PointerV >;

    bool schedule( Eval &eval )
    {
        if ( !_ctx.frame().cooked().null() )
            return false; /* nothing to be done */

        auto st = eval._result.cooked();

        if ( st.null() )
            return true;

        // _states.push_back( _ctx.snap( _last ) );
        _ctx.enter( _ctx.sched(), vm::nullPointer(),
                    Eval::IntV( eval.heap().size( st ) ), PointerV( st ) );

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
        eval.dispatch();
        /* $top was not updated yet */
        std::cerr << attr( get( "$top" ), "instruction" ) << std::endl;
        schedule( eval );
        set( "$_", _ctx.frame().cooked(), vm::DNKind::Frame, nullptr );
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

    void go( BackTrace bt )
    {
        set( "$$", bt.var );
        do {
            show( get( "$$" ) );
            set( "$$", "$$.parent", true );
            std::cerr << std::endl;
        } while ( get( "$$" ).valid() );
    }

    void go( Show s )
    {
        auto dn = get( s.var );
        if ( s.raw )
            std::cerr << attr( dn, "_raw" ) << std::endl;
        else
            show( dn );
    }

    void go( Set s ) { set( s.var, s.value ); }
    void go( BitCode bc ) { get( bc.var ).bitcode( std::cerr ); }
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

    auto varopts = cmd::make_option_set< WithVar >( v )
                   .option( "[{string}]", &WithVar::var, "a variable reference"s );
    auto showopts = cmd::make_option_set< Show >( v )
                    .option( "[--raw]", &Show::raw, "dump raw data"s );

    auto parser = cmd::make_parser( v )
                  .command< Exit >( "exit from divine"s )
                  .command< Help >( cmd::make_option( v, "[{string}]", &Help::_cmd ) )
                  .command< Step >( "execute one instruction"s )
                  .command< Run >( "execute the program until interrupted"s )
                  .command< Set >( "set a variable "s, varopts )
                  .command< BitCode >( "show the bitcode of the current function"s, varopts )
                  .command< Show >( "show an object"s, varopts, showopts )
                  .command< BackTrace >( "show a stack trace"s, varopts );

    try {
        auto cmd = parser.parse( tok.begin(), tok.end() );
        cmd.match( [&] ( Help h ) { std::cerr << parser.describe( h._cmd ) << std::endl; },
                   [&] ( auto opt ) { go( opt ); } );
        update();
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
