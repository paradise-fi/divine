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

union TaggedOffset {
    struct {
        uint32_t off : 30;
        uint32_t tag : 2;
    };
    uint32_t raw;
    TaggedOffset( uint32_t raw ) : raw( raw ) {}
    TaggedOffset( uint32_t offset, uint32_t tag ) : tag( tag ), off( offset ) {}
    operator uint32_t() const { return raw; }
    TaggedOffset & operator=( uint32_t r ) { raw = r; return *this; }
};

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

    using Maps = SnapshottedMap< TaggedOffset, uint32_t, typename Next::Pool >;
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

        if ( auto *p = map.at( l.object, { l.offset, layer } ) )
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
        map.set( l.object, { l.offset, layer }, v.cooked() );
    }

    template< typename FromH, typename ToH >
    static void copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int sz, bool internal )
    {
        if ( internal )
        {
            for ( uint8_t layer = 0; layer < NLayers; ++layer )
                to_h._maps.copy( from_h._maps, from.object, { from.offset, layer },
                                 to.object, { to.offset, layer }, sz );
        }
        Next::copy( from_h, from, to_h, to, sz, internal );
    }

    template< typename OtherSH >
    int compare( OtherSH &o, typename OtherSH::Internal a, Internal b, int sz, bool skip_objids )
    {
        auto ltypes = layer_types();
        auto no_objids_cb = [ltypes, skip_objids]( auto k )
        {
            return ltypes[ k.tag ] == MetaType::Pointers && skip_objids;
        };
        if ( int diff = _maps.compare( a, b, no_objids_cb ) )
            return diff;
        int diff = Next::compare( o, a, b, sz, skip_objids );
        return diff;
    }

    template< typename S, typename F >
    void hash( Internal i, int size, S &state, F ptr_cb ) const
    {
        state.realign();
        auto ltypes = layer_types();
        auto data_cb = [&]( uint32_t v ) { state.update_aligned( v ); };
        auto hash_cb = [&, ltypes]( auto k, auto v )
        {
            data_cb( k );
            if ( ltypes[ k.tag ] == MetaType::Pointers )
                ptr_cb( v );
            else
                data_cb( k );
        };
        _maps.foreach( i, hash_cb );
        Next::hash( i, size, state, ptr_cb );
    }

    MetaType layer_type( uint8_t layer ) const
    {
        return (*_type)[ layer ].load( std::memory_order_relaxed );
    }
    void layer_type( uint8_t layer, MetaType type )
    {
        return (*_type)[ layer ].store( type, std::memory_order_relaxed );
    }
    std::array< MetaType, NLayers > layer_types() const
    {
        std::array<MetaType, NLayers > ltypes;
        for ( int i = 0; i < NLayers; ++i )
            ltypes[ i ] = layer_type( i );
        return ltypes;
    }
};

}

