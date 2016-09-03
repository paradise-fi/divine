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

void Verify::run()
{
    vm::Explore ex( _bc );
    vm::explore::State error;
    bool error_found = false;

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

    typename vm::CowHeap::Pool ext;
    using Parent = std::atomic< vm::explore::State >;

    ex.start();
    ss::search(
        ss::Order::PseudoBFS, ex, 1,
        ss::listen(
            [&]( auto from, auto to, auto label )
            {
                ext.materialise( to.snap, sizeof( from ), ex.pool() );
                Parent &parent = *ext.machinePointer< Parent >( to.snap );
                if ( !parent.load().snap.slab() )
                    parent = from;
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
                ext.materialise( st.snap, sizeof( st ), ex.pool() );
                ++statecount; return ss::Listen::AsNeeded;
            } ) );

    done = true;
    progress.join();
    std::cerr << std::endl << "found " << statecount << " states and "
              << edgecount << " edges" << std::endl;
    auto hasher = ex._states->hasher; /* fixme */

    if ( error_found )
    {
        std::deque< vm::explore::State > trace;
        std::cerr << "found an error" << std::endl;
        while ( error.snap.slab() )
        {
            trace.push_front( error );
            error = *ext.machinePointer< Parent >( error.snap );
        }
        auto last = trace.begin(), next = last;
        next ++;
        ss::search( ss::Order::PseudoBFS, ex, 1,
                    ss::listen(
                        [&]( auto from, auto to, auto label )
                        {
                            if ( hasher.equal( from.snap, last->snap ) &&
                                 hasher.equal( to.snap, next->snap ) )
                            {
                                for ( auto l : label )
                                    std::cerr << l << std::endl;
                                ++last, ++next;
                                return ss::Listen::Process;
                            }
                            return ss::Listen::Ignore;
                        }, []( auto ) { return ss::Listen::Process; } ) );
    }

}

}
}
