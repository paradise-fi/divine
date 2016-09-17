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
#include <divine/ss/search.hpp>
#include <divine/ui/cli.hpp>

namespace divine {
namespace ui {

using DNSet = std::set< std::tuple< vm::GenericPointer, int, llvm::DIType * > >;

template< typename DN >
void dump( DN dn, DNSet &visited, int &stacks )
{
    if ( visited.count( dn.sortkey() ) || dn.address().type() != vm::PointerType::Heap )
        return;
    visited.insert( dn.sortkey() );

    if ( dn.kind() == vm::DNKind::Frame )
    {
        dn.attributes( []( std::string k, std::string v )
                       { if ( k == "@address" || k == "@location" || k == "@symbol" )
                             std::cout << k << ": " << v << std::endl;
                       } );
        std::cout << std::endl;
    }

    dn.related( [&]( std::string k, auto rel )
                {
                    if ( rel.kind() == vm::DNKind::Frame && k != "@parent" &&
                         !visited.count( rel.sortkey() ) )
                    {
                        if ( stacks )
                            std::cerr << "--------------------" << std::endl << std::endl;
                        ++ stacks;
                    }
                    dump( rel, visited, stacks );
                } );
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

    auto progress = std::thread(
        [&]()
        {
            while ( !done )
            {
                std::this_thread::sleep_for( msecs( 500 ) );
                std::stringstream time;
                auto interval = std::chrono::duration_cast< msecs >( clock::now() - start );
                float avg = 1000 * float( statecount ) / interval.count();
                time << int( interval.count() / 60000 ) << ":"
                     << std::setw( 2 ) << std::setfill( '0' ) << int(interval.count() / 1000) % 60;
                std::cerr << "\rsearching: " << edgecount << " edges and "
                          << statecount << " states found in " << time.str() << ", averaging "
                          << std::fixed << std::setprecision( 1 ) << avg << " states/s    ";
            }
        } );

    typename vm::CowHeap::SnapPool ext;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;

    ex.start();
    ss::search(
        ss::Order::PseudoBFS, ex, _threads,
        ss::listen(
            [&]( auto from, auto to, auto label, bool isnew )
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
            [&]( auto st )
            {
                ++statecount; return ss::Listen::AsNeeded;
            } ) );

    done = true;
    progress.join();
    std::cout << std::endl << "found " << statecount << " states and "
              << edgecount << " edges" << std::endl;

    if ( !error_found )
    {
        std::cout << "no errors found" << std::endl;
        return;
    }

    auto hasher = ex._states.hasher; /* fixme */

    vm::DebugContext< vm::Program, vm::CowHeap > dbg( _bc->program() );
    vm::setup::boot( dbg );
    vm::Eval< vm::Program, decltype( dbg ), vm::value::Void > dbg_eval( dbg.program(), dbg );
    dbg_eval.run();

    std::deque< vm::CowHeap::Snapshot > trace;
    std::vector< int > choices;
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
                        if ( hasher.equal( from.snap, *last ) &&
                             hasher.equal( to.snap, *next ) )
                        {
                            for ( auto l : label.first )
                                std::cerr << l << std::endl;
                            std::transform( label.second.begin(), label.second.end(),
                                            std::back_inserter( choices ),
                                            []( auto x ) { return x.first; } );
                            ++last, ++next;
                            if ( next == trace.end() )
                                return ss::Listen::Terminate;
                            return ss::Listen::Process;
                        }
                        return ss::Listen::Ignore;
                    }, []( auto ) { return ss::Listen::Process; } ) );

    std::cout << "choices made:";
    for ( int c : choices )
        std::cout << " " << c;
    std::cout << std::endl;

    vm::DebugNode< vm::Program, vm::CowHeap > dn(
            ex._ctx, trace.back(), ex._ctx.get( _VM_CR_State ).pointer, 0,
            vm::DNKind::Object, dbg._state_type, dbg._state_di_type );
    DNSet visited;
    int stacks = 0;
    dump( dn, visited, stacks );
}

}
}
