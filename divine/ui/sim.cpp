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
#include <divine/vm/dbg-stepper.hpp>
#include <divine/vm/dbg-node.hpp>
#include <divine/vm/dbg-print.hpp>
#include <divine/vm/dbg-dot.hpp>

#include <divine/ui/cli.hpp>
#include <divine/ui/logo.hpp>
#include <brick-string>
#include <brick-yaml>
#include <cstring>

#if OPT_SIM
#include <histedit.h>
#include <sys/stat.h>
#include <pwd.h>

namespace divine {

namespace str::doc { extern const std::string_view manual_sim_md; }

namespace ui {

using namespace std::literals;
namespace cmd = brick::cmd;

struct OneLineTokenizer
{
    OneLineTokenizer() : _tok( tok_init( nullptr ) ) {}
    ~OneLineTokenizer() { tok_end( _tok ); }

    cmd::Tokens tokenize( const std::string& s )
    {
        int argc;
        const char **argv;
        tok_reset( _tok );
        int r = tok_str( _tok, s.c_str(), &argc, &argv );
        if ( r == -1 )
            throw brick::except::Error{ "Uknown tokenizer error" };
        if ( r > 0 )
            throw brick::except::Error{ "Unmatched quotes" };
        cmd::Tokens ret;
        std::copy_n( argv, argc, std::back_inserter( ret ) );
        return ret;
    }

    ::Tokenizer *_tok;
};


namespace sim {

vm::Choice parse_choice( std::string s )
{
    auto i = s.find( '/' );
    if ( i == std::string::npos )
        throw std::runtime_error( "unexpected choice format: " + s );
    return vm::Choice( std::stoi( std::string( s, 0, i ) ),
                       std::stoi( std::string( s, i + 1, std::string::npos ) ) );
}

vm::Interrupt parse_interrupt( std::string s )
{
    auto i = s.find( '/' ), p = s.rfind( '/' );
    if ( i == std::string::npos || p == std::string::npos )
        throw std::runtime_error( "unexpected interrupt format: " + s );
    std::string type( s, 0, i ), ictr( s, i + 1, p ), pc( s, p + 1, std::string::npos );
    auto c = pc.find( ':' );
    if ( c == std::string::npos )
        throw std::runtime_error( "unexpected program counter format: " + pc );
    vm::Interrupt res;

    if ( type == "M" )
        res.type = vm::Interrupt::Mem;
    else if ( type == "C" )
        res.type = vm::Interrupt::Cfl;
    else
        throw std::runtime_error( "unexpected interrupt type: " + type );

    res.ictr = std::stoi( ictr );
    res.pc.function( std::stoi( std::string( pc, 0, c ) ) );
    res.pc.instruction( std::stoi( std::string( pc, c + 1, std::string::npos ) ) );

    return res;
}

struct Trace
{
    std::deque< vm::Step > steps;
};

namespace command {

struct CastIron {}; // finalize with sticky commands
struct Teflon       // finalize without sticky commands
{
    std::string output_to;
    bool clear_screen = false;
};

struct WithVar
{
    std::string var;
    WithVar( std::string v = "$_" ) : var( v ) {}
};

struct WithFrame : WithVar
{
    WithFrame() : WithVar( "$frame" ) {}
};

struct Set : CastIron { std::vector< std::string > options; };
struct WithSteps : WithFrame
{
    bool over, out, quiet, verbose; int count;
    WithSteps() : over( false ), out( false ), quiet( false ), verbose( false ), count( 1 ) {}
};

struct Start : CastIron
{
    bool verbose;
    Start() : verbose( false ) {}
};

struct Break : Teflon
{
    std::vector< std::string > where;
    bool list;
    brick::types::Union< std::string, int > del;
    Break() : list( false ) {}
};

struct StepI : WithSteps, CastIron {};
struct StepA : WithSteps, CastIron {};
struct Step : WithSteps, CastIron {};
struct Rewind : WithVar, CastIron
{
    Rewind() : WithVar( "#last" ) {}
};

struct Show : WithVar, Teflon
{
    bool raw;
    int depth, deref;
    Show() : raw( false ), depth( 10 ), deref( 0 ) {}
};

struct Dot : WithVar, Teflon
{
    std::string type = "none";
    std::string output_file;
};

struct Draw : Dot
{
    Draw() { type = "x11"; }
};

struct Inspect : Show {};
struct BitCode : WithFrame, Teflon {};
struct Source : WithFrame, Teflon {};
struct Thread : CastIron  { std::string spec; bool random; };

struct Trace : CastIron, sim::Trace
{
    Trace() = default;
    explicit Trace( const sim::Trace &tr ) : sim::Trace( tr ) {}
    std::string from;
    std::vector< int > simple_choices;
};

struct BackTrace : WithVar, Teflon
{
    BackTrace() : WithVar( "$top" ) {}
};

struct Setup : Teflon
{
    bool debug_kernel;
    bool clear_sticky;
    std::string xterm;
    std::vector< std::string > sticky_commands;
    Setup() : debug_kernel( false ), clear_sticky( false ) {}
};

struct Down : CastIron {};
struct Up : CastIron {};

struct Exit : Teflon {};
struct Help : Teflon { std::string _cmd; };

}

using Context = vm::dbg::Context< vm::CowHeap >;
namespace dbg = vm::dbg;

struct Interpreter
{
    using BC = std::shared_ptr< vm::BitCode >;
    using DN = vm::dbg::Node< vm::Program, vm::CowHeap >;
    using PointerV = Context::PointerV;
    using Stepper = vm::dbg::Stepper< Context >;
    using RefCnt = brick::mem::RefCnt< typename Context::RefCnt >;

