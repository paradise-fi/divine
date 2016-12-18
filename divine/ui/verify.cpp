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

namespace divine {
namespace ui {

void Verify::run()
{
    vm::explore::State error;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    std::atomic< int > edgecount( 0 ), statecount( 0 );

    using clock = std::chrono::steady_clock;
    using msecs = std::chrono::milliseconds;
    clock::time_point start = clock::now();
    msecs interval;

    auto time =
        [&]()
        {
            std::stringstream t;
            t << int( interval.count() / 60000 ) << ":"
              << std::setw( 2 ) << std::setfill( '0' ) << int(interval.count() / 1000) % 60;
            return t.str();
        };

    auto avg =
        [&]()
        {
            std::stringstream s;
            auto v = 1000 * float( statecount ) / interval.count();
            s << std::fixed << std::setprecision( 1 ) << v << " states/s";
            return s.str();
        };

    auto update_interval =
        [&]() { interval = std::chrono::duration_cast< msecs >( clock::now() - start ); };

    auto safety = mc::make_safety( bitcode(), ss::passive_listen() );
    safety.start( _threads, [&]( auto &search )
                  {
                      statecount = safety._ex._states._s->used;
                      search.ws_each( [&]( auto &bld, auto & )
                                      { statecount += bld._states._l.inserts; } );
                      update_interval();
                      std::cerr << "\rsearching: " << statecount << " states and "
                                << edgecount << " edges found in " << time()
                                << ", averaging " << avg() << "    ";
                  } );
    safety.wait();

    update_interval();
    std::cerr << "\rfound " << statecount << " states and "
              << edgecount << " edges" << " in " << time() << ", averaging " << avg()
              << "             " << std::endl;

    vm::DebugContext< vm::Program, vm::CowHeap > dbg( bitcode()->program() );
    vm::setup::dbg_boot( dbg );

    if ( !statecount )
    {
        std::cout << "no states produced, boot trace:" << std::endl;
        std::cout << dbg._info << std::endl;
        for ( auto s : dbg._trace )
            std::cerr << s << std::endl;
        return;
    }

    if ( !safety._error_found )
    {
        std::cout << "no errors found" << std::endl;
        return;
    }

    auto ce_states = safety.ce_states();
    auto trace = mc::trace( safety._ex, ce_states );

    std::cout << "found an error" << std::endl;

    std::cout << std::endl << "choices made:" << trace.choices << std::endl;
    std::cout << std::endl << "the error trace:" << std::endl;
    for ( std::string l : trace.labels )
        std::cout << "  " << l << std::endl;
    std::cout << std::endl;

    auto &ctx = safety._ex._ctx;
    ctx.heap().restore( trace.final );
    dbg.load( ctx );
    dbg._choices = { trace.choices.c.back().begin(), trace.choices.c.back().end() };
    dbg._choices.push_back( -1 ); // prevent execution after choices are depleted
    vm::setup::scheduler( dbg );
    using Stepper = vm::Stepper< decltype( dbg ) >;
    Stepper step;
    step._stop_on_error = true;
    step.run( dbg, Stepper::Quiet );
    mc::backtrace( dbg, dbg.snapshot(), _backtraceMaxDepth );
}

}
}
