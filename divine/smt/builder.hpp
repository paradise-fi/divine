// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
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

#include <divine/vm/heap.hpp>
#include <runtime/abstract/formula.h>
#include <brick-smt>

#if OPT_Z3
#include <z3++.h>
#endif

namespace divine::smt
{

template< typename Formula >
struct FormulaMap
{
    FormulaMap( vm::CowHeap &heap, std::string suff )
       : heap( heap ), suff( suff )
    {}

    sym::Formula *hp2form( vm::HeapPointer ptr )
    {
        return reinterpret_cast< sym::Formula * >( heap.unsafe_bytes( ptr ).begin() );
    }

    const Formula& operator[]( vm::HeapPointer p )
    {
        auto it = ptr2Sym.find( p );
        ASSERT( it != ptr2Sym.end() );
        return it->second;
    }

    vm::CowHeap &heap;
    std::string suff;
    std::unordered_set< vm::HeapPointer > pcparts;
    std::unordered_map< vm::HeapPointer, Formula > ptr2Sym;
};

struct SMTLibFormulaMap : FormulaMap< std::string > {
    SMTLibFormulaMap( vm::CowHeap &heap, std::unordered_set< int > &indices,
                      std::ostream &out, std::string suff = "" )
        : FormulaMap( heap, suff ), indices( indices ), out( out )
    {}

    static brick::smt::Printer type( int bitwidth ) {
        return bitwidth == 1 ? brick::smt::type( "Bool" ) : brick::smt::bitvecT( bitwidth );
    }

    std::string_view convert( vm::HeapPointer ptr );

    void pathcond()
    {
        brick::smt::Vector args;
        for ( auto ptr : pcparts )
            args.emplace_back( brick::smt::symbol( (*this)[ ptr ] ) );

        out << brick::smt::defineConst( "pathcond" + suff, brick::smt::type( "Bool" ),
                                        brick::smt::bigand( args ) )
            << std::endl;
    }

    int valcount = 0;
    std::unordered_set< int > &indices;
    std::ostream &out;
};

#if OPT_Z3
struct Z3FormulaMap : FormulaMap< z3::expr >
{
    Z3FormulaMap( vm::CowHeap &heap, z3::context &c, std::string suff = "" )
        : FormulaMap( heap, suff ), ctx( c )
    {}

    z3::expr convert( vm::HeapPointer ptr );

    z3::expr pathcond()
    {
        z3::expr_vector args( ctx );
        for ( const auto & ptr : pcparts ) {
            auto part = (*this)[ ptr ];
            if ( part.is_bv() ) {
                ASSERT_EQ( part.get_sort().bv_size(), 1 );
                args.push_back( part == ctx.bv_val( 1, 1 ) );
            } else {
                args.push_back( part );
            }
        }
        return z3::mk_and( args );
    }

private:
    z3::expr toz3( vm::HeapPointer ptr );
    z3::expr toz3( sym::Formula *formula );

    z3::context &ctx;
};
#endif

}