    bool _exit, _batch;
    BC _bc;

    std::vector< std::string > _env;

    std::map< std::string, DN > _dbg;
    std::map< vm::CowHeap::Snapshot, RefCnt > _state_refs;
    std::map< vm::CowHeap::Snapshot, std::string > _state_names;
    std::map< vm::CowHeap::Snapshot, vm::Step > _trace;
    vm::Explore _explore;

    std::pair< int, int > _sticky_tid;
    std::mt19937 _rand;
    bool _sched_random, _debug_kernel;

    std::vector< cmd::Tokens > _sticky_commands;

    Context _ctx;

    using RefLocation = std::pair< llvm::StringRef, int >;
    using Location = std::pair< std::string, int >;
    using Breakpoint = brick::types::Union< vm::CodePointer, Location >;
    std::vector< Breakpoint > _bps;

    char *_prompt;
    int _state_count;

    static bool *_sigint;

    std::map< std::string, brick::proc::XTerm > _xterms;
    std::ostream *_stream;
    std::ostream &out() { return *_stream; }

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
        auto stepopts = cmd::make_option_set< command::WithSteps >( v )
            .option( "[--over]", &command::WithSteps::over, "execute calls as one step"s )
            .option( "[--quiet]", &command::WithSteps::quiet, "suppress output"s )
            .option( "[--verbose]", &command::WithSteps::verbose, "increase verbosity"s )
            .option( "[--count {int}]", &command::WithSteps::count,
                     "execute {int} steps (default = 1)"s );
        auto startopts = cmd::make_option_set< command::Start >( v )
            .option( "[--verbose]", &command::Start::verbose, "increase verbosity"s );
        auto stepoutopts = cmd::make_option_set< command::WithSteps >( v )
            .option( "[--out]", &command::WithSteps::out,
                     "execute until the current function returns"s );
        auto threadopts = cmd::make_option_set< command::Thread >( v )
            .option( "[--random]", &command::Thread::random, "pick the thread to run randomly"s )
            .option( "[{string}]", &command::Thread::spec, "stick to the given thread"s );
        auto setupopts = cmd::make_option_set< command::Setup >( v )
            .option( "[--debug-kernel]", &command::Setup::debug_kernel, "enable kernel debugging"s )
            .option( "[--xterm {string}]", &command::Setup::xterm, "setup & name an X terminal"s )
            .option( "[--clear-sticky]", &command::Setup::clear_sticky, "remove sticky commands"s )
            .option( "[--sticky {string}]", &command::Setup::sticky_commands,
                     "run given commands after each step"s );

