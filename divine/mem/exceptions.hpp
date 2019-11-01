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
#include <brick-mem>
#include <divine/vm/divm.h>
#include <map>

namespace divine::mem
{

template< typename T, typename Pool >
struct SlavePoolSnapshotter
{
    using Internal = typename Pool::Pointer;
    using MasterPool = Pool;
    using SlavePool = brick::mem::SlavePool< Pool >;
    using SnapPool = MasterPool;
    using SnapPointer = typename SnapPool::Pointer;
    using value_type = T;

    mutable SlavePool _snap_pointers;
    mutable SnapPool _snapshots;

    SlavePoolSnapshotter( MasterPool & mp ) : _snap_pointers( mp ) {}

    SnapPointer _snap( Internal obj ) const
    {
        return *( _snap_pointers.template machinePointer< SnapPointer >( obj ) );
    }

    std::pair< value_type *, value_type * > snap_range( Internal obj ) const
    {
        auto snap = _snap( obj );
        if ( !snap )
            return std::make_pair( nullptr, nullptr );
        auto begin = _snapshots.template machinePointer< value_type >( snap );
        auto end = begin + _snapshots.size( snap ) / sizeof( value_type );
        return std::make_pair( begin, end );
    }

    bool snapped( Internal obj ) const { return !!_snap( obj ); }

    value_type * make_snap( Internal obj, unsigned size ) const
    {
        auto snap = _snapshots.allocate( size );
        ASSERT_EQ( *_snap_pointers.template machinePointer< SnapPointer >( obj ), SnapPointer() );
        *_snap_pointers.template machinePointer< SnapPointer >( obj ) = snap;
        return _snapshots.template machinePointer< value_type >( snap );
    }

    void materialise( Internal obj )
    {
        _snap_pointers.materialise( obj, sizeof( SnapPointer ) );
    }
};

template< typename Internal, typename K, typename V,
          template< typename, typename... > typename SnapshotterT, typename ... SnapOpts >
struct SnapshottedMap : SnapshotterT< typename std::map<K, V>::value_type, SnapOpts... >
{
    using Map = std::map< K, V >;
    using key_type = typename Map::key_type;
    using value_type = typename Map::value_type;
    using mapped_type = typename Map::mapped_type;
    using key_compare = typename Map::key_compare;
    using Snapshotter = SnapshotterT< value_type, SnapOpts... >;

    mutable struct Local {
        std::map< Internal, Map > _maps;
    } _l;

    using Snapshotter::Snapshotter;
    using Snapshotter::make_snap;
    using Snapshotter::snap_range;
    using Snapshotter::snapped;

    Map & operator[]( Internal obj ) { return _l._maps[ obj ]; }
    std::map< Internal, Map > & maps() { return _l._maps; }
    const std::map< Internal, Map > & maps() const { return _l._maps; }

    const value_type * at( Internal obj, key_type key ) const
    {
        const auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
        {
            const auto it = i_map->second.find( key );
            return ( it == i_map->second.end() ) ? nullptr : &*it;
        }

        auto [ begin, end ] = snap_range( obj );
        auto p = std::lower_bound( begin, end, key, []( auto & l, auto k )
                {
                return key_compare()( l.first, k );
                } );
        if ( p == end || key_compare()( key, p->first ) )
            return nullptr;
        return p;
    }

    value_type * at_rw( Internal obj, key_type key )
    {
        auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
        {
            auto it = i_map->second.find( key );
            return ( it == i_map->second.end() ) ? nullptr : &*it;
        }
        return nullptr;
    }

    void set( Internal obj, key_type key, const mapped_type &exc )
    {
        _l._maps[ obj ][ key ] = exc;
    }

    void erase( Internal obj, key_type key )
    {
        auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
            i_map->second.erase( key );
    }

