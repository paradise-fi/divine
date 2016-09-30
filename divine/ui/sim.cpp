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

#include <divine/vm/stepper.hpp>
#include <divine/vm/explore.hpp>
#include <divine/vm/debug.hpp>
#include <divine/vm/print.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/logo.hpp>
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
    bool over, out, quiet, verbose; int count;
    WithSteps() : over( false ), out( false ), quiet( false ), verbose( false ), count( 1 ) {}
};

struct Start {};

struct Break
{
    std::vector< std::string > where;
    bool remove, list;
    Break() : remove( false ), list( false ) {}
};

struct StepI : WithSteps {};
struct StepA : WithSteps {};
struct Step : WithSteps {};
struct Rewind : WithVar
{
    Rewind() : WithVar( "#last" ) {}
};

struct Show : WithVar
{
    bool raw;
    Show() : raw( false ) {}
};

struct Inspect : Show {};
struct BitCode : WithFrame {};
struct Source : WithFrame {};
struct Thread  { std::string spec; bool random; };
struct Trace
{
    std::string from;
    std::vector< std::string > choices;
};

struct BackTrace : WithVar
{
    BackTrace() : WithVar( "$top" ) {}
};

struct Setup
{
    bool debug_kernel;
    Setup() : debug_kernel( false ) {}
};

struct Exit {};
struct Help { std::string _cmd; };

}

using Context = vm::DebugContext< vm::Program, vm::CowHeap >;

struct Interpreter
{
    using BC = std::shared_ptr< vm::BitCode >;
    using DN = vm::DebugNode< vm::Program, vm::CowHeap >;
    using PointerV = Context::PointerV;

    bool _exit;
    BC _bc;

    std::vector< std::string > _env;

    std::map< std::string, DN > _dbg;
    std::map< vm::CowHeap::Snapshot, std::string > _state_names;
    std::map< vm::CowHeap::Snapshot, std::deque< int > > _trace;
    vm::Explore _explore;

    std::pair< int, int > _sticky_tid;
    std::mt19937 _rand;
    bool _sched_random, _debug_kernel;

    Context _ctx;
    std::set< vm::CodePointer > _bps;
    char *_prompt;
    int _state_count;

    void command( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    DN dn( vm::GenericPointer p, vm::DNKind k, llvm::Type *t, llvm::DIType *dit )
    {
        DN r( _ctx, _ctx.snapshot() );
        r.address( k, p );
        r.type( t );
        r.di_type( dit );
        return r;
    }

    DN nullDN() { return dn( vm::nullPointer(), vm::DNKind::Object, nullptr, nullptr ); }
    DN frameDN() { return dn( _ctx.frame(), vm::DNKind::Frame, nullptr, nullptr ); }

    DN objDN( vm::GenericPointer p, llvm::Type *t, llvm::DIType *dit )
    {
        return dn( p, vm::DNKind::Object, t, dit );
    }

    void set( std::string n, DN dn ) { _dbg.erase( n ); _dbg.emplace( n, dn ); }

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
            case '$': dn.relocate( _ctx.snapshot() ); break;
            case '#': break;
            default: UNREACHABLE( "impossible case" );
        }

        std::unique_ptr< DN > _dn = std::make_unique< DN >( dn ), _dn_next;

        for ( ; comp != split.end(); ++comp )
        {
            bool found = false;
            _dn->related( [&]( auto n, auto rel )
                          {
                              if ( *comp == n )
                                  found = true, _dn_next = std::make_unique< DN >( rel );
                          } );
            if ( silent && !found )
                return nullDN();
            _dn = std::move( _dn_next );
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
        set( "$top", frameDN() );

        auto globals = _ctx.get( _VM_CR_Globals ).pointer;
        if ( !get( "$globals", true ).valid() || get( "$globals" ).address() != globals )
        {
            DN gdn( _ctx, _ctx.snapshot() );
            gdn.address( vm::DNKind::Globals, globals );
            set( "$globals", gdn );
        }

        auto state = _ctx.get( _VM_CR_State ).pointer;
        if ( !get( "$state", true ).valid() || get( "$state" ).address() != state )
        {
            DN sdn( _ctx, _ctx.snapshot() );
            sdn.address( vm::DNKind::Object, state );
            sdn.type( _ctx._state_type );
            sdn.di_type( _ctx._state_di_type );
            set( "$state", sdn );
        }

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

    void info()
    {
        auto top = get( "$top" ), frame = get( "$frame" );
        auto sym = top.attribute( "@symbol" ), loc = top.attribute( "@location" );
        std::cerr << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 )
            std::cerr << std::endl << "#        at ";
        else
            std::cerr << " at ";
        std::cerr << loc << std::endl;
        if ( frame._address != top._address )
            std::cerr << "# NOTE: $frame in " << frame.attribute( "@symbol" ) << std::endl;
    }

