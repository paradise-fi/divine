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

#include <divine/vm/explore.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/debug.hpp>
#include <divine/vm/stepper.hpp>
#include <divine/ss/search.hpp>
#include <divine/ui/cli.hpp>

namespace divine {
namespace ui {

using DNSet = std::set< vm::DNKey >;

template< typename DN >
void dump( DN dn, DNSet &visited, int &stacks, int maxdepth )
{
    if ( visited.count( dn.sortkey() ) || dn.address().type() != vm::PointerType::Heap )
        return;
    visited.insert( dn.sortkey() );

    if ( !maxdepth )
        return;

    if ( dn.kind() == vm::DNKind::Frame )
    {
        dn.attributes( []( std::string k, std::string v )
                       {
                           if ( k == "@pc" || k == "@address" || k == "@location" || k == "@symbol" )
                               std::cout << "  " << k << ": " << v << std::endl;
                       } );
        std::cout << std::endl;
    }

    dn.related( [&]( std::string k, auto rel )
                {
                    if ( rel.kind() == vm::DNKind::Frame && k != "@caller" &&
                         !visited.count( rel.sortkey() ) && maxdepth > 1 )
                        std::cerr << "backtrace #" << ++stacks << ":" << std::endl;
                    dump( rel, visited, stacks, k == "@caller" ? maxdepth - 1 : maxdepth );
                } );
}

template< typename Dbg >
void dump( Dbg &dbg, vm::CowHeap::Snapshot snap, int maxdepth = 10 )
{
    vm::DebugNode< vm::Program, vm::CowHeap > dn( dbg, snap ), dn_top( dbg, snap );
    dn.address( vm::DNKind::Object, dbg.get( _VM_CR_State ).pointer );
    dn.type( dbg._state_type );
    dn.di_type( dbg._state_di_type );
    dn_top.address( vm::DNKind::Frame, dbg.get( _VM_CR_Frame ).pointer );
    DNSet visited;
    int stacks = 1;
    std::cerr << "backtrace #1 [active thread]:" << std::endl;
    dump( dn_top, visited, stacks, maxdepth );
    dump( dn, visited, stacks, maxdepth );
}

void Verify::run()
{
    vm::Explore ex( _bc );
    vm::explore::State error;
    bool error_found = false;

    if ( !_threads )
        _threads = std::min( 4u, std::thread::hardware_concurrency() );

    std::atomic< int > edgecount( 0 ), statecount( 0 );
    std::atomic< bool > done( false );

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

    auto progress = std::thread(
        [&]()
        {
            while ( !done )
            {
                interval = std::chrono::duration_cast< msecs >( clock::now() - start );
                std::this_thread::sleep_for( msecs( 500 ) );
                std::cerr << "\rsearching: " << statecount << " states and "
                          << edgecount << " edges found in " << time()
                          << ", averaging " << avg() << "    ";
            }
        } );

    typename vm::CowHeap::SnapPool ext;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;

    ex.start();
    ss::search(
        ss::Order::PseudoBFS, ex, _threads,
        ss::listen(
            [&]( auto from, auto to, auto, bool isnew )
            {
                if ( isnew )
                {
                    ext.materialise( to.snap, sizeof( from ), ex.pool() );
                    Parent &parent = *ext.machinePointer< Parent >( to.snap );
                    parent = from.snap;
                }
                ++edgecount;
                if ( to.error )
                {
                    error_found = true;
                    error = to;
                    return ss::Listen::Terminate;
                }
                return ss::Listen::AsNeeded;
            },
            [&]( auto )
            {
                ++statecount; return ss::Listen::AsNeeded;
            } ) );

    done = true;
    progress.join();
    std::cerr << "\rfound " << statecount << " states and "
              << edgecount << " edges" << " in " << time() << ", averaging " << avg()
              << "             " << std::endl;

    if ( !error_found )
    {
        std::cout << "no errors found" << std::endl;
        return;
    }

    auto hasher = ex._states.hasher; /* fixme */

    vm::DebugContext< vm::Program, vm::CowHeap > dbg( _bc->program() );
    vm::setup::dbg_boot( dbg );

    std::deque< vm::CowHeap::Snapshot > trace;
    std::vector< std::vector< int > > choices;
    std::vector< std::string > labels;

    std::cout << "found an error" << std::endl;

    auto i = error.snap;
    while ( i != ex._initial.snap )
    {
        trace.push_front( i );
        i = *ext.machinePointer< vm::CowHeap::Snapshot >( i );
    }
    trace.push_front( ex._initial.snap );

    auto last = trace.begin(), next = last;
    next ++;
    ss::search( ss::Order::PseudoBFS, ex, 1,
                ss::listen(
                    [&]( auto from, auto to, auto label )
                    {
                        if ( next != trace.end() &&
                             hasher.equal( from.snap, *last ) &&
                             hasher.equal( to.snap, *next ) )
                        {
                            for ( auto l : label.first )
                                labels.push_back( l );
                            choices.emplace_back();
                            std::transform( label.second.begin(), label.second.end(),
                                            std::back_inserter( choices.back() ),
                                            []( auto x ) { return x.first; } );
                            ++last, ++next;
                            if ( next == trace.end() )
                                return ss::Listen::Terminate;
                            return ss::Listen::Process;
                        }
                        return ss::Listen::Ignore;
                    }, []( auto ) { return ss::Listen::Process; } ) );

    std::cout << std::endl << "choices made:";
    for ( auto &v : choices )
        for ( int c : v )
            std::cout << " " << c;
    std::cout << std::endl;
    ASSERT( next == trace.end() );

    std::cout << std::endl << "the error trace:" << std::endl;
    for ( std::string l : labels )
        std::cout << "  " << l << std::endl;
    std::cout << std::endl;

    auto &ctx = ex._ctx;
    ASSERT_LEQ( 2, trace.size() );
    ctx.heap().restore( *( trace.end() - 2 ) );
    dbg.load( ctx );
    std::copy( choices.back().begin(), choices.back().end(), std::back_inserter( dbg._choices ) );
    vm::setup::scheduler( dbg );
    using Stepper = vm::Stepper< decltype( dbg ) >;
    Stepper step;
    step._stop_on_error = true;
    step.run( dbg, []( auto x ) { return x; }, []() {}, Stepper::Quiet );
    dump( dbg, dbg.snapshot(), _backtraceMaxDepth );
}

}
}