    void snapshot() const
    {
        for ( const auto &[ obj, map ] : _l._maps )
        {
            unsigned snap_size = map.size() * sizeof( value_type );
            if ( snap_size == 0 )
                continue;

            auto begin = make_snap( obj, snap_size );
            auto dst = begin;
            for ( const auto & p : map )
            {
                new ( dst ) value_type( p );
                ++dst;
            }

            ASSERT_EQ( snap_size, size_t( dst ) - size_t( begin ) );
        }
        _l._maps.clear();
    }

    void materialise( Internal obj )
    {
        Snapshotter::materialise( obj );
        ASSERT( !snapped( obj ) );
    }

    void free( Internal obj )
    {
        ASSERT( !snapped( obj ) );
        _l._maps.erase( obj );
    }

    template< typename cb_t >
    int compare( Internal a, Internal b, cb_t cb ) const
    {
        auto it_a = _l._maps.find( a );
        if ( it_a != _l._maps.end() ) {
            const auto & map_a = it_a->second;
            return _cmp( map_a.begin(), map_a.end(), b, cb );
        }

        auto [ begin_a, end_a ] = snap_range( a );
        return _cmp( begin_a, end_a, b, cb );
    }

    template< typename IterA, typename cb_t >
    int _cmp( IterA begin_a, IterA end_a, Internal b, cb_t cb ) const
    {
        auto it_b = _l._maps.find( b );
        if ( it_b != _l._maps.end() )
        {
            const auto & map_b = it_b->second;
            return _cmp( begin_a, end_a, map_b.begin(), map_b.end(), cb );
        }

        auto [ begin_b, end_b ] = snap_range( b );
        return _cmp( begin_a, end_a, begin_b, end_b, cb );
    }

    template< typename IterA, typename IterB, typename cb_t >
    int _cmp( IterA begin_a, IterA end_a, IterB begin_b, IterB end_b, cb_t cb ) const
    {
        for ( ; begin_a != end_a; ++begin_a, ++begin_b )
        {
            if ( begin_b == end_b )
                return -1;
            if ( int diff = begin_a->first - begin_b->first )
                return diff;
            if ( int diff = cb( begin_a->first, begin_a->second, begin_b->second ) )
                return diff;
        }

        if ( begin_b != end_b )
            return 1;

        return 0;
    }

    template< typename F >
    void foreach( Internal i, F kv_cb )
    {
        auto map = _l._maps.find( i );
        if ( map != _l._maps.end() )
            for ( const auto &[ key, val ] : map->second )
                kv_cb( key, val );

        auto [ b, e ] = snap_range( i );
        while ( b != e )
            kv_cb( b->first, b->second ), b++;
    }

    template< typename OM >
    void copy( OM &from_m, typename OM::Internal from_object, typename OM::key_type from_offset,
               Internal to_object, key_type to_offset, int sz )
    {
        if ( sz < 1 )
            return;

        int delta = to_offset - from_offset;
        auto translate_and_insert = [this, delta, to_object]( const auto & x )
        {
            _l._maps[ to_object ][ x.first + delta ] = x.second;
        };
        auto for_each_endo = []( auto first, auto last, auto f )
        {
            while ( first != last )
                f( *first++ );
        };
        auto it = from_m._l._maps.find( from_object );
        if ( it != from_m._l._maps.end() )
        {
            auto lb = it->second.lower_bound( from_offset );
            auto ub = it->second.lower_bound( from_offset + sz );
            bool reverse = false;
            if constexpr ( std::is_same_v< OM, SnapshottedMap > )
                reverse = from_object == to_object && delta > 0;
            if ( reverse )
                for_each_endo( std::reverse_iterator( ub ), std::reverse_iterator( lb ),
                               translate_and_insert );
            else
                std::for_each( lb, ub, translate_and_insert );
        }
        else
        {
            auto [ f_begin, f_end ] = from_m.snap_range( from_object );
            auto compare_pk = []( auto &p, auto &k ) { return key_compare()( p.first, k ); };
            auto lb = std::lower_bound( f_begin, f_end, from_offset, compare_pk );
            auto ub = std::lower_bound( lb, f_end, from_offset + sz, compare_pk );
            std::for_each( lb, ub, translate_and_insert );
        }
    }

