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

#pragma once

#include <divine/vm/explore.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/debug.hpp>
#include <divine/vm/stepper.hpp>
#include <divine/ss/search.hpp>

namespace divine {
namespace mc {

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
    std::cout << "  backtrace 1: # active stack" << std::endl;
    vm::backtrace( dn_top, visited, stacks, maxdepth );
    vm::backtrace( dn, visited, stacks, maxdepth );
}

struct Choices {
    std::vector< std::vector< int > > c;
    auto &back() { return c.back(); };
    void push() { c.emplace_back(); };
};

struct Trace
{
    Choices choices;
    std::vector< std::string > labels;
    vm::CowHeap::Snapshot final;
};

static std::ostream &operator<<( std::ostream &o, const Choices &choices )
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

template< typename Explore >
Trace trace( Explore &ex, std::deque< vm::CowHeap::Snapshot > states )
{
    Trace t;
    auto last = states.begin(), next = last + 1;
    auto hasher = ex._states.hasher; /* fixme */

    auto process =
        [&]( auto &label )
        {
            for ( auto l : label.first )
                t.labels.push_back( l );
            t.choices.push();
            std::transform( label.second.begin(), label.second.end(),
                            std::back_inserter( t.choices.back() ),
                            []( auto x ) { return x.first; } );
        };

    ss::search( ss::Order::PseudoBFS, ex, 1,
                ss::listen(
                    [&]( auto from, auto to, auto label )
                    {
                        ASSERT( t.final || last != states.end() );

                        if ( t.final || !hasher.equal( from.snap, *last ) )
                            return ss::Listen::Ignore;

                        if ( to.error )
                        {
                            t.final = from.snap;
                            process( label );
                            return ss::Listen::Terminate;
                        }

                        if ( next == states.end() || !hasher.equal( to.snap, *next ) )
                            return ss::Listen::Ignore;

                        process( label );
                        ++last, ++next;
                        return ss::Listen::Process;
                    }, []( auto ) { return ss::Listen::Process; } ) );
    ASSERT( t.final );
    return t;
}

}
}
