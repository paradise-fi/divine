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

#include <divine/mc/safety.hpp>
#include <divine/mc/trace.hpp>
#include <divine/vm/stepper.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/sysinfo.hpp>

namespace divine {
namespace ui {

template< typename S >
std::string printitem( S s )
{
    std::stringstream str;
    str << "{ items: " << s.count.used << ", used: " << s.bytes.used << ", held: " << s.bytes.held << " }";
    return str.str();
}

void printpool( std::string name, const brick::mem::Stats &s )
{
    std::cout << name << ":" << std::endl;
    std::cout << "  total: " << printitem( s.total ) << std::endl;
    for ( auto i : s )
        if ( i.count.held )
            std::cout << "  " << i.size << ": " << printitem( i ) << std::endl;
}

void Verify::run()
{
    vm::explore::State error;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    using clock = std::chrono::steady_clock;
    using msecs = std::chrono::milliseconds;
    clock::time_point start;
    msecs interval;

    auto time =
        [&]()
        {
            std::stringstream t;
            t << int( interval.count() / 60000 ) << ":"
              << std::setw( 2 ) << std::setfill( '0' ) << int( interval.count() / 1000 ) % 60;
            return t.str();
        };

    auto safety = mc::make_safety( bitcode(), ss::passive_listen(), _symbolic, true );

    auto avg = [&]() { return 1000 * float( safety->statecount() ) / interval.count(); };
    auto fmt_avg =
        [&]()
        {
            std::stringstream s;
            s << std::fixed << std::setprecision( 1 ) << avg() << " states/s";
            return s.str();
        };

    auto update_interval =
        [&]() { interval = std::chrono::duration_cast< msecs >( clock::now() - start ); };

    start = clock::now();
    SysInfo sysinfo;
    sysinfo.setMemoryLimitInBytes( _max_mem );
    safety->start( _threads, [&]()
                   {
                       update_interval();
                       std::cerr << "\rsearching: " << safety->statecount()
                                 << " states found in " << time()
                                 << ", averaging " << fmt_avg()
                                 << ", queued: " << safety->queuesize() << "      ";
                       sysinfo.updateAndCheckTimeLimit( _max_time );
                   } );
    safety->wait();

    update_interval();
    std::cerr << "\rfound " << safety->statecount()
              << " states in " << time() << ", averaging " << fmt_avg()
              << "                             " << std::endl << std::endl;

    vm::DebugContext< vm::Program, vm::CowHeap > dbg( bitcode()->program() );
    vm::setup::dbg_boot( dbg );

    brick::types::Defer stats( [&] {
            if ( _report != Report::None )
            {
                sysinfo.report( []( auto k, auto v ) {
                        std::cout << k << ": " << v << std::endl;
                    } );
                std::cout << "search time: " << std::setprecision( 3 )
                          << double( interval.count() ) / 1000
                          << std::endl << "state count: " << safety->statecount()
                          << std::endl << "states per second: " << avg()
                          << std::endl << "version: " << version()
                          << std::endl;
            }
            if ( _report == Report::YamlLong )
                for ( auto ps : safety->poolstats() )
                    printpool( ps.first, ps.second );
    } );

    if ( !safety->statecount() )
    {
        std::cout << "error found: boot" << std::endl;
        std::cout << "boot info:" << std::endl;
        std::cout << dbg._info << std::endl;
        std::cout << "boot trace: |" << std::endl;
        for ( auto s : dbg._trace )
            std::cout << "  " << s << std::endl;
        return;
    }

    if ( !safety->error_found() )
    {
        std::cout << "error found: no" << std::endl;
        return;
    }

    auto trace = safety->ce_trace();

    std::cout << "error found: yes" << std::endl;
    std::cout << "choices made:" << trace.choices << std::endl;
    std::cout << "error trace: |" << std::endl;
    for ( std::string l : trace.labels )
        std::cout << "  " << l << std::endl;
    std::cout << std::endl;

    std::cout << "error state:" << std::endl;
    safety->dbg_fill( dbg );
    dbg.load( trace.final );
    dbg._choices = { trace.choices.back().begin(), trace.choices.back().end() };
    dbg._choices.push_back( -1 ); // prevent execution after choices are depleted
    vm::setup::scheduler( dbg );
    using Stepper = vm::Stepper< decltype( dbg ) >;
    Stepper step;
    step._stop_on_error = true;
    step.run( dbg, Stepper::Quiet );
    mc::backtrace( dbg, dbg.snapshot(), _num_callers );
}

}
}
