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
#include <divine/vm/print.hpp>
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

using DebugContext = vm::DebugContext< vm::Program, vm::CowHeap >;

struct Context : DebugContext
{
    std::deque< int > _choices;

    Context( vm::Program &p ) : DebugContext( p ) {}

    template< typename I >
    int choose( int count, I, I )
    {
        if ( _choices.empty() )
            _choices.emplace_back( 0 );

        ASSERT_LT( _choices.front(), count );
        ASSERT_LEQ( 0, _choices.front() );
        if ( !_proc.empty() )
            _proc.clear();
        auto rv = _choices.front();
        _choices.pop_front();
        return rv;
    }
};

struct Stepper
{
    vm::GenericPointer _frame, _frame_cur, _parent_cur;
    std::pair< int, int > _lines, _instructions, _states, _jumps;
    std::pair< std::string, int > _line;
    std::set< vm::CodePointer > _bps;

    Stepper()
        : _frame( vm::nullPointer() ),
          _frame_cur( vm::nullPointer() ),
          _parent_cur( vm::nullPointer() ),
          _lines( 0, 0 ), _instructions( 0, 0 ),
          _states( 0, 0 ), _jumps( 0, 0 ),
          _line( "", 0 )
    {}

    void lines( int l ) { _lines.second = l; }
    void instructions( int i ) { _instructions.second = i; }
    void states( int s ) { _states.second = s; }
    void jumps( int j ) { _jumps.second = j; }
    void frame( vm::GenericPointer f ) { _frame = f; }
    auto frame() { return _frame; }

    void add( std::pair< int, int > &p )
    {
        if ( _frame.null() || _frame == _frame_cur )
            p.first ++;
    }

    bool _check( std::pair< int, int > &p )
    {
        return p.second && p.first >= p.second;
    }

    template< typename Eval >
    bool check( Context &ctx, Eval &eval )
    {
        for ( auto bp : _bps )
            if ( eval.pc() == bp )
                return true;

        if ( !_frame.null() && !ctx.heap().valid( _frame ) )
            return true;
        if ( _check( _jumps ) )
            return true;
        if ( !_frame.null() && _frame_cur != _frame )
            return false;
        return _check( _lines ) || _check( _instructions ) || _check( _states );
    }

    void instruction( vm::Program::Instruction &i )
    {
        add( _instructions );
        if ( i.op )
        {
            auto l = vm::fileline( *llvm::cast< llvm::Instruction >( i.op ) );
            if ( _line.second && l != _line )
                add( _lines );
            if ( _frame.null() || _frame == _frame_cur )
                _line = l;
        }
    }