        auto o_trace = cmd::make_option_set< Trace >( v )
            .option( "[{step}+]", &Trace::steps, "step descriptions" );
        auto o_trace_cmd = cmd::make_option_set< command::Trace >( v )
            .option( "--choices {int}+", &command::Trace::simple_choices, "use simple choices" )
            .option( "[--from {string}]", &command::Trace::from,
                     "start in a given state, instead of initial"s );

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
            .command< command::Draw >( "draw a portion of the heap"s, varopts )
            .command< command::Dot >( "draw a portion of the heap to a file of given type"s,
                                      teflopts, varopts, dotopts )
            .command< command::Setup >( "set configuration options"s, setupopts )
            .command< command::Inspect >( "like show, but also set $_"s, teflopts, varopts, showopts )
            .command< command::BackTrace >( "show a stack trace"s, teflopts, varopts )
            .command< command::Up >( "move up the stack (towards caller)"s )
            .command< command::Down >( "move down the stack (towards callee)"s );
    }

    void command( cmd::Tokens cmd );
    char *prompt() { return _prompt; }

    RefLocation location( vm::CodePointer pc )
    {
        auto npc = _bc->program().nextpc( pc );
        auto insn = _bc->debug().find( nullptr, npc ).first;
        return dbg::fileline( *insn );
    };

    DN dn( vm::GenericPointer p, dbg::DNKind k, llvm::Type *t, llvm::DIType *dit )
    {
        DN r( _ctx, _ctx.snapshot() );
        r.address( k, p );
        r.type( t );
        r.di_type( dit );
        return r;
    }

    DN nullDN() { return dn( vm::nullPointer(), dbg::DNKind::Object, nullptr, nullptr ); }
    DN frameDN() { return dn( _ctx.frame(), dbg::DNKind::Frame, nullptr, nullptr ); }

    DN objDN( vm::GenericPointer p, llvm::Type *t, llvm::DIType *dit )
    {
        return dn( p, dbg::DNKind::Object, t, dit );
    }

    void set( std::string n, DN dn ) { _dbg.erase( n ); _dbg.emplace( n, dn ); }

    DN get( std::string n, bool silent = false,
            std::unique_ptr< DN > start = nullptr, bool comp = false )
    {
        if ( !start && n[0] != '$' && n[0] != '#' ) /* normalize */
        {
            if ( n[0] == '.' || n[0] == ':' )
                n = "$_" + n;
            else
                n = "$_:" + n;
        }

        auto split = std::min( n.find( '.' ), n.find( ':' ) );
        std::string head( n, 0, split );
        std::string tail( n, split < n.size() ? split + 1 : head.size(), std::string::npos );

        if ( !start )
        {
            auto i = _dbg.find( head );
            if ( i == _dbg.end() && silent )
                return nullDN();
            if ( i == _dbg.end() )
                throw brick::except::Error( "variable " + head + " is not defined" );

            auto dn = i->second;
            switch ( head[0] )
            {
                case '$': dn.relocate( _ctx.snapshot() ); break;
                case '#': break;
                default: UNREACHABLE( "impossible case" );
            }
            if ( split >= n.size() )
                return dn;
            else
                return get( tail, silent, std::make_unique< DN >( dn ), n[split] == '.' );
        }

        std::unique_ptr< DN > dn_next;

        auto lookup = [&]( auto key, auto rel )
                          {
                              if ( head == key )
                                  dn_next = std::make_unique< DN >( rel );
                          };

        if ( comp )
            start->components( lookup );
        else
            start->related( lookup );

        if ( silent && !dn_next )
            return nullDN();
        if ( !dn_next )
            throw brick::except::Error( "lookup failed at " + head );

        if ( split >= n.size() )
            return *dn_next;
        else
            return get( tail, silent, std::move( dn_next ), n[split] == '.' );
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
            gdn.address( dbg::DNKind::Globals, globals );
            set( "$globals", gdn );
        }

        auto state = _ctx.get( _VM_CR_State ).pointer;
        if ( !get( "$state", true ).valid() || get( "$state" ).address() != state )
        {
            DN sdn( _ctx, _ctx.snapshot() );
            sdn.address( dbg::DNKind::Object, state );
            sdn.type( _ctx._state_type );
            sdn.di_type( _ctx._state_di_type );
            set( "$state", sdn );
        }

        if ( get( "$_" ).kind() == dbg::DNKind::Frame )
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
        auto sym = top.attribute( "symbol" ), loc = top.attribute( "location" );
        out() << "# executing " << sym;
        if ( sym.size() + loc.size() > 60 && !_batch )
            out() << std::endl << "#        at ";
        else
            out() << " at ";
        out() << loc << std::endl;
        if ( frame._address != top._address )
            out() << "# NOTE: $frame in " << frame.attribute( "symbol" ) << std::endl;
    }

