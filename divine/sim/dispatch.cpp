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

#include <divine/dbg/util.hpp>
#include <divine/dbg/dot.hpp>
#include <divine/sim/cli.hpp>
#include <brick-proc>

namespace divine::sim
{

void CLI::go( command::Exit )
{
    _exit = true;
}

void CLI::go( command::Start s )
{
    vm::setup::boot( _ctx );
    if ( s.noboot )
        return set( "$_", frameDN() );

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
    auto step = stepper( s, false );
    if ( !s.out )
        step.lines( s.count );
    run( step, !s.quiet );
    set( "$_", frameDN() );
}

void CLI::go( command::StepI s )
{
    auto step = stepper( s, false );
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

void CLI::go( command::BackTrace backtrace )
{
    dbg::DNSet visited;

    auto fmt = [this]( auto dn )
    {
        auto loc = dn.attribute( "location" ), sym = dn.attribute( "symbol" );
        out() << "  " << sym;
        if ( sym.size() + loc.size() >= 80 && !_batch )
            out() << std::endl << "    ";
        out() << " at " << loc << std::endl;
    };

    auto bt = [&]( int id )
    {
        out() << "# backtrace " << id << ":" << std::endl;
    };

    int stacks = 0;
    dbg::backtrace( bt, fmt, get( backtrace.var ), visited, stacks, 100 );
}

void CLI::go( command::Show cmd )
{
    auto dn = get( cmd.var );
    if ( cmd.raw )
        out() << dn.attribute( "raw" ) << std::endl;
    else
        dn.format( out(), cmd.depth, cmd.deref );
}

void CLI::go( command::Diff cmd )
{
    if ( cmd.vars.size() != 2 )
        throw brick::except::Error( "Diff needs exactly 2 arguments." );
    dbg::diff( std::cerr, get( cmd.vars[0] ), get( cmd.vars[1] ) );
}

void CLI::go( command::Info inf )
{
    OneLineTokenizer tok;

    if ( inf.setup.empty() )
    {
        if ( inf.cmd == "registers" )
            return dump_registers();
        /* other builtins? */
        if ( !_info_cmd.count( inf.cmd ) )
            throw brick::except::Error( "No such info sub-command: " + inf.cmd );
        command_raw( _info_cmd[ inf.cmd ] );
    }
    else
        _info_cmd[ inf.cmd ] = tok.tokenize( inf.setup );
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
    vm::Eval< Context > eval( ctx );
    ctx.flags_set( 0, _VM_CF_KernelMode | _VM_CF_AutoSuspend | _VM_CF_Stop );
    ctx.enter_debug();
    ctx.flags_set( _VM_CF_Stop, 0 );
    make_frame( ctx, pc, PointerV() );
    eval.run();
    for ( auto t : ctx._trace )
        out() << "  " << t << std::endl;
}

void CLI::go( command::Up )
{
    check_running();
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
    check_running();
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
    if ( ( set.debug_everything || !set.debug_components.empty() ) && !set.ignore_components.empty() )
        throw brick::except::Error( "sorry, cannot mix --ignore and --debug" );

    for ( auto c : set.debug_components )
        _ff_components &= ~Components( c );
    for ( auto c : set.ignore_components )
        _ff_components |= c;
    if ( set.debug_everything )
        _ff_components = dbg::Components();
}

}
