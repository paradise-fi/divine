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
#include <divine/mc/job.tpp>
#include <divine/mc/trace.hpp>
#include <divine/dbg/stepper.hpp>
#include <divine/dbg/setup.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/sysinfo.hpp>

namespace divine {
namespace ui {

void Verify::setup_report_file()
{
    if ( _report_filename.empty() )
        _report_filename = outputName( _file, "report" );

    if ( !_report_unique )
        return;

    std::random_device rd;
    std::uniform_int_distribution< char > dist( 'a', 'z' );
    std::string fn;
    int fd;

    do {
        fn = _report_filename + ".";
        for ( int i = 0; i < 6; ++i )
            fn += dist( rd );
        fd = open( fn.c_str(), O_CREAT | O_EXCL, 0666 );
    } while ( fd < 0 );

    _report_filename = fn;
    close( fd );
}

void Verify::setup()
{
    if ( _log == nullsink() )
    {
        std::vector< SinkPtr > log;

        if ( _interactive )
            log.push_back( make_interactive() );

        if ( _report != Report::None )
            log.push_back( make_yaml( std::cout, _report == Report::YamlLong ) );

        if ( !_no_report_file )
        {
            setup_report_file();
            _report_file.reset( new std::ofstream( _report_filename ) );
            log.push_back( make_yaml( *_report_file.get(), true ) );
        }

        _log = make_composite( log );
    }

    WithBC::setup();

    if ( _symbolic )
        bitcode()->solver( _solver );
}

void Check::setup()
{
    _systemopts.push_back( "nofail:malloc" );
    Verify::setup();
}

void Verify::run()
{
    if ( _liveness )
        liveness();
    else
        safety();
}

void Verify::print_ce( mc::Job &job )
{
    dbg::Context< vm::CowHeap > dbg( bitcode()->program(), bitcode()->debug() );

    _log->info( "\n" ); /* makes the output prettier */

    auto trace = job.ce_trace();

    if ( job.result() == mc::Result::BootError )
    {
        dbg::setup::boot( dbg );
        trace.bootinfo = dbg._info, trace.labels = dbg._trace;
    }

    _log->result( job.result(), trace );

    if ( job.result() == mc::Result::Error )
    {
        job.dbg_fill( dbg );
        dbg.load( trace.final );
        dbg._lock = trace.steps.back();
        dbg._lock_mode = decltype( dbg )::LockBoth;
        vm::setup::scheduler( dbg );
        using Stepper = dbg::Stepper< decltype( dbg ) >;
        Stepper step;
        step._stop_on_error = true;
        step._ff_components = dbg::Component::Kernel;
        step.run( dbg, Stepper::Quiet );
        _log->backtrace( dbg, _num_callers );
    }
}

void Verify::safety()
{
    mc::builder::State error;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    auto safety = mc::make_job< mc::Safety >( bitcode(), ss::passive_listen() );

    SysInfo sysinfo;
    sysinfo.setMemoryLimitInBytes( _max_mem );

    _log->start();
    int ps_ctr = 0;

    safety->start( _threads, [&]( bool last )
                   {
                       _log->progress( safety->stats(),
                                       safety->queuesize(), last );
                       if ( last || ( ++ps_ctr == 2 * _poolstat_period ) )
                           ps_ctr = 0, _log->memory( safety->poolstats(), last );
                       if ( !last )
                           sysinfo.updateAndCheckTimeLimit( _max_time );
                   } );
    safety->wait();
    report_options();
    _log->info( "smt solver: " + _solver + "\n", true );

    if ( safety->result() == mc::Result::Valid )
        return _log->result( safety->result(), mc::Trace() );

    print_ce( *safety );
}

void Verify::cleanup()
{
    if ( !_report_filename.empty() )
        std::cerr << "a report was written to " << _report_filename << std::endl;
}

void Verify::liveness()
{
    auto liveness = mc::make_job< mc::Liveness >( bitcode(), ss::passive_listen() );

    _log->start();
    liveness->start( 1 ); // threadcount
    liveness->wait();

    report_options();

    print_ce( *liveness );
}

}
}
