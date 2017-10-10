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

#include <divine/vm/dbg-util.hpp>
#include <divine/vm/dbg-dot.hpp>
#include <divine/vm/vmutil.h>
#include <divine/sim/cli.hpp>
#include <brick-proc>

namespace divine::sim
{

std::ostream &dump_flag( uint64_t val, std::ostream &o )
{
    if ( const char *n = __vmutil_flag_name( val ) )
        return o << n;

    return o << std::hex << "0x" << val << std::dec;
}

void dump_flags( uint64_t flags, std::ostream &o )
{
    bool any = false;
    for ( uint64_t m = 1; m; m <<= 1 )
    {
        if ( flags & m )
        {
            if ( any )
                o << " | ";
            dump_flag( m, o );
            any = true;
        }
    }

    if ( !any )
        o << 0;

    o << std::endl;
}

void CLI::go( command::Exit )
{
    _exit = true;
}

void CLI::go( command::Start s )
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

void CLI::go( command::Break b )
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

void CLI::go( command::Step s )
{
    auto step = stepper( s, true );
    if ( !s.out )
        step.lines( s.count );
    run( step, !s.quiet );
    set( "$_", frameDN() );
}

void CLI::go( command::StepI s )
{
    auto step = stepper( s, true );
    step.instructions( s.count );
    run( step, !s.quiet );
    set( "$_", frameDN() );
}

void CLI::go( command::StepA s )
{
    auto step = stepper( s, false );
    step.states( s.count );
    run( step, s.verbose );
    set( "$_", frameDN() );
}

void CLI::go( command::Rewind re )
{
    auto tgt = get( re.var );
    _ctx.load( tgt.snapshot() );
    vm::setup::scheduler( _ctx );
    if ( update_lock( tgt.snapshot() ) )
        out() << "# rewound to a trace location, locking the scheduler" << std::endl;
    reach_user();
    set( "$_", re.var );
}

void CLI::go( command::BackTrace bt )
{
    dbg::DNSet visited;
    int stacks = 0;
    dbg::backtrace( out(), get( bt.var ), visited, stacks, 100 );
}

void CLI::go( command::Show cmd )
{
    auto dn = get( cmd.var );
    if ( cmd.raw )
        out() << dn.attribute( "raw" ) << std::endl;
    else
        dn.format( out(), cmd.depth, cmd.deref );
}

void CLI::go( command::Register )
{
    size_t name_length = 0;
    for ( int i = 0; i < _VM_CR_Last; ++i )
        name_length = std::max( name_length, std::strlen( __vmutil_reg_name( i ) ) );

    for ( int i = 0; i < _VM_CR_Last; ++i ) {
        auto name = __vmutil_reg_name( i );
        int pad = name_length - strlen( name );
        out() << name << ": " << std::string( pad, ' ' );
        if ( i == _VM_CR_Flags )
            dump_flags( _ctx.ref( _VM_CR_Flags ).integer, out() );
        else
            out() << std::hex << _ctx.ref( _VM_ControlRegister( i ) ).integer
                    << std::dec << std::endl;
    }
}

void CLI::go( command::Dot cmd )
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

void CLI::go( command::Inspect i )
{
    go( command::Show( i ) );
    set( "$_", i.var );
}

void CLI::go( command::Call c )
{
    auto pc = _ctx.program().functionByName( c.function );
    if ( pc.null() )
        throw brick::except::Error( "the function '" + c.function + "' is not defined" );
    auto &fun = _ctx.program().function( pc );
    if ( fun.argcount )
        throw brick::except::Error( "the function must not take any arguments" );

    Context ctx( _ctx );
    vm::Eval< Context, vm::value::Void > eval( ctx );
    ctx.enter( pc, PointerV() );
    ctx.set( _VM_CR_FaultHandler, vm::nullPointer() );
    ctx.ref( _VM_CR_Flags ).integer |= _VM_CF_Mask | _VM_CF_KernelMode;
    eval.run();
    for ( auto t : ctx._trace )
        out() << "  " << t << std::endl;
    if ( ctx.ref( _VM_CR_Flags ).integer & _VM_CF_Error )
        throw brick::except::Error( "encountered an error while running '" + c.function + "'" );
}

void CLI::go( command::Up )
{
    auto current =  get( "$_" );
    if ( current._kind != dbg::DNKind::Frame )
        throw brick::except::Error( "$_ not set to a frame, can't go up" );

    auto up = frame_up( current );
    if ( !up.valid() )
        throw brick::except::Error( "outermost frame selected, can't go up" );
    set( "$_", up );
}

void CLI::go( command::Down )
{
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

void CLI::go( command::Set s )
{
    if ( s.options.size() != 2 )
        throw brick::except::Error( "2 options are required for set, the variable and the value" );
    set( s.options[0], s.options[1] );
}

void CLI::go( command::Trace tr )
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

void CLI::go( command::Thread thr )
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

void CLI::go( command::BitCode bc )
{
    get( bc.var ).bitcode( out() ); out() << std::flush;
}

void CLI::go( command::Source src )
{
    get( src.var ).source( out(), [this]( std::string txt )
                            {
                                if ( !_pygmentize )
                                    return txt;
                                auto ansi = brick::proc::spawnAndWait(
                                    brick::proc::StdinString( txt ) |
                                    brick::proc::CaptureStdout |
                                    brick::proc::CaptureStderr,
                                    { "pygmentize", "-l", "c++", "-f", "terminal256" } );
                                if ( ansi.ok() )
                                    return ansi.out();
                                else
                                    return txt;
                            } );
    out() << std::flush;
}

void CLI::go( command::Setup set )
{
    OneLineTokenizer tok;
    _debug_kernel = set.debug_kernel;
    if ( set.pygmentize )
        _pygmentize = true;
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

}