    Interpreter( BC bc )
        : _exit( false ), _batch( false ), _bc( bc ), _explore( bc ), _sticky_tid( -1, 0 ),
          _sched_random( false ), _debug_kernel( false ), _ctx( _bc->program(), _bc->debug() ),
          _state_count( 0 ), _stream( &std::cerr )
    {
        _ctx._lock_mode = Context::LockScheduler;
        vm::setup::boot( _ctx );
        _prompt = strdup( "> " );
        set( "$_", nullDN() );
        update();
    }

    bool update_lock( vm::CowHeap::Snapshot snap )
    {
        if ( !_trace.count( snap ) )
            return false;

        _ctx._lock = _trace[ snap ];
        return true;
    }

    auto newstate( vm::CowHeap::Snapshot snap, bool update_choices = true, bool terse = false )
    {
        snap = _explore.start( _ctx, snap );
        _explore.pool().sync();
        _ctx.load( snap );
        _ctx._lock_mode = _trace.count( snap ) ? Context::LockBoth : Context::LockScheduler;

        bool isnew = false;
        std::string name;

        if ( _state_names.count( snap ) )
        {
            name = _state_names[ snap ];
            if ( update_choices )
                update_lock( snap );
        }
        else
        {
            isnew = true;
            name = _state_names[ snap ] = "#"s + brick::string::fmt( ++_state_count );
            _state_refs[ snap ] = RefCnt( _ctx._refcnt, snap );
            DN state( _ctx, snap );
            state.address( dbg::DNKind::Object, _ctx.get( _VM_CR_State ).pointer );
            state.type( _ctx._state_type );
            state.di_type( _ctx._state_di_type );
            set( name, state );
        }

        set( "#last", name );

        if ( terse )
            out() << " " << name << std::flush;
        else if ( isnew )
            out() << "# a new program state was stored as " << name << std::endl;
        else if ( _trace.count( snap ) )
            out() << "# program follows the trace at " << name
                  << " (the scheduler is locked)"<< std::endl;
        else
            out() << "# program entered state " << name << " (already seen)" << std::endl;

        return snap;
    }

    void sched_policy()
    {
        auto &proc = _ctx._proc;
        auto &choices = _ctx._lock.choices;

        if ( proc.empty() )
            return;

        std::uniform_int_distribution< int > dist( 0, proc.size() - 1 );
        vm::Choice choice( -1, proc.size() );

        if ( _sched_random )
            choice.taken = dist( _rand );
        else
            for ( auto pi : proc )
                if ( pi.first == _sticky_tid )
                    choice.taken = pi.second;

        if ( choice.taken < 0 )
        {
            /* thread is gone, pick a replacement at random */
            int seq = dist( _rand );
            _sticky_tid = proc[ seq ].first;
            choice.taken = proc[ seq ].second;
        }

        if ( _ctx._lock_mode == Context::LockScheduler )
        {
            ASSERT( choices.empty() );
            choices.push_back( choice );
        }

        out() << "# active threads:";
        for ( auto pi : proc )
        {
            bool active = pi.second == choices.front().taken;
            out() << ( active ? " [" : " " )
                      << pi.first.first << ":" << pi.first.second
                      << ( active ? "]" : "" );
        }
        proc.clear();
        out() << std::endl;
    }

    void check_running()
    {
        if ( _ctx.frame().null() )
            throw brick::except::Error( "the program has already terminated" );
    }

    void run( Stepper &step, bool verbose )
    {
        check_running();
        return run( step, verbose ? Stepper::PrintInstructions : Stepper::TraceOnly );
    }

    void run( Stepper &step, Stepper::Verbosity verbose )
    {
        _sigint = &step._sigint;
        brick::types::Defer _( [](){ _sigint = nullptr; } );

        RefLocation initial = location( _ctx.get( _VM_CR_PC ).pointer );
        auto bp = [&]( vm::CodePointer pc, bool ch )
                  {
                      if ( ch && initial.second )
                          if ( location( pc ) != initial )
                              initial = std::make_pair( "", 0 );
                      for ( auto bp : _bps )
                          if ( bp.match( [&]( vm::CodePointer bp_pc )
                                         {
                                             if ( pc != bp_pc )
                                                 return false;
                                             auto name = _bc->debug().function( pc )->getName();
                                             out() << "# stopped at breakpoint " << name.str()
                                                   << std::endl;
                                             return true;
                                         },
                                         [&]( Location l )
                                         {
                                             RefLocation rl = l;
                                             if ( initial.second == rl.second && rl.first == initial.first )
                                                 return false;
                                             auto current = location( pc );
                                             if ( rl.second != current.second )
                                                 return false;
                                             if ( !brick::string::endsWith( current.first, l.first ) )
                                                 return false;
                                             out() << "# stopped at breakpoint "
                                                   << l.first << ":" << l.second << std::endl;
                                             return true;
                                         } ) )
                              return true;
                      return false;
                  };
        if ( !step._breakpoint )
            step._breakpoint = bp;
        step.run( _ctx, verbose );
    }