    void state() { add( _states ); }
    void in_frame( vm::GenericPointer next, vm::CowHeap &heap )
    {
        vm::GenericPointer last = _frame_cur, last_parent = _parent_cur;
        vm::value::Pointer next_parent;

        heap.read( next + vm::PointerBytes, next_parent );

        _frame_cur = next;
        _parent_cur = next_parent.cooked();

        if ( last == next )
            return; /* no change */
        if ( last.null() || next.null() )
            return; /* entry or exit */

        if ( next == last_parent )
            return; /* return from last into next */
        if ( next_parent.cooked() == last )
            return; /* call from last into next */

        ++ _jumps.first;
    }
};

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
        return DN( _ctx, _ctx.heap().snapshot(), p, 0, k, t, dit );
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
            case '$': dn.relocate( _ctx.heap().snapshot() ); break;
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
            [&]( std::string k, auto v )
            {
                if ( k != "@raw" || detailed )
                    std::cerr << k << ": " << v << std::endl;
            } );
        std::cerr << "related:" << std::flush;
        std::stringstream valued;
        int col = 0;
        dn.related( [&]( std::string n, auto sub )
                    {
                        if ( col + n.size() >= 68 )
                            col = 0, std::cerr << std::endl << "        ";
                        auto val = attr( sub, "@value" );
                        if ( val == "-" )
                        {
                            std::cerr << " " << n;
                            col += n.size();
                        }
                        else
                            valued << "  " << n << ": " << val << std::endl;
                    } );
        std::cerr << std::endl << valued.str();
    }

    void info()
    {
        auto top = get( "$top" ), frame = get( "$frame" );
        auto sym = attr( top, "@symbol" ), loc = attr( top, "@location" );
        std::cerr << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 )
            std::cerr << std::endl << "#        at ";
        else
            std::cerr << " at ";
        std::cerr << loc << std::endl;
        if ( frame._address != top._address )
            std::cerr << "# NOTE: $frame in " << attr( frame, "@symbol" ) << std::endl;
    }

    Interpreter( BC bc )
        : _exit( false ), _bc( bc ), _explore( bc ), _ctx( _bc->program() ), _state_count( 0 ),
          _sticky_tid( -1, 0 ), _sched_random( false ), _debug_kernel( false )
    {
        vm::setup::boot( _ctx );
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

    bool schedule( Eval &eval, bool update_choices = true, bool terse = false )
    {
        if ( !_ctx.frame().null() )
            return false; /* nothing to be done */

        if ( _ctx.ref( _VM_CR_Flags ).integer & _VM_CF_Cancel )
            return true;

        auto snap = _explore.start( _ctx, _ctx.heap().snapshot() );
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
            set( name, DN( _ctx, snap, _ctx.get( _VM_CR_State ).pointer, 0, vm::DNKind::Object,
                           _ctx._state_type, _ctx._state_di_type ) );
        }

        set( "#last", name );

        if ( terse )
            std::cerr << " " << name << std::flush;
        else if ( isnew )
            std::cerr << "# a new program state was stored as " << name << std::endl;
        else
            std::cerr << "# program entered state " << name << " (already seen)" << std::endl;

        vm::setup::scheduler( _ctx );

        return true;
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

    void run( Stepper step, bool verbose )
    {
        check_running();
        Eval eval( _bc->program(), _ctx );
        bool in_fault = eval.pc().function() == _ctx.get( _VM_CR_FaultHandler ).pointer.object();
        bool in_kernel = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;

        while ( !_ctx.frame().null() &&
                ( ( !_debug_kernel && in_kernel ) || !step.check( _ctx, eval ) ) &&
                ( in_fault || eval.pc().function()
                  != _ctx.get( _VM_CR_FaultHandler ).pointer.object() ) )
        {
            in_kernel = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;

            if ( in_kernel && !_debug_kernel )
                eval.advance();
            else
            {
                step.in_frame( _ctx.frame(), _ctx.heap() );
                eval.advance();
                step.instruction( eval.instruction() );
            }

            if ( verbose && ( !in_kernel || _debug_kernel ) )
            {
                auto frame = _ctx.frame();
                std::string before = vm::print::instruction( eval );
                eval.dispatch();
                if ( _ctx.heap().valid( frame ) )
                {
                    auto newframe = _ctx.frame();
                    _ctx.set( _VM_CR_Frame, frame ); /* :-( */
                    std::cerr << vm::print::instruction( eval ) << std::endl;
                    _ctx.set( _VM_CR_Frame, newframe );
                }
                else
                    std::cerr << before << std::endl;
            }
            else
                eval.dispatch();

            if ( schedule( eval ) )
                step.state();

            in_kernel = _ctx.get( _VM_CR_Flags ).integer & _VM_CF_KernelMode;
            if ( !in_kernel || _debug_kernel )
                step.in_frame( _ctx.frame(), _ctx.heap() );

            if ( !_ctx._proc.empty() )
            {
                if ( _ctx._choices.empty() )
                    _ctx._choices.push_back( sched_policy( _ctx._proc ) );
                std::cerr << "# active threads:";
                for ( auto pi : _ctx._proc )
                {
                    bool active = pi.second == _ctx._choices.front();
                    std::cerr << ( active ? " [" : " " )
                              << pi.first.first << ":" << pi.first.second
                              << ( active ? "]" : "" );
                }
                _ctx._proc.clear();
                std::cerr << std::endl;
            }

            for ( auto t : _ctx._trace )
                std::cerr << "T: " << t << std::endl;
            _ctx._trace.clear();
        }
    }

    void go( command::Exit ) { _exit = true; }

    Stepper stepper( command::WithSteps s, bool jmp )
    {
        Stepper step;
        step._bps = _bps;
        check_running();
        if ( jmp )
            step.jumps( 1 );
        if ( s.over || s.out )
            step.frame( get( s.var ).address() );
        return step;
    }

    void go( command::Start st )
    {
        vm::setup::boot( _ctx );
        Stepper step;
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
        Stepper step;
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
            show( get( "$$" ) );
            set( "$$", "$$.@parent", true );
            std::cerr << std::endl;
        } while ( get( "$$" ).valid() );
    }

    void go( command::Show s )
    {
        auto dn = get( s.var );
        if ( s.raw )
            std::cerr << attr( dn, "@raw" ) << std::endl;
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

        Eval eval( _bc->program(), _ctx );
        auto last = get( "#last", true ).snapshot();
        std::cerr << "traced states:";
        while ( !_ctx._choices.empty() )
        {
            eval.advance();
            eval.dispatch();
            if ( schedule( eval, false, true ) )
            {
                int count = choices.size() - _ctx._choices.size();
                auto b = choices.begin(), e = b + count;
                _trace[ last ] = std::deque< int >( b, e );
                choices.erase( b, e );
                last = get( "#last", true ).snapshot();
            }
        }

        std::cerr << std::endl;
        _ctx._trace.clear();
        Stepper step;
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
        .option( "[--from {string}]", &command::Trace::from, "start in a given state, instead of initial"s );

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
