// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Adam Matoušek <xmatous3@fi.muni.cz>
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
#include <divine/vm/divm.h>

namespace divine::mem
{

template< typename Map, typename Pool, uint8_t NLayers >
struct Snapshotter
{
    using MasterPool = Pool;
    using SlavePool = brick::mem::SlavePool< Pool >;
    using Internal = typename Pool::Pointer;
    using key_type = typename Map::key_type;
    using value_type = typename Map::value_type;
    using mapped_type = typename Map::mapped_type;
    using key_compare = typename Map::key_compare;
    using Layer = uint8_t;
    using SnapPool = MasterPool; //TODO: customise?
    using SnapPointer = typename SnapPool::Pointer;

    using DirEntry = uint32_t; // offset within snapshot
    using Directory = std::array< DirEntry, NLayers - 1 >;

    mutable struct Local {
        std::map< Internal, std::array< Map, NLayers > > _maps;
    } _l;

    mutable SlavePool _snap_pointers;
    mutable SnapPool _snapshots;

    Snapshotter( MasterPool & mp ) : _snap_pointers( mp ) {}

    const value_type * at( Internal obj, key_type key, Layer layer ) const
    {
        const auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
        {
            const auto it = i_map->second[ layer ].find( key );
            return ( it == i_map->second[ layer ].end() ) ? nullptr : &*it;
        }

        auto [ begin, end ] = _snap_range( obj, layer );
        auto p = std::lower_bound( begin, end, key, []( auto & l, auto k )
                {
                return key_compare()( l.first, k );
                } );
        if ( p == end || key_compare()( key, p->first ) )
            return nullptr;
        return p;
    }

    value_type * at_rw( Internal obj, key_type key, Layer layer )
    {
        auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
        {
            auto it = i_map->second[ layer ].find( key );
            return ( it == i_map->second[ layer ].end() ) ? nullptr : &*it;
        }
        return nullptr;
    }

    void set( Internal obj, key_type key, Layer layer, const mapped_type &exc )
    {
        _l._maps[ obj ][ layer ][ key ] = exc;
    }

    void erase( Internal obj, key_type key, Layer layer )
    {
        auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
            i_map->second[ layer ].erase( key );
    }

    void snapshot() const
    {
        for ( const auto &[ obj, maps ] : _l._maps )
        {
            unsigned snap_size = 0;
            for ( const auto & m : maps )
                snap_size += m.size() * sizeof( value_type );
            if ( snap_size == 0 )
                continue;

            snap_size += sizeof( Directory );
            auto snap = _snapshots.allocate( snap_size );
            ASSERT_EQ( *_snap_pointers.template machinePointer< SnapPointer >( obj ), SnapPointer() );
            *_snap_pointers.template machinePointer< SnapPointer >( obj ) = snap;

            Layer layer = 0;
            auto & dir = *_snapshots.template machinePointer< Directory >( snap );
            auto begin = _snapshots.template machinePointer< value_type >( snap, sizeof( Directory ) );
            auto dst = begin;
            for ( auto & map : maps )
            {
                if ( layer > 0 )
                    dir[ layer - 1 ] = DirEntry( dst - begin );
                for ( const auto & p : map )
                {
                    new ( dst ) value_type( p );
                    ++dst;
                }
                ++layer;
            }

            ASSERT_EQ( snap_size, size_t( dst ) - size_t( begin ) + sizeof( Directory ) );
        }
        _l._maps.clear();
    }

    void materialise( Internal obj )
    {
        _snap_pointers.materialise( obj, sizeof( SnapPointer ) );
        ASSERT( !_snap( obj ) );
    }

    void free( Internal obj )
    {
        ASSERT( !_snap( obj ) );
        _l._maps.erase( obj );
    }

    int compare( Internal a, Internal b, Layer layer, bool ignore_values ) const
    {
        auto it_a = _l._maps.find( a );
        if ( it_a != _l._maps.end() ) {
            const auto & map_a = it_a->second[ layer ];
            return _cmp( map_a.begin(), map_a.end(), b, layer, ignore_values );
        }

        auto [ begin_a, end_a ] = _snap_range( a, layer );
        return _cmp( begin_a, end_a, b, layer, ignore_values );
    }

    template< typename IterA >
    int _cmp( IterA begin_a, IterA end_a, Internal b, Layer layer, bool ignore_values ) const
    {
        auto it_b = _l._maps.find( b );
        if ( it_b != _l._maps.end() )
        {
            const auto & map_b = it_b->second[ layer ];
            return _cmp( begin_a, end_a, map_b.begin(), map_b.end(), ignore_values );
        }

        auto [ begin_b, end_b ] = _snap_range( b, layer );
        return _cmp( begin_a, end_a, begin_b, end_b, ignore_values );
    }

    template< typename IterA, typename IterB >
    int _cmp( IterA begin_a, IterA end_a, IterB begin_b, IterB end_b, bool ignore_values ) const
    {
        for ( ; begin_a != end_a; ++begin_a, ++begin_b )
        {
            if ( begin_b == end_b )
                return -1;
            if ( int diff = begin_a->first - begin_b->first )
                return diff;
            if ( !ignore_values )
                if ( int diff = begin_a->second - begin_b->second )
                    return diff;
        }

        if ( begin_b != end_b )
            return 1;

        return 0;
    }

