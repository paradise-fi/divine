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

namespace divine::vm
{

namespace smt = brick::smt;

struct FormulaMap
{
    sym::Formula *hp2form( HeapPointer ptr )
    {
        return reinterpret_cast< sym::Formula * >( heap.unsafe_bytes( ptr ).begin() );
    }

    FormulaMap( CowHeap &heap, std::string suff, std::unordered_set< int > &inputs, std::ostream &out )
        : heap( heap ), inputs( inputs ), suff( suff ), out( out )
    { }

    static smt::Printer type( int bitwidth )
    {
        return bitwidth == 1 ? smt::type( "Bool" ) : smt::bitvecT( bitwidth );
    }

    std::string_view convert( HeapPointer ptr );

    std::string_view operator[]( HeapPointer p )
    {
        auto it = ptr2Sym.find( p );
        ASSERT( it != ptr2Sym.end() );
        return it->second;
    }

    void pathcond()
    {
        smt::Vector args;
        for ( auto ptr : pcparts )
            args.emplace_back( smt::symbol( (*this)[ ptr ] ) );

        out << smt::defineConst( "pathcond" + suff, smt::type( "Bool" ), smt::bigand( args ) )
            << std::endl;
    }

    CowHeap &heap;
    std::unordered_map< HeapPointer, std::string > ptr2Sym;
    std::unordered_set< HeapPointer > pcparts;
    std::unordered_set< int > &inputs;
    std::string suff;
    int valcount = 0;
    std::ostream &out;
};

}
