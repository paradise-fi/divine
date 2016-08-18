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
struct WithSteps : WithFrame
{
    bool over, quiet; int count;
    WithSteps() : over( false ), quiet( false ), count( 1 ) {}
};

struct StepI : WithSteps {};
struct Step : WithSteps {};

struct Show : WithVar { bool raw; };
struct BitCode : WithFrame {};
struct Source : WithFrame {};

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

    struct BreakPoint
    {
        vm::CodePointer pc;
        vm::GenericPointer frame;
        BreakPoint( vm::CodePointer pc, vm::HeapPointer frame ) : pc( pc ), frame( frame ) {}
        bool operator<( BreakPoint o ) const
        {
            if ( pc == o.pc )
                return frame < o.frame;
            return pc < o.pc;
        }
    };

    struct Counter
    {
        vm::GenericPointer frame;
        int lines, instructions;
        Counter( vm::GenericPointer fr, int lines, int instructions )
            : frame( fr ), lines( lines ), instructions( instructions )
        {}
    };

    bool _exit;
    BC _bc;

    std::vector< std::string > _env;

    std::map< std::string, DN > _dbg;

    vm::explore::Context _ctx;
    std::set< vm::CodePointer > _breaks;
    char *_prompt;

    void command( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    DN dn( vm::GenericPointer p, vm::DNKind k, llvm::Type *t )
    {
        return DN( _ctx, _ctx.heap().snapshot(), p, k, t );
    }
    DN nullDN() { return dn( vm::nullPointer(), vm::DNKind::Object, nullptr ); }

    void set( std::string n, DN dn ) { _dbg.erase( n ); _dbg.emplace( n, dn ); }
    void set( std::string n, vm::GenericPointer p, vm::DNKind k, llvm::Type *t )
    {
        set( n, dn( p, k, t ) );
    }

    DN get( std::string n, bool silent = false )
    {
        brick::string::Splitter split( "\\.", REG_EXTENDED );
        auto comp = split.begin( n );

        auto i = _dbg.find( *comp );
        if ( i == _dbg.end() && silent )
            return nullDN();
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
        if ( get( "$_" ).kind() == vm::DNKind::Frame )
            set( "$frame", "$_" );
        else
            set( "$data", "$_" );
        if ( !get( "$frame", true ).valid() )
            set( "$frame", "$top" );
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

    void run( std::set< BreakPoint > bps, Counter ctr, bool verbose )
    {
        check_running();
        Eval eval( _bc->program(), _ctx );
        int lines = 0, instructions = 0, line = 0;
        while ( true )
        {
            for ( auto bp : bps )
            {
                if ( !bp.frame.null() && bp.frame != _ctx.frame().cooked() )
                    continue;
                if ( eval.pc() == bp.pc )
                    goto end;
            }

            if ( !ctr.frame.null() && !_ctx.heap().valid( ctr.frame ) )
                goto end; /* the frame went out of scope, halt */

            if ( ctr.frame.null() || ctr.frame == _ctx.frame().cooked() )
            {
                if ( ctr.instructions && ctr.instructions == instructions )
                    goto end;
                if ( ctr.lines && ctr.lines == lines )
                    goto end;

                ++ instructions;
                /* TODO line counter */
            }

            eval.advance();
            if ( verbose )
            {
                auto frame = _ctx.frame().cooked();
                auto &insn = eval.instruction();
                std::string before = vm::instruction( insn, eval );
                eval.dispatch();
                if ( _ctx.heap().valid( frame ) )
                {
                    auto newframe = _ctx.frame();
                    _ctx.frame( frame ); /* :-( */
                    std::cerr << vm::instruction( insn, eval ) << std::endl;
                    _ctx.frame( newframe );
                }
                else
                    std::cerr << before << std::endl;
            }
            else
                eval.dispatch();

            schedule( eval );
            for ( auto t : _ctx._trace )
                std::cerr << "T: " << t << std::endl;
            _ctx._trace.clear();
            if ( _ctx.frame().cooked().null() )
                goto end;
        }
    end:
        ;
    }

    void go( Exit ) { _exit = true; }
    void go( Step s ) { NOT_IMPLEMENTED(); }

    void go( StepI s )
    {
        check_running();
        auto frame = get( s.var );
        run( {}, Counter( s.over ? frame.address() : vm::nullPointer(), 0, s.count ), !s.quiet );
        set( "$_", _ctx.frame().cooked(), vm::DNKind::Frame, nullptr ); /* hmm */
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
    void go( Source src ) { get( src.var ).source( std::cerr ); }
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
    auto stepopts = cmd::make_option_set< WithSteps >( v )
                    .option( "[--over]", &WithSteps::over,
                            "execute calls as one step"s )
                    .option( "[--quiet]", &WithSteps::quiet,
                            "do not print what is being executed"s )
                    .option( "[--count {int}]", &WithSteps::count,
                             "execute {int} steps (default = 1)"s );

    auto parser = cmd::make_parser( v )
                  .command< Exit >( "exit from divine"s )
                  .command< Help >( cmd::make_option( v, "[{string}]", &Help::_cmd ) )
                  .command< StepI >( "execute source line"s, varopts, stepopts )
                  .command< Step >( "execute one instruction"s, varopts, stepopts )
                  .command< Run >( "execute the program until interrupted"s )
                  .command< Set >( "set a variable "s, varopts )
                  .command< BitCode >( "show the bitcode of the current function"s, varopts )
                  .command< Source >( "show the source code of the current function"s, varopts )
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
