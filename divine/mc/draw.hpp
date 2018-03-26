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

#include <divine/mc/builder.hpp>
#include <divine/vm/heap.hpp>
#include <divine/dbg/dot.hpp>
#include <brick-proc>

namespace divine {
namespace mc {

namespace {

template< typename Builder >
std::string draw_impl( Builder &bld, std::shared_ptr< BitCode > bc, int distance, bool heap )
{
    dbg::Context< vm::mem::CowHeap > dbg( bc->program(), bc->debug() );
    dbg.load( bld.context() );
    vm::setup::boot( dbg );
    vm::Eval< decltype( dbg ) > dbg_eval( dbg );
    dbg_eval.run();

    struct ext_data { int seq; int distance; };
    brick::mem::SlavePool< typename vm::mem::CowHeap::SnapPool > ext_pool( bld.pool() );
    int seq = 0;

    auto ext = [&]( auto st ) -> auto& { return *ext_pool.machinePointer< ext_data >( st.snap ); };

    auto init =
        [&]( auto st )
        {
            ext_pool.materialise( st.snap, sizeof( ext_data ), false );
            if ( ext( st ).seq )
                return false;
            ext( st ).seq = ++ seq;
            ext( st ).distance = distance + 1;
            return true;
        };

    std::stringstream str;
    str << "digraph { node [ fontname = Courier ] edge [ fontname = Courier ]\n";

    bld.initials( [&]( auto st ) { init( st ); ext( st ).distance = 0; } );

    ss::search(
        ss::Order::PseudoBFS, bld, 1, ss::listen(
            [&]( auto f, auto t, auto l )
            {
                init( f );
                bool isnew = init( t );

                ext( t ).distance = std::min( ext( t ).distance, ext( f ).distance + 1 );
                std::string lbl, color;
                for ( auto txt : l.trace )
                    lbl += txt + "\n";
                if ( l.error )
                    color = "color=red";
                if ( l.accepting )
                    color = "color=blue";
                str << ext( f ).seq << " -> " << ext( t ).seq
                    << " [ label = \"" << text2dot( lbl ) << "\" " << color << "]"
                    << std::endl;
                if ( isnew && ext( t ).distance < distance )
                    return ss::Listen::Process;
                return ss::Listen::Ignore;
            },
            [&]( auto st )
            {
                init( st );
                dbg::Node< vm::Program, vm::mem::CowHeap > dn( dbg, st.snap );
                dn._ref.get();
                dn.address( dbg::DNKind::Object, bld.context().get( _VM_CR_State ).pointer );
                dn.type( dbg._state_type );
                dn.di_type( dbg._state_di_type );
                str << ext( st ).seq << " [ style=filled fillcolor=gray ]" << std::endl;
                if ( heap )
                {
                    str << ext( st ).seq << " -> " << ext( st ).seq << ".1 [ label=root ]" << std::endl;
                    str << dotDN( dn, false, brick::string::fmt( ext( st ).seq ) + "." );
                }
                return ss::Listen::Process;
            } ) );
    str << "}";
    return str.str();
}

} // anonymous namespace

std::string draw( std::shared_ptr< BitCode > bc, int distance, bool heap )
{
    ASSERT( !bc->is_symbolic() );
    ExplicitBuilder bld( bc );
    bld.context().enable_debug();
    bld.start();
    return draw_impl( bld, bc, distance, heap );
}

template< typename Builder , template< typename > class SymbolicHasher >
std::string draw( std::shared_ptr< BitCode > bc, int distance, bool heap,
                  SymbolicHasher< typename Builder::Solver > *ctx = nullptr,
                  typename Builder::Snapshot *initial = nullptr )
{
    Builder bld( bc );
    bld.context().enable_debug();
    ASSERT( initial );
    bld.start( *ctx, *initial );
    return draw_impl( bld, bc, distance, heap );
}

}
}
