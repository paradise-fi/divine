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

template< typename Dbg >
void backtrace( Dbg &dbg, vm::CowHeap::Snapshot snap, int maxdepth = 10 )
{
    vm::DebugNode< vm::Program, vm::CowHeap > dn( dbg, snap ), dn_top( dbg, snap );
    dn.address( vm::DNKind::Object, dbg.get( _VM_CR_State ).pointer );
    dn.type( dbg._state_type );
    dn.di_type( dbg._state_di_type );
    dn_top.address( vm::DNKind::Frame, dbg.get( _VM_CR_Frame ).pointer );
    vm::DNSet visited;
    int stacks = 1;
    std::cerr << "backtrace #1 [active stack]:" << std::endl;
    vm::backtrace( dn_top, visited, stacks, maxdepth );
    vm::backtrace( dn, visited, stacks, maxdepth );
}

struct Choices { std::vector< std::vector< int > > c; };

std::ostream &operator<<( std::ostream &o, const Choices &choices )
{
    int last = -1, multiply = 1;
    for ( auto &v : choices.c )
        for ( int c : v )
        {
            if ( c == last )
                multiply ++;
            else
            {
                if ( multiply > 1 )
                    o << "^" << multiply;
                multiply = 1, last = c;
                o << " " << c;
            }
        }
    if ( multiply > 1 )
        o << "^" << multiply;
    return o;
}

void Verify::run()
{
    vm::Explore ex( bitcode() );
    vm::explore::State error;
    bool error_found = false;

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

    std::cerr << "booting... " << std::flush;
    ex.start();
    std::cerr << "done" << std::endl;

    auto progress = brick::shmem::async_loop(
        [&]()
        {
            update_interval();
            std::this_thread::sleep_for( msecs( 500 ) );
            std::cerr << "\rsearching: " << statecount << " states and "
                      << edgecount << " edges found in " << time()
                      << ", averaging " << avg() << "    ";
        } );

    typename vm::CowHeap::SnapPool ext;
    using Parent = std::atomic< vm::CowHeap::Snapshot >;

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

    progress.stop();
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

    if ( !error_found )
    {
        std::cout << "no errors found" << std::endl;
        return;
    }

    auto hasher = ex._states.hasher; /* fixme */

    std::deque< vm::CowHeap::Snapshot > trace;
    Choices choices;
    std::vector< std::string > labels;

    std::cout << "found an error" << std::endl;

    auto i = error.snap;
    while ( i != ex._initial.snap )
    {
        trace.push_front( i );
        i = *ext.machinePointer< vm::CowHeap::Snapshot >( i );
    }
    trace.push_front( ex._initial.snap );

    bool checkSelfloop = false;
    int lastBeforeErrorOffset = 1;
    auto last = trace.begin(), next = last;
    next ++;
    auto pushLabel = [&]( auto &label ) {
        for ( auto l : label.first )
            labels.push_back( l );
        choices.c.emplace_back();
        std::transform( label.second.begin(), label.second.end(),
                        std::back_inserter( choices.c.back() ),
                                    []( auto x ) { return x.first; } );
    };
    ss::search( ss::Order::PseudoBFS, ex, 1,
                ss::listen(
                    [&]( auto from, auto to, auto label )
                    {
                        if ( next != trace.end() &&
                             hasher.equal( from.snap, *last ) &&
                             hasher.equal( to.snap, *next ) )
                        {
                            pushLabel( label );
                            ++last, ++next;
                            if ( next == trace.end() ) {
                                checkSelfloop = !to.error;
                                return ss::Listen::Terminate;
                            }
                            return ss::Listen::Process;
                        }
                        else if ( next == trace.end() &&
                                  hasher.equal( from.snap, *last) &&
                                  hasher.equal( to.snap, *last ) )
                        {
                            pushLabel( label );
                            lastBeforeErrorOffset = 0;
                            return ss::Listen::Terminate;
                        }
                        return ss::Listen::Ignore;
                    }, []( auto ) { return ss::Listen::Process; } ) );

    if ( checkSelfloop ) {
        vm::explore::State from;
        from.snap = trace.back();
        ex.edges( from, [&]( auto to, auto label, auto ) {
                if ( checkSelfloop && to.error && hasher.equal( to.snap, from.snap ) ) {
                    pushLabel( label );
                    checkSelfloop = false;
                    lastBeforeErrorOffset = 0;
                }
            } );
    }

    std::cout << std::endl << "choices made:" << choices << std::endl;
    std::cout << std::endl << "the error trace:" << std::endl;
    for ( std::string l : labels )
        std::cout << "  " << l << std::endl;
    std::cout << std::endl;

    ASSERT( !checkSelfloop && next == trace.end() );

    auto &ctx = ex._ctx;
    ASSERT_LT( lastBeforeErrorOffset, trace.size() );
    ctx.heap().restore( *(trace.rbegin() + lastBeforeErrorOffset) );
    dbg.load( ctx );
    dbg._choices = { choices.c.back().begin(), choices.c.back().end() };
    dbg._choices.push_back( -1 ); // prevent running after choices are depletet
    vm::setup::scheduler( dbg );
    using Stepper = vm::Stepper< decltype( dbg ) >;
    Stepper step;
    step._stop_on_error = true;
    step.run( dbg, Stepper::Quiet );
    backtrace( dbg, dbg.snapshot(), _backtraceMaxDepth );
}

}
}
