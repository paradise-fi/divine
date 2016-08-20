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

namespace command {

struct WithVar
{
    std::string var;
    WithVar( std::string v = "$_" ) : var( v ) {}
};

struct WithFrame : WithVar
{
    WithFrame() : WithVar( "$frame" ) {}
};

struct Set { std::vector< std::string > options; };
struct WithSteps : WithFrame
{
    bool over, quiet; int count;
    WithSteps() : over( false ), quiet( false ), count( 1 ) {}
};

struct StepI : WithSteps {};
struct StepS : WithSteps {};
struct Step : WithSteps {};
struct Run { bool verbose; };

struct Show : WithVar { bool raw; };
struct Inspect : Show {};
struct BitCode : WithFrame {};
struct Source : WithFrame {};
struct Thread  { std::string spec; bool random; };

struct BackTrace : WithVar
{
    BackTrace() : WithVar( "$top" ) {}
};

struct Exit {};
struct Help { std::string _cmd; };

}

using ProcInfo = std::vector< std::pair< std::pair< int, int >, int > >;

struct Context : vm::Context< vm::CowHeap >
{
    using Program = vm::Program;
    std::vector< std::string > _trace;
    ProcInfo _proc;
    int _choice;

    Context( Program &p ) : vm::Context< vm::CowHeap >( p ), _choice( 0 ) {}

    template< typename I >
    int choose( int count, I, I )
    {
        ASSERT_LT( _choice, count );
        ASSERT_LEQ( 0, _choice );
        if ( !_proc.empty() )
            _proc.clear();
        return _choice;
    }

    void doublefault()
    {
        _trace.push_back( "fatal double fault" );
        _t.frame = vm::nullPointer();
    }

    void trace( vm::TraceText tt )
    {
        _trace.push_back( heap().read_string( tt.text ) );
    }

    void trace( vm::TraceSchedInfo ) { NOT_IMPLEMENTED(); }
    void trace( vm::TraceFlag ) { NOT_IMPLEMENTED(); }

    void trace( vm::TraceSchedChoice tsc )
    {
        std::cerr << "tsc = " << tsc.list << std::endl;
        auto ptr = tsc.list;
        int size = heap().size( ptr.cooked() );
        if ( size % 12 )
            return; /* invalid */
        for ( int i = 0; i < size / 12; ++i )
        {
            vm::value::Int< 32, true > pid, tid, choice;
            heap().read_shift( ptr, pid );
            heap().read_shift( ptr, tid );
            heap().read_shift( ptr, choice );
            _proc.emplace_back( std::make_pair( pid.cooked(), tid.cooked() ), choice.cooked() );
        }
    }
};

struct Interpreter
{
    using BC = std::shared_ptr< vm::BitCode >;
    using DN = vm::DebugNode< Context >;
    using PointerV = Context::PointerV;

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
    std::pair< int, int > _sticky_tid;
    std::mt19937 _rand;
    bool _sched_random;

    Context _ctx;
    std::set< vm::CodePointer > _breaks;
    char *_prompt;
    int _state_count;

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

        auto var = ( n[0] == '$' || n[0] == '#' ) ? *comp++ : "$_";
        auto i = _dbg.find( var );
        if ( i == _dbg.end() && silent )
            return nullDN();
        if ( i == _dbg.end() )
            throw brick::except::Error( "variable " + var + " is not defined" );

        auto dn = i->second;
        switch ( var[0] )
        {
            case '$': dn.relocate( _ctx.heap().snapshot() ); break;
            case '#': break;
            default: UNREACHABLE( "impossible case" );
        }

        auto _dn = std::make_unique< DN >( dn );

        for ( ; comp != split.end(); ++comp )
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

    Interpreter( BC bc )
        : _exit( false ), _bc( bc ), _ctx( _bc->program() ), _state_count( 0 ),
          _sticky_tid( -1, 0 ), _sched_random( false )
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

    using Eval = vm::Eval< vm::Program, Context, PointerV >;

    bool schedule( Eval &eval )
    {
        if ( !_ctx.frame().cooked().null() )
            return false; /* nothing to be done */

        auto st = eval._result.cooked();

        if ( st.null() )
            return true;

        /* TODO do not allocate a new #NNN for already-visited states */
        auto name = "#"s + brick::string::fmt( ++_state_count );
        set( name, st, vm::DNKind::Object, nullptr );
        std::cerr << "# a new program state was stored as " << name << std::endl;

        // _states.push_back( _ctx.snap( _last ) );
        _ctx.enter( _ctx.sched(), vm::nullPointer(),
                    Eval::IntV( eval.heap().size( st ) ), PointerV( st ) );

        return true;
    }

