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
#include <divine/vm/dbg-node.hpp>
#include <divine/vm/dbg-util.hpp>
#include <divine/ss/search.hpp>

#include <optional>

namespace divine {
namespace mc {

template< typename Dbg >
void backtrace( std::ostream &ostr, Dbg &dbg, vm::CowHeap::Snapshot snap, int maxdepth = 10 )
{
    vm::dbg::Node< vm::Program, vm::CowHeap > dn( dbg, snap ), dn_top( dbg, snap );
    dn.address( vm::dbg::DNKind::Object, dbg.get( _VM_CR_State ).pointer );
    dn.type( dbg._state_type );
    dn.di_type( dbg._state_di_type );
    dn_top.address( vm::dbg::DNKind::Frame, dbg.get( _VM_CR_Frame ).pointer );
    vm::dbg::DNSet visited;
    int stacks = 1;
    ostr << "  backtrace 1: # active stack" << std::endl;
    vm::dbg::backtrace( ostr, dn_top, visited, stacks, maxdepth );
    vm::dbg::backtrace( ostr, dn, visited, stacks, maxdepth );
}

struct Trace
{
    std::vector< vm::Step > steps;
    std::vector< std::string > labels;
    std::string bootinfo;
    vm::CowHeap::Snapshot final;
};

template< typename Ex >
using StateTrace = std::deque< std::pair< vm::CowHeap::Snapshot,
                                          std::optional< typename Ex::Label > > >;

template< typename Explore >
Trace trace( Explore &ex, StateTrace< Explore > states )
{
    Trace t;
    auto last = states.begin(), next = last + 1;
    auto hasher = ex._states.hasher; /* fixme */
    ex._ctx.enable_debug();
    ex._overwrite = true;

    auto process =
        [&]( auto &label )
        {
            for ( auto l : label.trace )
                t.labels.push_back( l );
            t.steps.emplace_back();
            std::copy( label.interrupts.begin(), label.interrupts.end(),
                       std::back_inserter( t.steps.back().interrupts ) );
            std::copy( label.stack.begin(), label.stack.end(),
                       std::back_inserter( t.steps.back().choices ) );
        };

    ss::search( ss::Order::PseudoBFS, ex, 1,
                ss::listen(
                    [&]( auto from, auto to, auto label )
                    {
                        ASSERT( last != states.end() );

                        if ( !hasher.equal( from.snap, last->first ) )
                            return ss::Listen::Ignore;

                        if ( next == states.end() )
                            return ss::Listen::Terminate;
                        if ( !hasher.equal( to.snap, next->first ) )
                            return ss::Listen::Ignore;
                        if ( next->second.has_value() && label != next->second.value() )
                            return ss::Listen::Ignore;

                        process( label );
                        ++last, ++next;
                        return ss::Listen::Process;
                    }, []( auto ) { return ss::Listen::Process; } ) );

    ASSERT( next == states.end() );
    typename Explore::State origin;
    origin.snap = *last;
    ex.edges( origin, [&]( auto, auto label, bool )
              {
                  if ( label.error && !t.final )
                  {
                      t.final = *last;
                      process( label );
                  }
              } );

    ASSERT( t.final );
    return t;
}

}
}
