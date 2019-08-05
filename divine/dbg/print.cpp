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

#include <divine/dbg/print.tpp>
#include <divine/dbg/context.hpp>

using namespace std::literals;

namespace divine::dbg::print
{

template< typename Heap >
std::string raw( Heap &heap, vm::HeapPointer hloc, int sz )
{
    std::stringstream out;

    auto bytes = heap.unsafe_bytes( hloc, 0, sz );
    /*
    auto types = heap.type( hloc, hloc.offset(), sz );
    auto defined = heap.defined( hloc, hloc.offset(), sz );
    */

    for ( int c = 0; c < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ); ++c )
    {
        int col = 0;
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
        {
            ASSERT_LEQ( i, bytes.size() );
            print::hexbyte( out, col, i, bytes[ i ] );
        }
        /*
        print::pad( out, col, 30 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::hexbyte( out, col, i, defined[ i ] );
        print::pad( out, col, 60 ); out << "| ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, bytes[ i ] );
        print::pad( out, col, 72 ); out << " | ";
        for ( int i = c * 12; i < std::min( (c + 1) * 12, sz ); ++i )
            print::ascbyte( out, col, types[ i ] );
        */
        print::pad( out, col, 84 );
        if ( c + 1 < ( sz / 12 ) + ( sz % 12 ? 1 : 0 ) )
            out << std::endl;
    }

    return out.str();
}

// template struct Print< vm::Context< vm::CowHeap > >;
template struct Print< dbg::Context< vm::CowHeap > >;
template struct Print< dbg::DNContext< vm::CowHeap > >;
template std::string raw< vm::CowHeap >( vm::CowHeap &, vm::HeapPointer, int );

}