    struct const_iterator : std::iterator< std::bidirectional_iterator_tag, value_type >
    {
        using MapIter = typename Map::const_iterator;
        std::variant< MapIter, value_type * > _it;

        const_iterator( MapIter it ) : _it( it ) {}
        const_iterator( typename Map::iterator it ) : _it( it ) {}
        const_iterator( value_type * p ) : _it( p ) {}

        const value_type & operator*() const
        {
            return std::visit( []( auto && it ) -> const value_type & { return *it; }, _it );
        }
        const value_type * operator->() const { return &( operator*() ); }
        const_iterator &operator++() { std::visit( [this]( auto && it ){ ++it; }, _it ); return *this; }
        const_iterator operator++( int ) { auto i = *this; ++*this; return i; }
        const_iterator &operator--() { std::visit( [this]( auto && it ){ --it; }, _it ); return *this; }
        const_iterator operator--( int ) { auto i = *this; --*this; return i; }
        const_iterator &operator+=( int off )
        {
            std::visit( [off]( auto && it ){ it += off; }, _it);
            return *this;
        }
        const_iterator operator+( int off ) const { auto r = *this; r += off; return r; }
        const_iterator &operator-=( int off ) { return operator+=( -off ); }
        const_iterator operator-( int off ) const { auto r = *this; r -= off; return r; }
        bool operator!=( const_iterator o ) const { return _it != o._it; }
        bool operator==( const_iterator o ) const { return _it == o._it; }
        bool operator<( const_iterator o ) const { return _it < o._it; }
    };

    const_iterator cbegin( Internal obj ) const { return begin( obj ); }
    const_iterator begin( Internal obj ) const
    {
        const auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
            return i_map->second.begin();

        auto [ begin, end ] = snap_range( obj );
        return begin;
    }

    const_iterator cend( Internal obj ) const { return end( obj ); }
    const_iterator end( Internal obj ) const
    {
        const auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
            return i_map->second.end();

        auto [ begin, end ] = snap_range( obj );
        return end;
    }

    const_iterator upper_bound( Internal obj, key_type key ) const
    {
        const auto i_map = _l._maps.find( obj );
        if ( i_map != _l._maps.end() )
        {
            const auto it = i_map->second.upper_bound( key );
            return const_iterator( it );
        }

        auto [ begin, end ] = snap_range( obj );
        auto p = std::upper_bound( begin, end, key, []( auto k, auto & l )
                {
                return key_compare()( k, l.first );
                } );
        return const_iterator( p );
    }
};

template< typename K >
struct Interval {
    K from;
    mutable K to;
    bool operator<( const Interval & o ) const { return from < o.from; }
    bool operator<( const K k ) const { return from < k; }
    friend bool operator<( const K k, const Interval & i ) { return k < i.from; }
    Interval operator+( K tr ) const { return { from + tr, to + tr }; }
    Interval operator-( K tr ) const { return { from - tr, to - tr }; }
    //TODO: demystify (ie. dont abuse operator- for comparison)
    // (once operator<=> is here...)
    template< typename Other >
    int operator-( const Other & o ) const
    {
        if ( from == o.from )
            return to - o.to;
        return from - o.from;
    }

    Interval( K from, K to ) : from( from ), to( to ) {}
    explicit Interval( K from ) : from( from ), to( from ) {}

    friend std::ostream& operator<<( std::ostream &os, const Interval & i )
    {
        return os << '[' << i.from << ',' << i.to << ')';
    }
};

template< typename K, typename T, typename Pool >
struct IntervalMetadataMap
{
    using key_type = Interval< K >;
    using scalar_type = K;
    using Internal = typename Pool::Pointer;
    using map = SnapshottedMap< Internal, key_type, T, SlavePoolSnapshotter, Pool >;
    using value_type = typename map::value_type;
    using const_iterator = typename map::const_iterator;

    map _storage;

    IntervalMetadataMap( Pool & mp ) : _storage( mp ) {}