    int sched_policy( const ProcInfo &proc )
    {
        std::uniform_int_distribution< int > dist( 0, proc.size() - 1 );
        if ( _sched_random )
            return dist( _rand );
        for ( auto pi : proc )
            if ( pi.first == _sticky_tid )
                return pi.second;
        /* thread is gone, pick a replacement at random */
        int seq = dist( _rand );
        _sticky_tid = proc[ seq ].first;
        return proc[ seq ].second;
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
        bool in_fault = eval.pc().function() == _ctx.fault_handler().function();
        bool jumped = false;
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

            if ( eval.instruction().hypercall == vm::HypercallJump )
                jumped = true;

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

            if ( !_ctx._proc.empty() )
            {
                _ctx._choice = sched_policy( _ctx._proc );
                std::cerr << "# active threads:";
                for ( auto pi : _ctx._proc )
                {
                    bool active = pi.second == _ctx._choice;
                    std::cerr << ( active ? " [" : " " )
                              << pi.first.first << ":" << pi.first.second
                              << ( active ? "]" : "" );
                }
                std::cerr << std::endl;
            }

            for ( auto t : _ctx._trace )
                std::cerr << "T: " << t << std::endl;
            _ctx._trace.clear();

            if ( _ctx.frame().cooked().null() )
                goto end;
            if ( jumped )
                goto end;
            if ( !in_fault && eval.pc().function() == _ctx.fault_handler().function() )
                goto end;
        }
    end:
        ;
    }

    void go( command::Exit ) { _exit = true; }
    void go( command::Step s ) { NOT_IMPLEMENTED(); }

    void go( command::StepI s )
    {
        check_running();
        auto frame = get( s.var );
        run( {}, Counter( s.over ? frame.address() : vm::nullPointer(), 0, s.count ), !s.quiet );
        set( "$_", _ctx.frame().cooked(), vm::DNKind::Frame, nullptr ); /* hmm */
    }

    void go( command::Run )
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

    void go( command::BackTrace bt )
    {
        set( "$$", bt.var );
        do {
            show( get( "$$" ) );
            set( "$$", "$$.parent", true );
            std::cerr << std::endl;
        } while ( get( "$$" ).valid() );
    }

    void go( command::Show s )
    {
        auto dn = get( s.var );
        if ( s.raw )
            std::cerr << attr( dn, "_raw" ) << std::endl;
        else
            show( dn );
    }

    void go( command::Inspect i )
    {
        command::Show s;
        s.var = i.var;
        s.raw = i.raw;
        go( s );
        set( "$_", s.var );
    }

    void go( command::Set s )
    {
        if ( s.options.size() != 2 )
            throw brick::except::Error( "2 options are required for set, the variable and the value" );
        set( s.options[0], s.options[1] );
    }

    void go( command::Thread thr )
    {
        _sched_random = thr.random;
        if ( !thr.spec.empty() )
        {
            std::istringstream istr( thr.spec );
            char c;
            istr >> _sticky_tid.first >> c >> _sticky_tid.second;
            if ( c != ':' )
                throw brick::except::Error( "expected thread specifier format: <pid>:<tid>" );
            std::cerr << "tid = " << _sticky_tid.first << ":" << _sticky_tid.second << std::endl;
        }
    }

    void go( command::BitCode bc ) { get( bc.var ).bitcode( std::cerr ); }
    void go( command::Source src ) { get( src.var ).source( std::cerr ); }
    void go( command::Help ) { UNREACHABLE( "impossible case" ); }
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

    auto varopts = cmd::make_option_set< command::WithVar >( v )
        .option( "[{string}]", &command::WithVar::var, "a variable reference"s );
    auto showopts = cmd::make_option_set< command::Show >( v )
        .option( "[--raw]", &command::Show::raw, "dump raw data"s );
    auto stepopts = cmd::make_option_set< command::WithSteps >( v )
        .option( "[--over]", &command::WithSteps::over, "execute calls as one step"s )
        .option( "[--quiet]", &command::WithSteps::quiet, "do not print what is being executed"s )
        .option( "[--count {int}]", &command::WithSteps::count, "execute {int} steps (default = 1)"s );
    auto threadopts = cmd::make_option_set< command::Thread >( v )
        .option( "[--random]", &command::Thread::random, "pick the thread to run randomly"s )
        .option( "[{string}]", &command::Thread::spec, "stick to the given thread"s );

    auto parser = cmd::make_parser( v )
        .command< command::Exit >( "exit from divine"s )
        .command< command::Help >( cmd::make_option( v, "[{string}]", &command::Help::_cmd ) )
        .command< command::StepI >( "execute source line"s, varopts, stepopts )
        .command< command::Step >( "execute one instruction"s, varopts, stepopts )
        .command< command::Run >( "execute the program until interrupted"s )
        .command< command::Set >( "set a variable "s, &command::Set::options )
        .command< command::BitCode >( "show the bitcode of the current function"s, varopts )
        .command< command::Source >( "show the source code of the current function"s, varopts )
        .command< command::Thread >( "control thread scheduling"s, threadopts )
        .command< command::Show >( "show an object"s, varopts, showopts )
        .command< command::Inspect >( "like show, but also set $_"s, varopts, showopts )
        .command< command::BackTrace >( "show a stack trace"s, varopts );

    try {
        auto cmd = parser.parse( tok.begin(), tok.end() );
        cmd.match( [&] ( command::Help h ) { std::cerr << parser.describe( h._cmd ) << std::endl; },
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
