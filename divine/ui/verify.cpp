// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/liveness.hpp>
#include <divine/mc/safety.hpp>
#include <divine/mc/trace.hpp>
#include <divine/vm/dbg-stepper.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/sysinfo.hpp>

namespace divine {
namespace ui {

void Verify::setup()
{
    if ( _log == nullsink() )
    {
        std::vector< SinkPtr > log;

        if ( _interactive )
            log.push_back( make_interactive() );

        if ( _report != Report::None )
            log.push_back( make_yaml( _report == Report::YamlLong ) );

        _log = make_composite( log );
    }

    WithBC::setup();
}

void Verify::run()
{
    if ( _liveness )
        liveness();
    else
        safety();
}

void Verify::safety()
{
    vm::explore::State error;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    auto safety = mc::make_safety( bitcode(), ss::passive_listen(), _symbolic );

    SysInfo sysinfo;
    sysinfo.setMemoryLimitInBytes( _max_mem );

    _log->start();

    safety->start( _threads, [&]( bool last )
                   {
                       _log->progress( safety->statecount(),
                                       safety->queuesize(), last );
                       _log->memory( safety->poolstats(), last );
                       sysinfo.updateAndCheckTimeLimit( _max_time );
                   } );
    safety->wait();

    if ( safety->result() == mc::Result::Valid )
        return _log->result( safety->result(), mc::Trace() );

    vm::dbg::Context< vm::CowHeap > dbg( bitcode()->program(), bitcode()->debug() );
    vm::setup::dbg_boot( dbg );

    std::cout << std::endl;

    auto trace = safety->ce_trace();

    if ( safety->result() == mc::Result::BootError )
        trace.bootinfo = dbg._info, trace.labels = dbg._trace;

    _log->result( safety->result(), trace );

    if ( safety->result() != mc::Result::Error )
        return;

    std::cout << "error state:" << std::endl;
    safety->dbg_fill( dbg );
    dbg.load( trace.final );
    dbg._choices = { trace.choices.back().begin(), trace.choices.back().end() };
    dbg._choices.push_back( -1 ); // prevent execution after choices are depleted
    vm::setup::scheduler( dbg );
    using Stepper = vm::dbg::Stepper< decltype( dbg ) >;
    Stepper step;
    step._stop_on_error = true;
    step.run( dbg, Stepper::Quiet );
    mc::backtrace( dbg, dbg.snapshot(), _num_callers );
}

void Verify::liveness()
{
    auto liveness = mc::make_liveness( bitcode(), ss::passive_listen(), _symbolic );

    liveness->start( 1 ); // threadcount
    liveness->wait();
    _log->result( liveness->result(), mc::Trace() );
}

}
}
