// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Adam Matoušek <xmatous3@fi.muni.cz>
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

#include <brick-types>

#include <divine/vm/value.hpp>

namespace divine::vm::mem
{

namespace bitlevel = brick::bitlevel;

template< typename Pool_ >
struct Base
{
    using Pool = Pool_;
    using Internal = typename Pool::Pointer;

    mutable Pool _objects, _snapshots;

    struct Loc : public brick::types::Ord
    {
        using Internal = typename Pool::Pointer;
        Internal object;
        uint32_t objid, offset;

        Loc( Internal o, int objid, int off )
            : object( o ), objid( objid ), offset( off )
        {}

        Loc operator-( int i ) const { Loc r = *this; r.offset -= i; return r; }
        Loc operator+( int i ) const { Loc r = *this; r.offset += i; return r; }
        bool operator==( const Loc & o) const { return object == o.object && offset == o.offset; }

        bool operator<=(const Loc & o) const
        {
            return object < o.object || (object == o.object && offset <= o.offset);
        }

        bool operator<(const Loc & o) const
        {
            return object < o.object || (object == o.object && offset < o.offset);
        }
    };

    Loc loc( HeapPointer p, Internal i ) const
    {
	return Loc( i, p.object(), p.offset() );
    }
};

template< typename Next >
struct ShadowBase : Next
{
    using typename Next::Expanded;
    using typename Next::Loc;
    using typename Next::Internal;

    template< typename V >
    void write( Loc, V, Expanded * ) {}
    template< typename V >
    void read( Loc, V &, Expanded * ) const {}

    template< typename FromH, typename ToH >
    static void copy_word( FromH &, ToH &, typename FromH::Loc, Expanded, Loc, Expanded ) {}

    template< typename FromH, typename ToH >
    static void copy_init_src( FromH &, ToH &, typename FromH::Internal, int off, const Expanded & )
    {
        ASSERT_EQ( off % 4, 0 );
    }

    template< typename ToH >
    static void copy_init_dst( ToH &, Internal, int off, const Expanded & )
    {
        ASSERT_EQ( off % 4, 0 );
    }

    template< typename FromH, typename ToH >
    static void copy_byte( FromH &, ToH &, typename FromH::Loc, const Expanded &, Loc, Expanded & ) {}

    template< typename ToH >
    static void copy_done( ToH &, Internal, int off, Expanded & )
    {
        ASSERT_EQ( off % 4, 0 );
    }

    template< typename OtherSh >
    int compare_word( OtherSh &, typename OtherSh::Loc a, Expanded, Loc b, Expanded )
    {
            ASSERT_EQ( a.offset % 4, 0 );
            ASSERT_EQ( b.offset % 4, 0 );
            return 0;
    }

    template< typename OtherSh >
    int compare_byte( OtherSh &, typename OtherSh::Loc a, Expanded, Loc b, Expanded )
    {
        ASSERT_EQ( a.offset % 4, b.offset % 4 );
        return 0;
    }

    void make( Internal, int ) {}
    void free( Internal ) {}
};

}