    Interpreter( BC bc )
        : _exit( false ), _bc( bc ), _explore( bc ), _ctx( _bc->program() ),
          _sticky_tid( -1, 0 ), _state_count( 0 ), _sched_random( false ), _debug_kernel( false )
    {
        vm::setup::boot( _ctx );
        _prompt = strdup( "> " );
        set( "$_", nullDN() );
        update();
    }

    auto newstate( vm::CowHeap::Snapshot snap, bool update_choices = true, bool terse = false )
    {
        snap = _explore.start( _ctx, snap );

        bool isnew = false;
        std::string name;

        if ( _state_names.count( snap ) )
        {
            name = _state_names[ snap ];
            if ( update_choices && _trace.count( snap ) )
                _ctx._choices = _trace[ snap ];
        }
        else
        {
            isnew = true;
            name = _state_names[ snap ] = "#"s + brick::string::fmt( ++_state_count );
            DN state( _ctx, snap );
            state.address( vm::DNKind::Object, _ctx.get( _VM_CR_State ).pointer );
            state.type( _ctx._state_type );
            state.di_type( _ctx._state_di_type );
            set( name, state );
        }

        set( "#last", name );

        if ( terse )
            std::cerr << " " << name << std::flush;
        else if ( isnew )
            std::cerr << "# a new program state was stored as " << name << std::endl;
        else
            std::cerr << "# program entered state " << name << " (already seen)" << std::endl;

        return snap;
    }

