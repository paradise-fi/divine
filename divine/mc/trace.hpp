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

#include <divine/mc/bitcode.hpp>
#include <divine/mc/types.hpp>
#include <divine/dbg/node.hpp>
#include <divine/dbg/util.hpp>
#include <divine/ss/search.hpp>

#include <optional>

namespace divine::mc
{

using DbgCtx = dbg::Context< vm::CowHeap >;
using DbgNode = dbg::Node< vm::Program, vm::CowHeap >;

struct BadTrace
{
    vm::CowHeap::Snapshot expected;
    std::vector< vm::CowHeap::Snapshot > candidate;
};

static inline DbgNode root( DbgCtx &ctx, vm::CowHeap::Snapshot snap )
{
    DbgNode n( ctx, snap );
    n.address( dbg::DNKind::Object, ctx.state_ptr() );
    n.type( ctx._state_type );
    n.di_type( ctx._state_di_type );
    n._ref.get();
    return n;
}

template< typename BT, typename Fmt, typename Dbg >
void backtrace( BT bt, Fmt fmt, Dbg &dbg, vm::CowSnapshot snap, int maxdepth = 10 )
{
    auto dn = root( dbg, snap );
    DbgNode dn_top( dbg, snap );
    dn_top.address( dbg::DNKind::Frame, dbg.frame() );
    dbg::DNSet visited;
    int stacks = 0;
    dbg::backtrace( bt, fmt, dn_top, visited, stacks, maxdepth );
    dbg::backtrace( bt, fmt, dn, visited, stacks, maxdepth );
}

// LabelComparer: check if the label in the trace is equal to the label of an edge (in this order)
template< typename Explore, typename Label, typename LabelComparer = std::equal_to< Label > >
Trace trace( Explore &ex, LabelledTrace< Label > states, LabelComparer comparer = LabelComparer() )
{
    Trace t;
    auto last = states.begin(), next = last + 1;
    ex.context().enable_debug();
    ex.enable_overwrite();

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
                    [&, has_acc = false]( auto from, auto to, auto label ) mutable
                    {
                        ASSERT( last != states.end() );

                        if ( !ex.equal( from.snap, last->first ) )
                            return ss::Listen::Ignore;

                        if ( next == states.end() )
                            return ss::Listen::Terminate;
                        if ( !ex.equal( to.snap, next->first ) )
                            return ss::Listen::Ignore;
                        if ( next->second.has_value() && !comparer( *next->second, label ) )
                            return ss::Listen::Ignore;

                        if ( label.error )
                        {
                            ASSERT( has_acc || !t.final );
                            t.final = last->first;
                        }

                        if ( label.accepting ) {
                            t.final = last->first;
                            has_acc = true;
                        }

                        process( label );
                        ++last, ++next;
                        return ss::Listen::Process;
                    }, []( auto ) { return ss::Listen::Process; } ) );

    if ( next != states.end() )
    {
        BadTrace error;
        error.expected = next->first;
        typename std::remove_reference_t< decltype ( ex ) >::State state;
        state.snap = last->first;

        ex.edges( state, [&]( auto st, auto, bool isnew )
        {
                error.candidate.push_back( st.snap );
        } );
        throw error;
    }

    if ( !t.final )
        std::cerr << "W: Failed to find an error label. Probably a bad trace." << std::endl;

    return t;
}

}