    void go( command::Exit ) { _exit = true; }

    Stepper stepper()
    {
        Stepper step;
        step._ff_kernel = !_debug_kernel;
        step._sched_policy = [this]() { sched_policy(); };
        step._yield_state = [this]( auto snap ) { return newstate( snap ); };
        return step;
    }

    Stepper stepper( command::WithSteps s, bool jmp )
    {
        auto step = stepper();
        check_running();
        if ( jmp )
            step.jumps( 1 );
        if ( s.over || s.out )
            step.frame( get( s.var ).address() );
        return step;
    }

    void go( command::Start s )
    {
        vm::setup::boot( _ctx );
        auto step = stepper();
        step._booting = true;
        auto mainpc = _bc->program().functionByName( "main" );
        step._breakpoint = [mainpc]( vm::CodePointer pc, bool ) { return pc == mainpc; };
        run( step, s.verbose );
        if ( !_ctx._info.empty() )
            out() << "# boot info:\n" << _ctx._info;
        set( "$_", frameDN() );
    }

    void bplist( command::Break b )
    {
        std::deque< decltype( _bps )::iterator > remove;
        int id = 1;
        if ( _bps.empty() )
            out() << "no breakpoints defined" << std::endl;
        for ( auto bp : _bps )
        {
            out() << std::setw( 2 ) << id << ": ";
            bool del_this = !b.del.empty() && id == b.del;
            bp.match(
                [&]( vm::CodePointer pc )
                {
                    auto fun = _bc->debug().function( pc )->getName().str();
                    out() << fun << " +" << pc.instruction()
                          << " (at " << dbg::location( location( pc ) ) << ")";
                    if ( !b.del.empty() && !pc.instruction() && b.del == fun )
                        del_this = true;
                },
                [&]( Location loc )
                {
                    out() << loc.first << ":" << loc.second;
                } );

            if ( del_this )
            {
                remove.push_front( _bps.begin() + id - 1 );
                out() << " [deleted]";
            }
            out() << std::endl;
            ++ id;
        }

        for ( auto r : remove ) /* ordered from right to left */
            _bps.erase( r );
    }

