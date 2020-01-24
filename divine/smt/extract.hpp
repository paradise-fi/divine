// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/memory.hpp>
#include <divine/vm/memory.tpp>
#include <divine/smt/builder.hpp>
#include <divine/smt/rpn.hpp>

namespace divine::smt
{

template< typename Builder >
struct Extract : Builder
{
    using Node = typename Builder::Node;
    using expr_t = brq::smt_expr< std::vector >;

    template< typename... Args >
    Extract( vm::CowHeap &heap, Args && ... args )
        : Builder( std::forward< Args >( args )... ), _heap( heap )
    {}

    expr_t read( vm::HeapPointer ptr )
    {
        auto data = _heap.unsafe_bytes( ptr );
        return expr_t{ data.begin(), data.end() - 1 };
    }

    expr_t read_constraints( vm::HeapPointer ptr )
    {
        vm::PointerV map( ptr ), clause;
        expr_t expr;

        if ( !_heap.valid( ptr ) )
            return expr;

        bool first = true;

        for ( int i = 0; i < _heap.size( ptr ); i += vm::PointerBytes )
        {
            _heap.read_shift( map, clause );
            if ( clause.pointer() && _heap.valid( clause.cooked() ) )
            {
                ASSERT_EQ( clause.cooked().type(), vm::PointerType::Marked );
                auto b = _heap.unsafe_bytes( clause.cooked() );
                expr_t clause_expr{ b.begin(), b.end() - 1 };
                TRACE( "clause:", clause_expr, first );
                expr.apply( clause_expr );
                if ( first )
                    first = false;
                else
                    expr.apply( brq::smt_op::bool_and );
            }
        }

        TRACE( "constraints:", expr );
        return expr;
    }

    Node build( vm::HeapPointer p );
    Node convert( vm::HeapPointer p )
    {
        auto it = _values.find( p );
        if ( it == _values.end() )
            it = _values.emplace( p, build( p ) ).first;
        return it->second;
    }

    vm::CowHeap &_heap;
    std::unordered_map< vm::HeapPointer, Node > _values;
};

}

namespace divine::smt::extract
{

using SMTLib2 = Extract< builder::SMTLib2 >;

template< typename heap_t >
std::string to_string( heap_t &heap, vm::HeapPointer ptr )
{
    brq::smtlib_context ctx;
    SMTLib2 extract( heap, ctx, "", false );
    auto assumes = extract.read( ptr );
    TRACE( "smt::extract::to_string", assumes );
    return to_string( evaluate( extract, assumes ) );
}

#if OPT_Z3
using Z3      = Extract< builder::Z3 >;
#endif

#if OPT_STP
using STP     = Extract< builder::STP >;
#endif

}