    int sched_policy( const vm::ProcInfo &proc )
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
        if ( _ctx.frame().null() )
            throw brick::except::Error( "the program has already terminated" );
    }

    void run( vm::Stepper step, bool verbose )
    {
        check_running();
        step.run( _ctx,
                  [&]( auto snap ) { return newstate( snap ); },
                  [&]() { return sched_policy( _ctx._proc ); },
                  verbose );
    }

    void go( command::Exit ) { _exit = true; }

    vm::Stepper stepper( command::WithSteps s, bool jmp )
    {
        vm::Stepper step;
        step._ff_kernel = !_debug_kernel;
        step._bps = _bps;
        check_running();
        if ( jmp )
            step.jumps( 1 );
        if ( s.over || s.out )
            step.frame( get( s.var ).address() );
        return step;
    }

    void go( command::Start )
    {
        vm::setup::boot( _ctx );
        vm::Stepper step;
        step._bps.insert( _bc->program().functionByName( "main" ) );
        run( step, false );
        set( "$_", frameDN() );
    }

    void go( command::Break b )
    {
        if ( b.list )
        {
            for ( auto pc : _bps )
                std::cerr << _bc->program().llvmfunction( pc )->getName().str() << std::endl;
            return;
        }

        for ( auto w : b.where )
        {
            auto pc = _bc->program().functionByName( w );
            if ( pc.null() )
                throw brick::except::Error( "Could not find " + w );
            if ( b.remove )
                _bps.erase( pc );
            else
                _bps.insert( pc );
        }
    }

    void go( command::Step s )
    {
        auto step = stepper( s, true );
        if ( !s.out )
            step.lines( s.count );
        run( step, !s.quiet );
        set( "$_", frameDN() );
    }

    void go( command::StepI s )
    {
        auto step = stepper( s, true );
        step.instructions( s.count );
        run( step, !s.quiet );
        set( "$_", frameDN() );
    }

    void go( command::StepA s )
    {
        auto step = stepper( s, false );
        step.states( s.count );
        run( step, s.verbose );
        set( "$_", frameDN() );
    }

    void go( command::Rewind re )
    {
        vm::Stepper step;
        step._instructions = std::make_pair( 1, 1 );
        auto tgt = get( re.var );
        _ctx.heap().restore( tgt.snapshot() );
        vm::setup::scheduler( _ctx );
        if ( _trace.count( tgt.snapshot() ) )
            _ctx._choices = _trace[ tgt.snapshot() ];
        run( step, false ); /* make 0 (user mode) steps */
        set( "$_", re.var );
    }

    void go( command::BackTrace bt )
    {
        set( "$$", bt.var );
        do {
            get( "$$" ).format( std::cerr, 0, false );
            set( "$$", "$$.@parent", true );
            std::cerr << std::endl;
        } while ( get( "$$" ).valid() );
    }

    void go( command::Show s )
    {
        auto dn = get( s.var );
        if ( s.raw )
            std::cerr << dn.attribute( "@raw" ) << std::endl;
        else
            dn.format( std::cerr );
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

    void go( command::Trace tr )
    {
        if ( tr.from.empty() )
            vm::setup::boot( _ctx );
        else
        {
            _ctx.heap().restore( get( tr.from ).snapshot() );
            vm::setup::scheduler( _ctx );
        }

        _trace.clear();
        std::deque< int > choices;
        for ( auto c : tr.choices )
            choices.push_back( std::stoi( c ) );
        _ctx._choices = choices;

        vm::Eval< vm::Program, Context, vm::value::Void > eval( _bc->program(), _ctx );
        auto last = get( "#last", true ).snapshot();
        std::cerr << "traced states:";
        while ( !_ctx._choices.empty() )
        {
            eval.advance();
            eval.dispatch();
            if ( _ctx.frame().null() )
            {
                newstate( _ctx.snapshot(), false, true );
                vm::setup::scheduler( _ctx );
                int count = choices.size() - _ctx._choices.size();
                auto b = choices.begin(), e = b + count;
                _trace[ last ] = std::deque< int >( b, e );
                choices.erase( b, e );
                last = get( "#last", true ).snapshot();
            }
        }

        std::cerr << std::endl;
        _ctx._trace.clear();
        vm::Stepper step;
        step._instructions = std::make_pair( 1, 1 );
        run( step, false ); /* make 0 (user mode) steps */
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
        }
    }

    void go( command::BitCode bc ) { get( bc.var ).bitcode( std::cerr ); }
    void go( command::Source src ) { get( src.var ).source( std::cerr ); }
    void go( command::Setup set )
    {
        _debug_kernel = set.debug_kernel;
    }

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
    auto breakopts = cmd::make_option_set< command::Break >( v )
        .option( "[--delete]", &command::Break::remove, "delete the designated breakpoint(s)"s )
        .option( "[--list]", &command::Break::list, "list breakpoints"s );
    auto showopts = cmd::make_option_set< command::Show >( v )
        .option( "[--raw]", &command::Show::raw, "dump raw data"s );
    auto stepopts = cmd::make_option_set< command::WithSteps >( v )
        .option( "[--over]", &command::WithSteps::over, "execute calls as one step"s )
        .option( "[--quiet]", &command::WithSteps::quiet, "suppress output"s )
        .option( "[--verbose]", &command::WithSteps::verbose, "increase verbosity"s )
        .option( "[--count {int}]", &command::WithSteps::count, "execute {int} steps (default = 1)"s );
    auto stepoutopts = cmd::make_option_set< command::WithSteps >( v )
        .option( "[--out]", &command::WithSteps::out, "execute until the current function returns"s );
    auto threadopts = cmd::make_option_set< command::Thread >( v )
        .option( "[--random]", &command::Thread::random, "pick the thread to run randomly"s )
        .option( "[{string}]", &command::Thread::spec, "stick to the given thread"s );
    auto setupopts = cmd::make_option_set< command::Setup >( v )
        .option( "[--debug-kernel]", &command::Setup::debug_kernel, "enable kernel debugging"s );
    auto o_trace = cmd::make_option_set< command::Trace >( v )
        .option( "[--from {string}]", &command::Trace::from,
                 "start in a given state, instead of initial"s );

    auto parser = cmd::make_parser( v )
        .command< command::Exit >( "exit from divine"s )
        .command< command::Help >( "show this help, or describe a particular command in more detail"s,
                                   cmd::make_option( v, "[{string}]", &command::Help::_cmd ) )
        .command< command::Start >( "boot the system and stop at main()"s )
        .command< command::Break >( "insert a breakpoint"s, &command::Break::where, breakopts )
        .command< command::StepA >( "execute one atomic action"s, varopts, stepopts )
        .command< command::Step >( "execute source line"s, varopts, stepopts, stepoutopts )
        .command< command::StepI >( "execute one instruction"s, varopts, stepopts )
        .command< command::Rewind >( "rewind to a stored program state"s, varopts )
        .command< command::Set >( "set a variable "s, &command::Set::options )
        .command< command::BitCode >( "show the bitcode of the current function"s, varopts )
        .command< command::Source >( "show the source code of the current function"s, varopts )
        .command< command::Thread >( "control thread scheduling"s, threadopts )
        .command< command::Trace >( "load a counterexample trace"s,
                                    &command::Trace::choices, o_trace )
        .command< command::Show >( "show an object"s, varopts, showopts )
        .command< command::Setup >( "set configuration options"s, setupopts )
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

    std::cerr << logo << std::endl;
    std::cerr << "Welcome to 'divine sim', an interactive debugger. Type 'help' to get started."
              << std::endl;

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