    void go( command::Break b )
    {
        if ( b.list || !b.del.empty() )
            bplist( b );
        else
            for ( auto w : b.where )
                if ( w.find( ':' ) == std::string::npos )
                {
                    auto pc = _bc->program().functionByName( w );
                    if ( pc.null() )
                        throw brick::except::Error( "Could not find " + w );
                    _bps.emplace_back( pc );
                }
                else
                {
                    std::string file( w, 0, w.find( ':' ) );
                    int line;
                    try {
                        line = std::stoi( std::string( w, w.find( ':' ) + 1,
                                          std::string::npos ) );
                    } catch ( std::invalid_argument &e ) {
                        throw brick::except::Error( "Line number expected after ':'" );
                    }
                    _bps.emplace_back( std::make_pair( file, line ) );
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

    void reach_user()
    {
        Stepper step;
        step._instructions = std::make_pair( 1, 1 );
        run( step, false ); /* make 0 (user mode) steps */
    }

    void go( command::Rewind re )
    {
        auto tgt = get( re.var );
        _ctx.load( tgt.snapshot() );
        vm::setup::scheduler( _ctx );
        if ( update_lock( tgt.snapshot() ) )
            out() << "# rewound to a trace location, locking the scheduler" << std::endl;
        reach_user();
        set( "$_", re.var );
    }

    void go( command::BackTrace bt )
    {
        dbg::DNSet visited;
        int stacks = 0;
        dbg::backtrace( out(), get( bt.var ), visited, stacks, 100 );
    }

    void go( command::Show cmd )
    {
        auto dn = get( cmd.var );
        if ( cmd.raw )
            out() << dn.attribute( "raw" ) << std::endl;
        else
            dn.format( out(), cmd.depth, cmd.deref );
    }

    void go( command::Dot cmd )
    {
        std::string dot = dotDN( get( cmd.var ), true ), print;
        if ( cmd.type == "none" )
            print = dot;
        else {
            auto r = brick::proc::spawnAndWait( brick::proc::StdinString( dot ) |
                                                brick::proc::CaptureStdout, "dot", "-T" + cmd.type );
            if ( !r )
                std::cerr << "ERROR: dot failed" << std::endl;
            print = r.out();
        }
        if ( !cmd.output_file.empty() )
            brick::fs::writeFile( cmd.output_file, print );
        else
            out() << print << std::endl;
    }

    void go( command::Inspect i )
    {
        go( command::Show( i ) );
        set( "$_", i.var );
    }

    DN frame_up( DN frame ) {
        auto fup = std::make_unique< DN >( frame );
        if ( !_debug_kernel ) {
            // TODO: obtain this through some DiOS API?
            std::regex fault( "__dios.*fault_handler", std::regex::basic );
            if ( frame._kind == dbg::DNKind::Frame &&
                    std::regex_search( frame.attribute( "symbol" ), fault  ) )
                return get( "frame:deref", false, std::move( fup ), true );
        }
        return get( "caller", false, std::move( fup ) );
    }

    void go( command::Up ) {
        auto current =  get( "$_" );
        if ( current._kind != dbg::DNKind::Frame )
            throw brick::except::Error( "$_ not set to a frame, can't go up" );
        try {
            set( "$_", frame_up( current ) );
        } catch ( brick::except::Error & ) {
            throw brick::except::Error( "outermost frame selected, can't go up" );
        }
    }

    void go( command::Down ) {
        auto frame = get( "$top" ), prev = frame, current = get( "$_" );
        if ( current._kind != dbg::DNKind::Frame )
            throw brick::except::Error( "$_ not set to a frame, can't go down" );
        if ( frame.address() == current.address() )
            throw brick::except::Error( "bottom (innermost) frame selected, can't go down" );

        frame = frame_up( frame );
        while ( frame.address() != current.address() )
        {
            prev = frame;
            frame = frame_up( frame );
        }
        set( "$_", prev );
    }

    void go( command::Set s )
    {
        if ( s.options.size() != 2 )
            throw brick::except::Error( "2 options are required for set, the variable and the value" );
        set( s.options[0], s.options[1] );
    }

    void go( command::Trace tr )
    {
        auto step = stepper();

        if ( tr.from.empty() )
        {
            vm::setup::boot( _ctx );
            step._booting = true;
        }
        else
        {
            _ctx.load( get( tr.from ).snapshot() );
            vm::setup::scheduler( _ctx );
        }

        _trace.clear();
        bool simple = false;
        _ctx._lock_mode = Context::LockBoth;

        if ( !tr.simple_choices.empty() )
        {
            simple = true;
            _ctx._lock_mode = Context::LockChoices;
            if ( !tr.steps.empty() )
                throw brick::except::Error( "Can't specify both steps and (simple) choices." );
            for ( auto i : tr.simple_choices )
                _ctx._lock.choices.emplace_back( i, 0 );
        }

        std::set< vm::CowHeap::Snapshot > visited;

        auto update_lock = [&]( vm::CowHeap::Snapshot snap )
        {
            _trace[ snap ] = _ctx._lock = tr.steps.front();
            tr.steps.pop_front();
        };

        auto last = get( "#last", true ).snapshot();
        out() << "traced states:";
        bool stop = false;
        step._sched_policy = [&]()
        {
            if ( simple && _ctx._lock.choices.empty() )
                stop = true;
            if ( !simple && tr.steps.empty() )
                stop = true;
        };
        step._breakpoint = [&]( vm::CodePointer, bool ) { return stop; };
        step._stop_on_error = false;
        step._yield_state =
            [&]( auto snap )
            {
                auto next = newstate( snap, false, true );
                if ( visited.count( next ) )
                {
                    out() << " [loop closed]" << std::flush;
                    step._ff_kernel = false;
                    stop = true;
                    return next;
                }
                visited.insert( next );
                _ctx._instruction_counter = 0;
                if ( !simple )
                    update_lock( snap );
                last = get( "#last", true ).snapshot();
                return next;
            };
        run( step, Stepper::Quiet );

        out() << std::endl;

        if ( simple && !_ctx._lock.choices.empty() )
        {
            out() << "unused choices:";
            for ( auto c : _ctx._lock.choices )
                out() << " " << c;
            out() << std::endl;
        }

        if ( !simple && !tr.steps.empty() )
            out() << "WARNING: Program terminated unexpectedly." << std::endl;

        if ( !_ctx._trace.empty() )
            out() << "trace:" << std::endl;;
        for ( auto t : _ctx._trace )
            out() << "T: " << t << std::endl;
        _ctx._trace.clear();
        reach_user();
        _ctx._lock_mode = Context::LockScheduler;
        set( "$_", frameDN() );
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

    void go( command::BitCode bc ) { get( bc.var ).bitcode( out() ); out() << std::flush; }
    void go( command::Source src ) { get( src.var ).source( out() ); out() << std::flush; }
    void go( command::Setup set )
    {
        OneLineTokenizer tok;
        _debug_kernel = set.debug_kernel;
        if ( set.clear_sticky )
            _sticky_commands.clear();
        if ( !set.xterm.empty() )
        {
            brick::proc::XTerm xt;
            xt.open();
            _xterms.emplace( set.xterm, std::move( xt ) );
        }
        for ( const std::string& cmd : set.sticky_commands )
            _sticky_commands.push_back( tok.tokenize( cmd ) );
    }

    void go( command::Help ) { UNREACHABLE( "impossible case" ); }

    template< typename Parser >
    void help( Parser &p, std::string arg )
    {
        if ( arg.empty() )
            out() << str::doc::manual_sim_md << std::endl
                  << "# Command overview" << std::endl << std::endl;
        out() << p.describe( arg ) << std::endl;
    }

    void prepare( const command::Teflon &tfl )
    {
        if ( !tfl.output_to.empty() )
        {
            if ( _xterms.count( tfl.output_to ) )
                _stream = &_xterms[ tfl.output_to ].stream();
            else
                out() << "ERROR: no xterm named " << tfl.output_to << " found!";
        }

        if ( tfl.clear_screen )
            out() << char( 27 ) << "[2J" << char( 27 ) << "[;H";
    }

    void prepare( command::CastIron ) {}

    void finalize( command::Teflon )
    {
        update();
    }

    void finalize( command::CastIron )
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
};

bool *Interpreter::_sigint = nullptr;

char *prompt( EditLine *el )
{
    Interpreter *interp;
    el_get( el, EL_CLIENTDATA, &interp );
    return interp->prompt();
}

void sigint_handler( int raised )
{
    if ( raised != SIGINT )
        abort();
    if ( Interpreter::_sigint )
        *Interpreter::_sigint = true;
}

void Interpreter::command( cmd::Tokens tok )
{
    auto parser = make_parser();
    auto cmd = parser.parse( tok.begin(), tok.end() );
    cmd.match( [&] ( command::Help h ) { help( parser, h._cmd ); },
               [&] ( auto opt ) { prepare( opt ); go( opt ); finalize( opt ); } );
    _stream = &std::cerr; /* always revert to the main output */
}

}

void Sim::run()
{
    auto el = el_init( "divine", stdin, stdout, stderr );
    auto hist = history_init();
    HistEvent hist_ev;
    sim::Interpreter interp( bitcode() );
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

    OneLineTokenizer tok;

    if ( !init_path.empty() && !_skip_init )
    {
        std::ifstream init( init_path.c_str() );
        std::string line;
        if ( init.is_open() )
            while ( std::getline( init, line ), !init.eof() )
                interp.command( tok.tokenize( line ) );
    }

    if ( _trace )
        interp.go( sim::command::Trace( *_trace ) );

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

void Sim::setup()
{
    if ( _load_report )
    {
        std::string yaml = brick::fs::readFile( _file );
        brick::yaml::Parser parsed( yaml );
        _file = parsed.get< std::string >( { "input file" } );

        if ( parsed.get< std::string >( { "error found" } ) == "yes" )
        {
            _trace.reset( new sim::Trace );
            OneLineTokenizer tok;
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

    WithBC::setup();
}

}
}

#else

namespace divine {
namespace ui {

void Sim::setup() {}
void Sim::run()
{
    throw std::runtime_error( "This build of DIVINE does not support the 'sim' command." );
}

}
}

#endif
