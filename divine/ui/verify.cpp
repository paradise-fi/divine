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

void Verify::run()
{
    vm::explore::State error;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    using clock = std::chrono::steady_clock;
    using msecs = std::chrono::milliseconds;
    clock::time_point start;
    msecs interval;
    std::atomic< int > statecount( 0 );

    auto time =
        [&]()
        {
            std::stringstream t;
            t << int( interval.count() / 60000 ) << ":"
              << std::setw( 2 ) << std::setfill( '0' ) << int( interval.count() / 1000 ) % 60;
            return t.str();
        };

    auto avg = [&]() { return 1000 * float( statecount ) / interval.count(); };
    auto fmt_avg =
        [&]()
        {
            std::stringstream s;
            s << std::fixed << std::setprecision( 1 ) << avg() << " states/s";
            return s.str();
        };

    auto update_interval =
        [&]() { interval = std::chrono::duration_cast< msecs >( clock::now() - start ); };

    auto safety = mc::make_safety( bitcode(), ss::passive_listen(), true );
    start = clock::now();
    SysInfo sysinfo;
    sysinfo.setMemoryLimitInBytes( _max_mem );
    safety.start( _threads, [&]( auto &search )
                  {
                      statecount = safety._ex._states._s->used;
                      search.ws_each( [&]( auto &bld, auto & )
                                      { statecount += bld._states._l.inserts; } );
                      update_interval();
                      std::cerr << "\rsearching: " << statecount << " states found in " << time()
                                << ", averaging " << fmt_avg() << ", queued: " << safety._search->qsize() << "      ";
                      sysinfo.updateAndCheckTimeLimit( _max_time );
                  } );
    safety.wait();

    statecount = safety._ex._states._s->used;
    update_interval();
    std::cerr << "\rfound " << statecount << " states in " << time() << ", averaging " << fmt_avg()
              << "                             " << std::endl << std::endl;

    vm::DebugContext< vm::Program, vm::CowHeap > dbg( bitcode()->program() );
    vm::setup::dbg_boot( dbg );


    brick::types::Defer stats( [&] {
            if ( _report ) {
                sysinfo.report( []( auto k, auto v ) {
                        std::cout << k << ": " << v << std::endl;
                    } );
                std::cout << "search time: " << std::setprecision( 3 )
                          << double( interval.count() ) / 1000
                          << std::endl << "state count: " << statecount
                          << std::endl << "states per second: " << avg()
                          << std::endl << "version: " << version()
                          << std::endl;
            }
    } );

    if ( !statecount )
    {
        std::cout << "error found: boot" << std::endl;
        std::cout << "boot info:" << std::endl;
        std::cout << dbg._info << std::endl;
        std::cout << "boot trace: |" << std::endl;
        for ( auto s : dbg._trace )
            std::cout << "  " << s << std::endl;
        return;
    }

    if ( !safety._error_found )
    {
        std::cout << "error found: no" << std::endl;
        return;
    }

    auto ce_states = safety.ce_states();
    auto trace = mc::trace( safety._ex, ce_states );

    std::cout << "error found: yes" << std::endl;
    std::cout << "choices made:" << trace.choices << std::endl;
    std::cout << "error trace: |" << std::endl;
    for ( std::string l : trace.labels )
        std::cout << "  " << l << std::endl;
    std::cout << std::endl;

    std::cout << "error state:" << std::endl;
    dbg.load( safety._ex._ctx );
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
