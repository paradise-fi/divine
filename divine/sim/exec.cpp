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

#include <divine/sim/cli.hpp>

namespace divine::sim
{

void CLI::run( Stepper &step, bool verbose )
{
    check_running();
    return run( step, verbose ? Stepper::PrintInstructions : Stepper::TraceOnly );
}

void CLI::run( Stepper &step, Stepper::Verbosity verbose )
{
    _sigint = &step._sigint;
    brick::types::Defer _( [](){ _sigint = nullptr; } );

    auto initial = location( _ctx.pc() );
    if ( !step._breakpoint )
        step._breakpoint = [&]( auto a, auto b ) { return check_bp( initial, a, b ); };

    step.run( _ctx, verbose );
}

Stepper CLI::stepper()
{
    Stepper step;
    step._ff_components = _ff_components;
    step._callback = [this]() { sched_policy(); return true; };
    step._yield_state = [this]( auto snap ) { return newstate( snap ); };
    return step;
}

Stepper CLI::stepper( command::with_steps s, bool jmp )
{
    auto step = stepper();
    check_running();
    if ( jmp )
        step.jumps( 1 );
    if ( s.over || s.out )
        step.frame( get( s.var ).address() );
    return step;
}

void CLI::reach_user()
{
    Stepper step = stepper();
    step._instructions = std::make_pair( 1, 1 );
    step._yield_state = []( Snapshot ) -> Snapshot
    {
        throw brq::error( "could not reach userspace" );
    };
    run( step, false ); /* make 0 (user mode) steps */
    set( "$_", frameDN() );
    update();
}

void CLI::reach_error()
{
    auto step = stepper();
    step._stop_on_accept = true;
    run( step, false );
    set( "$_", frameDN().related( "caller" ) );
    update();
}

void CLI::bplist( command::breakpoint b )
{
    std::deque< decltype( _bps )::iterator > remove;
    int id = 1;
    if ( _bps.empty() )
        out() << "no breakpoints defined" << std::endl;
    for ( auto bp : _bps )
    {
        out() << std::setw( 2 ) << id << ": ";
        bool del_this = !b.del.id.empty() && id == b.del.id;
        bp.match(
            [&]( vm::CodePointer pc )
            {
                auto fun = _bc->debug().function( pc )->getName().str();
                out() << fun << " +" << pc.instruction()
                        << " (at " << dbg::location( location( pc ) ) << ")";
                if ( !b.del.id.empty() && !pc.instruction() && b.del.id == fun )
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

Snapshot CLI::newstate( Snapshot snap, bool update_choices, bool terse )
{
    snap = _explore.start( _ctx, snap );
    _explore.pool().sync();
    _ctx.load( snap );

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
        name = _state_names[ snap ] = "#" + std::to_string( ++_state_count );
        _state_refs[ snap ] = Context::RefCnt( _ctx._refcnt, snap );
        DN state( _ctx, snap );
        state.address( dbg::DNKind::Object, _ctx.state_ptr() );
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

void CLI::sched_policy()
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

bool CLI::update_lock( Snapshot snap )
{
    if ( _trace.count( snap ) )
    {
        _ctx._lock = _trace[ snap ];
        _ctx._lock_mode = Context::LockBoth;
        return true;
    }
    else
    {
        _ctx._lock_mode = Context::LockScheduler;
        return false;
    }
}

bool CLI::check_bp( RefLocation initial, vm::CodePointer pc, bool ch )
{
    if ( ch && initial.second )
        if ( location( pc ) != initial )
            initial = std::make_pair( "", 0 );

    auto match_pc = [&]( vm::CodePointer bp_pc )
    {
        if ( pc != bp_pc )
            return false;
        auto name = _bc->debug().function( pc )->getName();
        out() << "# stopped at breakpoint " << name.str() << std::endl;
        return true;
    };

    auto match_loc = [&]( Location l )
    {
        RefLocation rl = l;
        if ( initial.second == rl.second && rl.first == initial.first )
            return false;
        auto current = location( pc );
        if ( rl.second != current.second )
            return false;
        if ( !brq::ends_with( current.first.str(), l.first ) )
            return false;
        out() << "# stopped at breakpoint " << l.first << ":" << l.second << std::endl;
        return true;
    };

    for ( auto bp : _bps )
        if ( bp.match( match_pc, match_loc ) )
            return true;
    return false;
}

void CLI::trace( Trace tr, bool boot, std::function< void() > end )
{
    std::set< vm::CowHeap::Snapshot > visited;

    auto step = stepper();
    if ( ( step._booting = boot ) )
        vm::setup::boot( _ctx );

    auto old_mode = _ctx._lock_mode;
    _ctx._lock_mode = Context::LockBoth;
    brick::types::Defer _( [&](){ _ctx._lock_mode = old_mode; } );

    auto update_lock = [&]( vm::CowHeap::Snapshot snap )
    {
        ASSERT( !tr.steps.empty() );
        _trace[ snap ] = _ctx._lock = tr.steps.front();
        tr.steps.pop_front();
    };

    auto last = get( "#last", true ).snapshot();
    out() << "traced states:";
    bool stop = false, loop = false;
    step._callback = [&]()
    {
        if ( tr.steps.empty() )
            stop = true;
        return !stop;
    };
    step._stop_on_error = step._stop_on_accept = false;
    step._yield_state =
        [&]( auto snap )
        {
            auto next = newstate( snap, false, true );
            if ( visited.count( next ) )
            {
                if ( !loop )
                    out() << " [loop closed] " << std::flush;
                loop = true;
            }
            visited.insert( next );
            _ctx.instruction_count( 0 );
            update_lock( snap );
            last = get( "#last", true ).snapshot();
            return next;
        };
    run( step, Stepper::Quiet );

    out() << std::endl;

    if ( !tr.steps.empty() )
        out() << "WARNING: Program terminated unexpectedly." << std::endl;

    if ( !_ctx._trace.empty() )
        out() << "trace:" << std::endl;
    for ( auto t : _ctx._trace )
        out() << "T: " << t << std::endl;
    _ctx._trace.clear();

    end();
}

}