    template< typename TT >
    auto insert( Internal obj, key_type key, TT && data )
    {
        return insert( obj, key.from, key.to, std::forward< TT >( data ) );
    }

    template< typename TT >
    auto insert( Internal obj, scalar_type from, scalar_type to, TT && data )
    {
        auto it = erase_or_create( obj, from, to );
        if ( from >= to )
            return it; // == _storage[ obj ].end()
        return _storage[ obj ].insert( it, { { from, to }, std::forward< TT >( data ) } );
    }

    void erase( Internal obj, scalar_type from, scalar_type to )
    {
        if ( _storage.maps().count( obj ) )
            erase_or_create( obj, from, to );
    }

    auto erase_or_create( Internal obj, scalar_type from, scalar_type to )
    {
        auto & map = _storage[ obj ];
        if ( from >= to || map.empty() )
            return map.end();

        auto it = map.upper_bound( key_type( from ) );

        if ( it != map.begin() ) {
            --it;
        }
        if ( it->first.from < from && to < it->first.to ) {
            // Splitting an existing interval in two
            map.insert( { { to, it->first.to }, it->second } );
            it->first.to = from;
        } else {
            if ( it->first.to <= from ) {
                ++it;
            } else if ( it->first.from < from
                    && to >= it->first.to ) { // Chomp from right
                it->first.to = from;
                ++it;
            }

            while ( it != map.end()
                    && from <= it->first.from
                    && to >= it->first.to ) { // Destroy covered
                it = map.erase( it );
            }

            if ( it != map.end()
                    && it->first.from < to
                    && to < it->first.to ) { // Chomp from left
                // XXX: once we have a newer stdlib...
                // auto nh = map.extract( it++ );
                // nh.key().from = to;
                // it = map.insert( it, std::move( nh ) );

                // It would *probably* be safe to change the key inside the map
                T data = std::move( it->second );
                key_type i = it->first;
                map.erase( it++ );
                i.from = to;
                it = map.insert( it, { i, std::move( data ) } );
            }
        }

        return it;
    }

    const value_type * at( Internal obj, scalar_type val ) const
    {
        auto it = _storage.upper_bound( obj, key_type( val ) );
        if ( it == nullptr || it == _storage.begin( obj ) )
            return nullptr;
        --it;
        if ( _inside( val, it->first ) )
            return &*it;
        return nullptr;
    }

    template< typename OM >
    void copy( OM &from_m, typename OM::Internal from_object, typename OM::scalar_type from_offset,
               Internal to_object, scalar_type to_offset, int sz )
    {
        if ( sz < 1 )
            return;

        int delta = to_offset - from_offset;

        // Left partial
        if ( 0 < from_offset )
        {
            if ( auto *p = from_m.at( from_object, from_offset - 1 ) )
            {
                if ( p->first.to > from_offset + sz )
                {
                    // Inner
                    insert( to_object, to_offset, to_offset + sz, p->second );
                    return;
                }
                insert( to_object, to_offset, p->first.to + delta, p->second );
                sz -= p->first.to - from_offset;
                from_offset = p->first.to;
                to_offset = p->first.to + delta;
            }
        }
        // Right partial
        if ( auto *p = from_m.at( from_object, from_offset + sz ) )
        {
            insert( to_object, p->first.from + delta, to_offset + sz, p->second );
            sz = p->first.from - from_offset;
        }

        if ( sz < 1 )
            return;

        // Clean existing intervals in destination
        erase( to_object, to_offset, to_offset + sz );
        _storage.copy( from_m._storage, from_object, key_type( from_offset ),
                       to_object, key_type( to_offset ), sz );
    }

    static bool _inside( const scalar_type &p, const key_type &i ) {
        bool b = i.from <= p && p < i.to;
        return b;
    }
};


// Map assigns metadata exceptions to shadow memory locations
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

    template< typename cb_t >
    int compare( Internal a, Internal b, int sz, cb_t cb )
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
            if ( int diff = cb( i_a->second, i_b->second ) )
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
