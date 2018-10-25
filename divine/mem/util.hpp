// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2011-2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/types.hpp>
#include <divine/vm/pointer.hpp>
#include <unordered_map>

namespace divine::mem
{
    template< typename Pointer >
    struct NoopCmp
    {
        void pointer( Pointer, Pointer, int ) {}
        void shadow( Pointer, Pointer ) {}
        void bytes( Pointer, Pointer, int ) {}
        void marked( Pointer, Pointer ) {}
        void size( Pointer, Pointer, int, int ) {}
        void structure( Pointer, Pointer, int, int ) {}
    };

    template< typename H1, typename H2, typename CB >
    int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H1::Pointer r2,
                std::unordered_map< int, int > &v1, std::unordered_map< int, int > &v2, int &seq,
                CB &callback );

    template< typename Heap >
    int hash( Heap &heap, typename Heap::Pointer root,
              std::unordered_map< int, int > &visited,
              brick::hash::jenkins::SpookyState &state, int depth );

    template< typename H1, typename H2, typename CB >
    int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H1::Pointer r2, CB &callback )
    {
        std::unordered_map< int, int > v1, v2;
        int seq = 1;
        return compare( h1, h2, r1, r2, v1, v2, seq, callback );
    }

    template< typename H1, typename H2 >
    int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H1::Pointer r2 )
    {
        NoopCmp< typename H1::Pointer > callback;
        return compare( h1, h2, r1, r2, callback );
    }

    using brick::hash::hash128_t;

    template< typename Heap >
    hash128_t hash( Heap &heap, typename Heap::Pointer root )
    {
        std::unordered_map< int, int > visited;
        brick::hash::jenkins::SpookyState state( 0, 0 );
        hash( heap, root, visited, state, 0 );
        return state.finalize();
    }

    enum class CloneType { All, SkipWeak, HeapOnly };

    template< typename FromH, typename ToH >
    auto clone( FromH &f, ToH &t, typename FromH::Pointer root,
                std::map< typename FromH::Pointer, typename FromH::Pointer > &visited,
                CloneType ct ) -> typename FromH::Pointer;

    template< typename FromH, typename ToH >
    auto clone( FromH &f, ToH &t, typename FromH::Pointer root,
                CloneType ct = CloneType::All ) -> typename FromH::Pointer
    {
        std::map< typename FromH::Pointer, typename FromH::Pointer > visited;
        return clone( f, t, root, visited, ct );
    }

    template< typename Heap, typename F, typename... Roots >
    void leaked( Heap &h, F f, Roots... roots );
}
