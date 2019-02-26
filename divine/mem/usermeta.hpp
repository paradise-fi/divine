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
#include <atomic>

namespace divine::mem
{

template< typename Next >
struct UserMeta : Next
{
    using typename Next::Internal;
    using typename Next::Loc;
    using Value = typename Next::UIntV;
    static constexpr const uint8_t NLayers = 4;

    enum MetaType : uint8_t { Unknown, Pointers, Scalars };
    using LayerTypes = std::array< std::atomic< MetaType >, NLayers >;
    std::shared_ptr< LayerTypes > _type;

    using Map = std::map< uint32_t, uint32_t >;
    using Maps = Snapshotter< Map, typename Next::Pool, NLayers >;
    mutable Maps _maps;


    UserMeta() : _type( std::make_shared< LayerTypes >() ), _maps( Next::_objects )
    {
        for ( auto & t : *_type )
            t.store( MetaType::Unknown, std::memory_order_relaxed );
    }

    void materialise( Internal p, int size )
    {
        _maps.materialise( p );
        Next::materialise( p, size );
    }

    void notify_snapshot() const
    {
        _maps.snapshot();
        Next::notify_snapshot();
    }

    void free( Internal p ) const
    {
        _maps.free( p );
        Next::free( p );
    }

    Value peek( Loc l, int layer )
    {
        auto &map = _maps;

        if ( auto *p = map.at( l.object, l.offset, layer ) )
        {
           return Value( p->second , -1, layer_type( layer ) == MetaType::Pointers );
        }

        return Value( 0, 0, layer_type( layer ) == MetaType::Pointers );
    }

    void poke( Loc l, int layer, Value v )
    {
        auto &map = _maps;
        if ( layer_type( layer ) == MetaType::Unknown )
            layer_type( layer, v.pointer() ? MetaType::Pointers : MetaType::Scalars );

        ASSERT_EQ( layer_type( layer ) == MetaType::Pointers, v.pointer() );
        map.set( l.object, l.offset, layer, v.cooked() );
    }

    template< typename FromH, typename ToH >
    static void copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int sz, bool internal )
    {
        if ( internal )
            to_h._maps.copy( from_h._maps, from.object, from.offset, to.object, to.offset, sz );
        Next::copy( from_h, from, to_h, to, sz, internal );
    }

    template< typename OtherSH >
    int compare( OtherSH &o, typename OtherSH::Internal a, Internal b, int sz, bool skip_objids )
    {
        for ( uint8_t layer = 0; layer < NLayers; ++layer )
        {
            bool no_objids = layer_type( layer ) == MetaType::Pointers && skip_objids;
            if ( int diff = _maps.compare( a, b, layer, no_objids ) )
                return diff;
        }
        int diff = Next::compare( o, a, b, sz, skip_objids );
        return diff;
    }

    MetaType layer_type( uint8_t layer ) const
    {
        return (*_type)[ layer ].load( std::memory_order_relaxed );
    }
    void layer_type( uint8_t layer, MetaType type )
    {
        return (*_type)[ layer ].store( type, std::memory_order_relaxed );
    }
};

}

