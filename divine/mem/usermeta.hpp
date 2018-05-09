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

#include <divine/mem/exceptions.hpp>
#include <divine/vm/value.hpp>
#include <array>

namespace divine::mem
{

template< typename Next >
struct UserMeta : Next
{
    using typename Next::Internal;
    using typename Next::Loc;
    using Value = typename Next::UIntV;

    struct Map : ExceptionMap< uint32_t, Loc >
    {
        Map() = default;
        Map( Map && ) = default;
        enum { Pointers, Scalars, Unknown } _type = Unknown;
    };

    using Maps = std::array< Map, 4 >; /* TODO make this a vector? */
    std::shared_ptr< Maps > _maps;

    UserMeta() : _maps( new Maps ) {}

    void free( Internal p )
    {
        for ( auto &e : *_maps )
            e.free( p );
        Next::free( p );
    }

    bool equal( Internal a, Internal b, int sz )
    {
        for ( unsigned i = 0; i < _maps->size(); ++ i )
            if ( !_maps->at( i ).equal( a, b, sz ) )
                return false;
        return Next::equal( a, b, sz );
    }

    Value peek( Loc l, int key )
    {
        auto &map = _maps->at( key );
        if ( map.has( l.object, l.offset ) )
            return Value( map.at( l.object, l.offset ), -1, map._type == Map::Pointers );
        else
            return Value( 0, 0, map._type == Map::Pointers );
    }

    void poke( Loc l, int key, Value v )
    {
        auto &map = _maps->at( key );
        if ( map._type == Map::Unknown )
            map._type = v.pointer() ? Map::Pointers : Map::Scalars;

        ASSERT_EQ( map._type == Map::Pointers, v.pointer() );
        map.set( l.object, l.offset, v.cooked() );
    }

    template< typename FromH, typename ToH >
    static void copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int sz, bool internal )
    {
        if ( internal )
            for ( unsigned i = 0; i < to_h._maps->size(); ++ i )
                to_h._maps->at( i ).copy( from_h._maps->at( i ), from, to, sz );
        Next::copy( from_h, from, to_h, to, sz, internal );
    }
};

}