    template< typename OM >
    void copy( OM &from_m, typename OM::Internal from_object, typename OM::key_type from_offset,
               Internal to_object, key_type to_offset, int sz )
    {
        int delta = to_offset - from_offset;
        for ( Layer layer = 0; layer < NLayers; ++layer )
        {
            auto translate_and_insert = [this, delta, to_object, layer]( const auto & x )
            {
                _l._maps[ to_object ][ layer ][ x.first + delta ] = x.second;
            };
            auto it = from_m._l._maps.find( from_object );
            if ( it != from_m._l._maps.end() )
            {
                auto lb = it->second[ layer ].lower_bound( from_offset );
                auto ub = it->second[ layer ].lower_bound( from_offset + sz );
                std::for_each( lb, ub, translate_and_insert );
            }
            else
            {
                auto [ f_begin, f_end ] = from_m._snap_range( from_object, layer );
                auto compare_pk = []( auto &p, auto &k ) { return key_compare()( p.first, k ); };
                auto lb = std::lower_bound( f_begin, f_end, from_offset, compare_pk );
                auto ub = std::lower_bound( lb, f_end, from_offset + sz, compare_pk );
                std::for_each( lb, ub, translate_and_insert );
            }
        }
    }

    std::pair< value_type *, value_type * > _snap_range( Internal obj, Layer layer ) const
    {
        ASSERT_LT( layer, NLayers );
        auto snap = _snap( obj );
        if ( !snap )
            return std::make_pair( nullptr, nullptr );
        auto & dir = *_snapshots.template machinePointer< Directory >( snap );
        auto begin = _snapshots.template machinePointer< value_type >( snap, sizeof( Directory ) );
        auto end = begin + ( _snapshots.size( snap ) - sizeof( Directory ) ) / sizeof( value_type );
        if constexpr ( NLayers > 1 )
        {
            if ( layer < NLayers - 1 )
                end = begin + dir[ layer ];
            if ( layer > 0 )
                begin += dir[ layer - 1 ];
        }
        return std::make_pair( begin, end );
    }
    SnapPointer _snap( Internal obj ) const
    {
        return *( _snap_pointers.template machinePointer< SnapPointer >( obj ) );
    }

};

template< typename ExceptionType, typename Loc_ >
struct ExceptionMap
{
    using Loc = Loc_;
    using Internal = typename Loc::Internal;
    using ExcMap = std::map< Loc, ExceptionType >;
    using Lock = std::lock_guard< std::mutex >;

    ExceptionMap &operator=( const ExceptionType & o ) = delete;

    ExceptionType &at( Internal obj, int wpos )
    {
        Lock lk( _mtx );

        auto it = _exceptions.find( Loc( obj, wpos ) );
        ASSERT( it != _exceptions.end() );
        return it->second;
    }

    bool has( Internal obj, int wpos )
    {
        Lock lk( _mtx );

        auto it = _exceptions.find( Loc( obj, wpos ) );
        return it != _exceptions.end();
    }

    void set( Internal obj, int wpos, const ExceptionType &exc )
    {
        Lock lk( _mtx );
        _exceptions[ Loc( obj, wpos ) ] = exc;
    }

    void free( Internal obj )
    {
        Lock lk( _mtx );

        auto lb = _exceptions.lower_bound( Loc( obj, 0 ) );
        auto ub = _exceptions.upper_bound( Loc( obj, (1 << _VM_PB_Off) - 1 ) );
        _exceptions.erase( lb, ub );
    }

    int compare( Internal a, Internal b, int sz, bool ignore_values )
    {
        Lock lk( _mtx );

        auto lb_a = _exceptions.lower_bound( Loc( a, 0 ) ),
             lb_b = _exceptions.lower_bound( Loc( b, 0 ) );
        auto ub_a = _exceptions.upper_bound( Loc( a, sz ) ),
             ub_b = _exceptions.upper_bound( Loc( b, sz ) );

        auto i_b = lb_b;
        for ( auto i_a = lb_a; i_a != ub_a; ++i_a, ++i_b )
        {
            if ( i_b == ub_b )
                return -1;
            if ( int diff = i_a->first.offset - i_b->first.offset )
                return diff;
            if ( !ignore_values )
                if ( int diff = i_a->second - i_b->second )
                    return diff;
        }

        if ( i_b != ub_b )
            return 1;

        return 0;
    }

    template< typename OM >
    void copy( OM &from_m, typename OM::Loc from, Loc to, int sz )
    {
        Lock lk( _mtx );
        typename OM::Loc from_p( from.object, from.offset + sz );

        int delta = to.offset - from.offset;

        auto lb = from_m._exceptions.lower_bound( from );
        auto ub = from_m._exceptions.upper_bound( from_p );
        std::transform( lb, ub, std::inserter( _exceptions, _exceptions.begin() ), [&]( auto x )
        {
            auto fl = x.first;
            Loc l( to.object, fl.offset + delta );
            return std::make_pair( l, x.second );
        } );
    }

    bool empty()
    {
        Lock lk( _mtx );
        return std::all_of( _exceptions.begin(), _exceptions.end(),
                            []( const auto & e ) { return ! e.second.valid(); } );
    }

    ExcMap _exceptions;
    mutable std::mutex _mtx;
};

}
