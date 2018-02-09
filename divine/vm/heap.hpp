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

#include <brick-types>
#include <brick-hash>
#include <brick-hashset>
#include <unordered_set>

#include <divine/vm/value.hpp>
#include <divine/vm/shadow.hpp>
#include <divine/vm/types.hpp>

/*
 * Both MutableHeap and PersistentHeap implement the graph memory concept. That
 * is, they maintain a directed graph of objects induced by pointers. The
 * Mutable version can be used for a forward-only interpreter when speed or
 * compact memory footprint (or simplicity) are essential.
 *
 * The PersistentHeap implementation has a commit operation, which freezes the
 * current content of the heap; it can efficiently store a large number of
 * configurations using a hashed graph structure.
 *
 * Both implementations track bit-precise initialisation status and track
 * pointers exactly (that is, pointers are tagged out-of-band).
 */

namespace divine {
namespace vm {

namespace mem = brick::mem;

namespace heap {

template< typename H1, typename H2, typename MarkedComparer >
int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2,
             std::unordered_map< HeapPointer, int > &v1,
             std::unordered_map< HeapPointer, int > &v2, int &seq,
             MarkedComparer &markedComparer )
{
    r1.offset( 0 ); r2.offset( 0 );

    auto v1r1 = v1.find( r1 );
    auto v2r2 = v2.find( r2 );

    if ( v1r1 != v1.end() && v2r2 != v2.end() )
        return v1r1->second - v2r2->second;

    if ( v1r1 != v1.end() )
        return -1;
    if ( v2r2 != v2.end() )
        return 1;

    v1[ r1 ] = seq;
    v2[ r2 ] = seq;
    ++ seq;

    if ( h1.valid( r1 ) != h2.valid( r2 ) )
        return h1.valid( r1 ) - h2.valid( r2 );

    if ( !h1.valid( r1 ) )
        return 0;

    if ( h1.shared( r1 ) != h2.shared( r2 ) )
        return h1.shared( r1 ) - h2.shared( r2 );

    auto i1 = h1.ptr2i( r1 ), i2 = h2.ptr2i( r2 );
    int s1 = h1.size( r1, i1 ), s2 = h2.size( r2, i2 );
    if ( s1 - s2 )
        return s1 - s2;
    auto b1 = h1.unsafe_bytes( r1, i1 ), b2 = h2.unsafe_bytes( r2, i2 );
    auto p1 = h1.pointers( r1, i1 ), p2 = h2.pointers( r2, i2 );
    auto d1 = h1.defined( r1, i1 ), d2 = h2.defined( r2, i2 );
    int offset = 0;
    auto p1i = p1.begin(), p2i = p2.begin();
    while ( true )
    {
        if ( ( p1i == p1.end() ) ^ ( p2i == p2.end() ) )
            return p1i == p1.end() ? -1 : 1;

        if ( p1i != p1.end() )
        {
            if ( p1i->offset() != p2i->offset() )
                return p1i->offset() - p2i->offset();
            if ( p1i->size() != p2i->size() )
                return p1i->size() - p2i->size();
        }

        int end = p1i == p1.end() ? s1 : p1i->offset();
        while ( offset < end ) /* TODO definedness! */
        {
            if ( b1[ offset ] != b2[ offset ] )
                return b1[ offset ] - b2[ offset ];
            if ( d1[ offset ] != d2[ offset ] )
                return int( d1[ offset ] ) - int( d2[ offset ] );
            ++ offset;
        }

        if ( p1i == p1.end() )
            return 0;

        while ( offset < end + p1i->size() )
        {
            if ( d1[ offset ] != d2[ offset ] )
                return int( d1[ offset ] ) - int( d2[ offset ] );
            ++ offset;
        }

        /* recurse */
        value::Pointer p1p, p2p;
        ASSERT_EQ( p1i->offset(), p2i->offset() );
        r1.offset( p1i->offset() );
        r2.offset( p1i->offset() );
        h1.unsafe_read( r1, p1p, i1 );
        h2.unsafe_read( r2, p2p, i2 );
        int pdiff = 0;
        auto p1pp = p1p.cooked(), p2pp = p2p.cooked();
        if ( p1pp.type() == p2pp.type() )
        {
            if ( p1pp.offset() != p2pp.offset() )
                return p1pp.offset() - p2pp.offset();
            else if ( p1pp.type() == PointerType::Heap )
                pdiff = compare( h1, h2, p1pp, p2pp, v1, v2, seq, markedComparer );
            else if ( p1pp.type() == PointerType::Marked )
                markedComparer( p1pp, p2pp );
            else if ( p1pp.heap() ); // Weak
            else
                pdiff = p1pp.object() - p2pp.object();
        } else pdiff = int( p1pp.type() ) - int( p2pp.type() );
        if ( pdiff )
            return pdiff;
        ASSERT_EQ( p1i->size(), p2i->size() );
        ++ p1i; ++ p2i;
    }
    UNREACHABLE( "heap comparison fell through" );
}

template< typename Heap >
int hash( Heap &heap, HeapPointer root,
          std::unordered_map< int, int > &visited,
          brick::hash::jenkins::SpookyState &state, int depth )
{
    auto i = heap.ptr2i( root );
    int size = heap.size( root, i );
    uint32_t content_hash = i.tag();

    visited.emplace( root.object(), content_hash );

    if ( depth > 8 )
        return content_hash;

    if ( size > 16 * 1024 )
        return content_hash; /* skip following pointers in objects over 16k, not worth it */

    int ptr_data[2];
    auto pointers = heap.shadows().pointers(
            typename Heap::Shadows::Loc( i, 0 ), size );
    for ( auto pos : pointers )
    {
        value::Pointer ptr;
        ASSERT_LT( pos.offset(), heap.size( root ) );
        root.offset( pos.offset() );
        heap.unsafe_read( root, ptr, i );
        auto obj = ptr.cooked();
        ptr_data[1] = obj.offset();
        if ( obj.type() == PointerType::Heap )
        {
            auto vis = visited.find( obj.object() );
            if ( vis == visited.end() )
            {
                obj.offset( 0 );
                if ( heap.valid( obj ) )
                    ptr_data[0] = hash( heap, obj, visited, state, depth + 1 );
                else
                    ptr_data[0] = 0; /* freed object, ignore */
            }
            else
                ptr_data[0] = vis->second;
        }
        else if ( obj.heap() ); // Weak or Marked
        else
            ptr_data[0] = obj.object();
        state.update( ptr_data, sizeof( ptr_data ) );
    }

    return content_hash;
}

template< typename H1, typename H2, typename MarkedComparer >
int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2, MarkedComparer mc )
{
    std::unordered_map< HeapPointer, int > v1, v2;
    int seq = 0;
    return compare( h1, h2, r1, r2, v1, v2, seq, mc );
}

template< typename H1, typename H2 >
int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2 )
{
    return compare( h1, h2, r1, r2, []( HeapPointer, HeapPointer ) { } );
}

template< typename Heap >
brick::hash::hash128_t hash( Heap &heap, HeapPointer root )
{
    std::unordered_map< int, int > visited;
    brick::hash::jenkins::SpookyState state( 0, 0 );
    hash( heap, root, visited, state, 0 );
    return state.finalize();
}

enum class CloneType { All, SkipWeak, HeapOnly };

template< typename FromH, typename ToH >
HeapPointer clone( FromH &f, ToH &t, HeapPointer root,
                   std::map< HeapPointer, HeapPointer > &visited,
                   CloneType ct, bool overwrite )
{
    if ( root.null() )
        return root;
    if ( !f.valid( root ) )
        return root;
    auto seen = visited.find( root );
    if ( seen != visited.end() )
        return seen->second;

    auto root_i = f.ptr2i( root );
    if ( overwrite )
        t.free( root );
    auto result = t.make( f.size( root ), root.object(), true ).cooked();
    if ( overwrite )
        ASSERT_EQ( root.object(), result.object() );
    auto result_i = t.ptr2i( result );
    visited.emplace( root, result );

    t.copy( f, root, result, f.size( root ) );

    for ( auto pos : f.pointers( root ) )
    {
        value::Pointer ptr, ptr_c;
        GenericPointer cloned;
        root.offset( pos.offset() );
        result.offset( pos.offset() );
        f.read( root, ptr, root_i );
        auto obj = ptr.cooked();
        obj.offset( 0 );
        if ( obj.heap() )
            ASSERT( ptr.pointer() );
        if ( ct == CloneType::SkipWeak && obj.type() == PointerType::Weak )
            cloned = obj;
        else if ( ct == CloneType::HeapOnly && obj.type() != PointerType::Heap )
            cloned = obj;
        else if ( obj.heap() )
            cloned = clone( f, t, obj, visited, ct, overwrite );
        else
            cloned = obj;
        cloned.offset( ptr.cooked().offset() );
        t.write( result, value::Pointer( cloned ), result_i );
    }
    result.offset( 0 );
    return result;
}

template< typename FromH, typename ToH >
HeapPointer clone( FromH &f, ToH &t, HeapPointer root, CloneType ct = CloneType::All, bool over = false )
{
    std::map< HeapPointer, HeapPointer > visited;
    return clone( f, t, root, visited, ct, over );
}

}

struct HeapBytes
{
    uint8_t *_start, *_end;
    HeapBytes( uint8_t *start, uint8_t *end ) : _start( start ), _end( end ) {}
    int size() { return _end - _start; }
    uint8_t *begin() { return _start; }
    uint8_t *end() { return _end; }
    uint8_t &operator[]( int i ) { return *( _start + i ); }
};

template< typename Self, typename Shadows, typename Internal >
struct HeapMixin
{
    using PointerV = value::Pointer;

    Self &self() { return *static_cast< Self * >( this ); }
    const Self &self() const { return *static_cast< const Self * >( this ); }

    auto &shadows() { return self()._shadows; }
    const auto &shadows() const { return self()._shadows; }

    template< typename F >
    auto with_shadow( F f, HeapPointer p, int from, int sz, Internal i )
    {
        sz = sz ? sz : self().size( p, i ) - from;
        p.offset( from );
        auto shl = self().shloc( p, i );
        return (shadows().*f)( shl, sz );
    }

    auto pointers( HeapPointer p, Internal i, int from = 0, int sz = 0 )
    {
        return with_shadow( &Shadows::pointers, p, from, sz, i );
    }

    auto pointers( HeapPointer p, int from = 0, int sz = 0 )
    {
        return pointers( p, self().ptr2i( p ), from, sz );
    }

    auto defined( HeapPointer p, int from = 0, int sz = 0 )
    {
        return defined( p, self().ptr2i( p ), from, sz );
    }

    auto defined( HeapPointer p, Internal i, int from = 0, int sz = 0 )
    {
        return with_shadow( &Shadows::defined, p, from, sz, i );
    }

    auto type( HeapPointer p, int from = 0, int sz = 0 )
    {
        return with_shadow( &Shadows::type, p, from, sz, self().ptr2i( p ) );
    }

    template< typename T >
    void write_shift( PointerV &p, T t )
    {
        self().write( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    template< typename T >
    void read_shift( PointerV &p, T &t ) const
    {
        self().read( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    template< typename T >
    void read_shift( HeapPointer &p, T &t ) const
    {
        self().read( p, t );
        p = p + sizeof( typename T::Raw );
    }

    std::string read_string( HeapPointer ptr ) const
    {
        std::string str;
        value::Int< 8, false > c;
        unsigned sz = self().size( ptr );
        do {
            if ( ptr.offset() >= sz )
                return "<out of bounds>";
            read_shift( ptr, c );
            if ( c.cooked() )
                str += c.cooked();
        } while ( c.cooked() );
        return str;
    }

    void skip( PointerV &p, int bytes ) const
    {
        HeapPointer pv = p.cooked();
        pv.offset( pv.offset() + bytes );
        p.v( pv );
    }

    HeapBytes unsafe_bytes( HeapPointer p, Internal i, int off = 0, int sz = 0 ) const
    {
        sz = sz ? sz : self().size( p, i ) - off;
        ASSERT_LEQ( off + sz, self().size( p, i ) );
        auto start = self().unsafe_ptr2mem( p, i );
        return HeapBytes( start + off, start + off + sz );
    }

    HeapBytes unsafe_bytes( HeapPointer p, int off = 0, int sz = 0 ) const
    {
        return unsafe_bytes( p, self().ptr2i( p ), off, sz );
    }
};

template< typename Self, typename PR >
struct SimpleHeap : HeapMixin< Self, PooledShadow< mem::Pool< PR > >,
                               typename mem::Pool< PR >::Pointer >
{
    Self &self() { return *static_cast< Self * >( this ); }

    using ObjPool = mem::Pool< PR >;
    using SnapPool = mem::Pool< PR >;

    using Internal = typename ObjPool::Pointer;
    using Snapshot = typename SnapPool::Pointer;

    using Shadows = PooledShadow< ObjPool >;
    using PointerV = value::Pointer;
    using ShadowLoc = typename Shadows::Loc;

    struct SnapItem
    {
        uint32_t first;
        Internal second;
        operator std::pair< uint32_t, Internal >() { return std::make_pair( first, second ); }
        SnapItem( std::pair< const uint32_t, Internal > p ) : first( p.first ), second( p.second ) {}
        bool operator==( SnapItem si ) const { return si.first == first && si.second == second; }
    } __attribute__((packed));

    mutable ObjPool _objects;
    mutable SnapPool _snapshots;
    mutable Shadows _shadows;

    SimpleHeap() : _shadows( _objects ) {}

    mutable struct Local
    {
        std::map< uint32_t, Internal > exceptions;
        Snapshot snapshot;
    } _l;

    uint64_t objhash( Internal ) { return 0; }
    Internal detach( HeapPointer, Internal i ) { return i; }
    void made( HeapPointer ) {}

    void reset() { _l.exceptions.clear(); _l.snapshot = Snapshot(); }
    void rollback() { _l.exceptions.clear(); } /* fixme leak */

    ShadowLoc shloc( HeapPointer p, Internal i ) const
    {
        return ShadowLoc( i, p.offset() );
    }

    ShadowLoc shloc( HeapPointer p ) const { return shloc( p, ptr2i( p ) ); }

    uint8_t *unsafe_ptr2mem( HeapPointer, Internal i ) const
    {
        return _objects.template machinePointer< uint8_t >( i );
    }

    int snap_size( Snapshot s ) const
    {
        if ( !_snapshots.valid( s ) )
            return 0;
        return _snapshots.size( s ) / sizeof( SnapItem );
    }

    SnapItem *snap_begin( Snapshot s ) const
    {
        if ( !_snapshots.valid( s ) )
            return nullptr;
        return _snapshots.template machinePointer< SnapItem >( s );
    }

    SnapItem *snap_begin() const { return snap_begin( _l.snapshot ); }
    SnapItem *snap_end( Snapshot s ) const { return snap_begin( s ) + snap_size( s ); }
    SnapItem *snap_end() const { return snap_end( _l.snapshot ); }

    SnapItem *snap_find( uint32_t obj ) const
    {
        auto begin = snap_begin(), end = snap_end();
        if ( !begin )
            return nullptr;

        while ( begin < end )
        {
            auto pivot = begin + (end - begin) / 2;
            if ( pivot->first > obj )
                end = pivot;
            else if ( pivot->first < obj )
                begin = pivot + 1;
            else
            {
                ASSERT( _objects.valid( pivot->second ) );
                return pivot;
            }
        }

        return begin;
    }

    Internal ptr2i( HeapPointer p ) const
    {
        auto hp = _l.exceptions.find( p.object() );
        if ( hp != _l.exceptions.end() )
            return hp->second;

        auto si = snap_find( p.object() );
        return si && si != snap_end() && si->first == p.object() ? si->second : Internal();
    }

    PointerV make( int size, uint32_t hint = 1, bool overwrite = false )
    {
        HeapPointer p;
        SnapItem *search = snap_find( hint );
        bool found = false;
        while ( !found )
        {
            found = true;
            if ( _l.exceptions.count( hint ) )
                found = false;
            if ( search && search != snap_end() && search->first == hint )
                ++ search, found = false;
            if ( overwrite && _l.exceptions.count( hint ) )
                found = !_l.exceptions[ hint ].slab();
            if ( !found )
                ++ hint;
        }
        p.object( hint );
        p.offset( 0 );
        ASSERT( !ptr2i( p ).slab() );
        auto obj = _l.exceptions[ p.object() ] = _objects.allocate( size );
        _shadows.make( obj, size );
        self().made( p );
        return PointerV( p );
    }

    bool resize( HeapPointer p, int sz_new )
    {
        if ( p.offset() || !valid( p ) )
            return false;
        int sz_old = size( p );
        auto obj_old = ptr2i( p );
        auto obj_new = _objects.allocate( sz_new );
        _shadows.make( obj_new, sz_new );
        copy( *this, p, obj_old, p, obj_new, std::min( sz_new, sz_old ) );
        _l.exceptions[ p.object() ] = obj_new;
        self().made( p ); /* fixme? */
        return true;
    }

    bool free( HeapPointer p )
    {
        if ( !valid( p ) )
            return false;
        auto ex = _l.exceptions.find( p.object() );
        if ( ex == _l.exceptions.end() )
            _l.exceptions.emplace( p.object(), Internal() );
        else
        {
            _shadows.free( ex->second );
            _objects.free( ex->second );
            ex->second = Internal();
        }
        if ( p.offset() )
            return false;
        return true;
    }

    bool valid( HeapPointer p ) const
    {
        if ( !p.object() )
            return false;
        return ptr2i( p ).slab();
    }

    bool shared( HeapPointer p ) const
    {
        return _shadows.shared( ptr2i( p ) );
    }

    bool shared( GenericPointer gp, bool sh )
    {
        if ( !gp.heap() || !valid( gp ) )
            return false;

        HeapPointer p = gp;
        auto i = ptr2i( p );

        if ( _shadows.shared( i ) == sh )
            return false; /* nothing to be done */

        auto detached = self().detach( p, i );
        _shadows.shared( detached ) = sh;
        bool rv = i != detached;

        if ( !sh )
            return rv;

        for ( auto pos : this->pointers( p, detached ) ) /* flood */
        {
            value::Pointer ptr;
            p.offset( pos.offset() );
            read( p, ptr, detached );
            rv = shared( ptr.cooked(), true ) || rv;
        }

        return rv;
    }

    int size( HeapPointer, Internal i ) const { return _objects.size( i ); }
    int size( HeapPointer p ) const { return size( p, ptr2i( p ) ); }

    void write( HeapPointer, value::Void ) {}
    void write( HeapPointer, value::Void, Internal ) {}
    void read( HeapPointer, value::Void& ) const {}
    void read( HeapPointer, value::Void&, Internal ) const {}

    template< typename T >
    void read( HeapPointer p, T &t, Internal i ) const
    {
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p, i ) - p.offset() );

        t.raw( *_objects.template machinePointer< typename T::Raw >( i, p.offset() ) );
        _shadows.read( shloc( p, i ), t );
    }

    template< typename T >
    T *unsafe_deref( HeapPointer p, Internal i ) const
    {
        return _objects.template machinePointer< T >( i, p.offset() );
    }

    template< typename T >
    void unsafe_read( HeapPointer p, T &t, Internal i ) const
    {
        t.raw( *unsafe_deref< typename T::Raw >( p, i ) );
    }

    template< typename T >
    void read( HeapPointer p, T &t ) const { read( p, t, ptr2i( p ) ); }

    template< typename T >
    Internal write( HeapPointer p, T t, Internal i )
    {
        i = self().detach( p, i );
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p, i ) - p.offset() );
        _shadows.write( shloc( p, i ), t );
        *_objects.template machinePointer< typename T::Raw >( i, p.offset() ) = t.raw();
        if ( t.pointer() && shared( p ) )
            if ( shared( value::Pointer( t ).cooked(), true ) )
                return Internal();
        return i;
    }

    template< typename T >
    void write( HeapPointer p, T t ) { write( p, t, ptr2i( p ) ); }

    template< typename FromH >
    bool copy( FromH &from_h, HeapPointer _from, typename FromH::Internal _from_i,
               HeapPointer _to, Internal &_to_i, int bytes )
    {
        if ( _from.null() || _to.null() )
            return false;
        _to_i = self().detach( _to, _to_i );
        int from_s( from_h.size( _from, _from_i ) ), to_s( size( _to, _to_i ) );
        auto from = from_h.unsafe_bytes( _from, _from_i, 0, from_s ),
               to = self().unsafe_bytes( _to, _to_i, 0, to_s );
        int from_off( _from.offset() ), to_off( _to.offset() );
        if ( !from.begin() || !to.begin() || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;
        std::copy( from.begin() + from_off, from.begin() + from_off + bytes, to.begin() + to_off );
        _shadows.copy( from_h.shadows(), from_h.shloc( _from, _from_i ),
                       shloc( _to, _to_i ), bytes );

        if ( _shadows.shared( _to_i ) )
            for ( auto pos : _shadows.pointers( shloc( _to, _to_i ), bytes ) )
            {
                value::Pointer ptr;
                _to.offset( to_off + pos.offset() );
                read( _to, ptr, _to_i );
                shared( ptr.cooked(), true );
            }

        return true;
    }

    template< typename FromH >
    bool copy( FromH &from_h, HeapPointer _from, HeapPointer _to, int bytes )
    {
        auto _to_i = ptr2i( _to );
        return copy( from_h, _from, from_h.ptr2i( _from ), _to, _to_i, bytes );
    }

    bool copy( HeapPointer f, HeapPointer t, int b ) { return copy( *this, f, t, b ); }

    void restore( Snapshot ) { UNREACHABLE( "restore() is not available in SimpleHeap" ); }
    Snapshot snapshot() { UNREACHABLE( "snapshot() is not available in SimpleHeap" ); }
};

struct CowHeap : SimpleHeap< CowHeap >
{
    using Super = SimpleHeap< CowHeap >;

    struct ObjHasher
    {
        CowHeap *_heap;
        auto &objects() { return _heap->_objects; }
        auto &shadows() { return _heap->_shadows; }

        auto content_only( Internal i )
        {
            auto size = objects().size( i );
            auto base = objects().dereference( i );

            brick::hash::jenkins::SpookyState high( 0, 0 );

            auto types = shadows().type( ShadowLoc( i, 0 ), size );
            auto t = types.begin();
            int offset = 0;

            while ( offset + 4 <= size )
            {
                if ( *t != ShadowType::Pointer ) /* NB. assumes little endian */
                    high.update( base + offset, 4 );
                offset += 4;
                t += 4;
            }

            high.update( base + offset, size - offset );
            ASSERT_LEQ( offset, size );

            return high.finalize().first;
        }

        auto hash( Internal i )
        {
            /* TODO also hash some shadow data into low for better precision? */
            auto low = brick::hash::spooky( objects().dereference( i ), objects().size( i ) );
            return std::make_pair( ( content_only( i ) & 0xFFFFFFF000000000 ) | /* high 28 bits */
                                   ( low.first & 0x0000000FFFFFFFF ), low.second );
        }

        bool equal( Internal a, Internal b )
        {
            int size = objects().size( a );
            if ( objects().size( b ) != size )
                return false;
            if ( ::memcmp( objects().dereference( a ), objects().dereference( b ), size ) )
                return false;
            if ( shadows().shared( a ) != shadows().shared( b ) )
                return false;
            ShadowLoc a_shloc( a, 0 ), b_shloc( b, 0 );
            if ( !shadows().equal( a_shloc, b_shloc, size ) )
                return false;
            return true;
        }
    };

    auto objhash( Internal i ) { return _ext.objects.hasher.content_only( i ); }

    mutable struct Ext
    {
        std::unordered_set< int > writable;
        brick::hashset::Concurrent< Internal, ObjHasher > objects;
    } _ext;

    void setupHT() { _ext.objects.hasher._heap = this; }

    CowHeap() { setupHT(); }
    CowHeap( const CowHeap &o ) : Super( o ), _ext( o._ext )
    {
        setupHT();
        restore( o.snapshot() );
    }

    CowHeap &operator=( const CowHeap &o )
    {
        Super::operator=( o );
        _ext = o._ext;
        setupHT();
        restore( o.snapshot() );
        return *this;
    }

    void made( HeapPointer p )
    {
        _ext.writable.insert( p.object() );
    }

    void reset()
    {
        Super::reset();
        _ext.writable.clear();
    }

    void rollback()
    {
        Super::rollback();
        _ext.writable.clear();
    }

    Internal detach( HeapPointer p, Internal i )
    {
        if ( _ext.writable.count( p.object() ) )
            return i;
        ASSERT_EQ( _l.exceptions.count( p.object() ), 0 );
        _ext.writable.insert( p.object() );
        p.offset( 0 );
        int sz = size( p, i );
        auto oldloc = shloc( p, i );
        auto oldbytes = unsafe_bytes( p, i, 0, sz );

        auto obj = _objects.allocate( sz );
        _shadows.make( obj, sz );

        _l.exceptions[ p.object() ] = obj;
        auto newloc = shloc( p, obj );
        auto newbytes = unsafe_bytes( p, obj, 0, sz );
        _shadows.copy( _shadows, oldloc, newloc, sz );
        std::copy( oldbytes.begin(), oldbytes.end(), newbytes.begin() );
        _shadows.shared( obj ) = _shadows.shared( i );
        return obj;
    }

    SnapItem dedup( SnapItem si ) const
    {
        auto r = _ext.objects.insert( si.second );
        if ( !r.isnew() )
        {
            ASSERT_NEQ( *r, si.second );
            _shadows.free( si.second );
            _objects.free( si.second );
        }
        si.second = *r;
        return si;
    }

    Snapshot snapshot() const
    {
        int count = 0;

        if ( _l.exceptions.empty() )
            return _l.snapshot;

        auto snap = snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != snap_end() && snap->first < except.first )
                ++ snap, ++ count;
            if ( snap != snap_end() && snap->first == except.first )
                snap++;
            if ( _objects.valid( except.second ) )
                ++ count;
        }

        while ( snap != snap_end() )
            ++ snap, ++ count;

        if ( !count )
            return Snapshot();

        auto s = _snapshots.allocate( count * sizeof( SnapItem ) );
        auto si = _snapshots.template machinePointer< SnapItem >( s );
        snap = snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != snap_end() && snap->first < except.first )
                *si++ = *snap++;
            if ( snap != snap_end() && snap->first == except.first )
                snap++;
            if ( _objects.valid( except.second ) )
                *si++ = dedup( except );
        }

        while ( snap != snap_end() )
            *si++ = *snap++;

        auto newsnap = _snapshots.template machinePointer< SnapItem >( s );
        ASSERT_EQ( si, newsnap + count );
        for ( auto s = newsnap; s < newsnap + count; ++s )
            ASSERT( _objects.valid( s->second ) );

        _l.exceptions.clear();
        _ext.writable.clear();
        _l.snapshot = s;

        return s;
    }

    bool is_shared( Snapshot s ) const { return s == _l.snapshot; }
    void restore( Snapshot s )
    {
        _l.exceptions.clear();
        _ext.writable.clear();
        _l.snapshot = s;
    }
};

}

namespace t_vm {

struct MutableHeap
{
    using IntV = vm::value::Int< 32, true >;
    using PointerV = vm::value::Pointer;

    TEST(alloc)
    {
        vm::SmallHeap heap;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), IntV( 10 ) );
        IntV q;
        heap.read( p.cooked(), q );
        ASSERT_EQ( q.cooked(), 10 );
    }

    TEST(conversion)
    {
        vm::SmallHeap heap;
        auto p = heap.make( 16 );
        ASSERT_EQ( vm::HeapPointer( p.cooked() ),
                   vm::HeapPointer( vm::GenericPointer( p.cooked() ) ) );
    }

    TEST(write_read)
    {
        vm::SmallHeap heap;
        PointerV p, q;
        p = heap.make( 16 );
        heap.write( p.cooked(), p );
        heap.read( p.cooked(), q );
        ASSERT( p.cooked() == q.cooked() );
    }

    TEST(resize)
    {
        vm::SmallHeap heap;
        PointerV p, q;
        p = heap.make( 16 );
        heap.write( p.cooked(), p );
        heap.resize( p.cooked(), 8 );
        heap.read( p.cooked(), q );
        ASSERT_EQ( p.cooked(), q.cooked() );
        ASSERT_EQ( heap.size( p.cooked() ), 8 );
    }

    TEST(pointers)
    {
        vm::SmallHeap heap;
        auto p = heap.make( 16 ), q = heap.make( 16 ), r = PointerV( vm::nullPointer() );
        heap.write( p.cooked(), q );
        heap.write( q.cooked(), r );
        int count = 0;
        for ( auto pos : heap.pointers( p.cooked() ) )
            static_cast< void >( pos ), ++ count;
        ASSERT_EQ( count, 1 );
    }

    TEST(clone_int)
    {
        vm::SmallHeap heap, cloned;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), IntV( 33 ) );
        auto c = vm::heap::clone( heap, cloned, p.cooked() );
        IntV i;
        cloned.read( c, i );
        ASSERT( ( i == IntV( 33 ) ).cooked() );
        ASSERT( ( i == IntV( 33 ) ).defined() );
    }

    TEST(clone_ptr_chain)
    {
        vm::SmallHeap heap, cloned;
        auto p = heap.make( 16 ), q = heap.make( 16 ), r = PointerV( vm::nullPointer() );
        heap.write( p.cooked(), q );
        heap.write( q.cooked(), r );

        auto c_p = vm::heap::clone( heap, cloned, p.cooked() );
        PointerV c_q, c_r;
        cloned.read( c_p, c_q );
        ASSERT( c_q.pointer() );
        cloned.read( c_q.cooked(), c_r );
        ASSERT( c_r.cooked().null() );
    }

    TEST(clone_ptr_loop)
    {
        vm::SmallHeap heap, cloned;
        auto p = heap.make( 16 ), q = heap.make( 16 );
        heap.write( p.cooked(), q );
        heap.write( q.cooked(), p );
        auto c_p1 = vm::heap::clone( heap, cloned, p.cooked() );
        PointerV c_q, c_p2;
        cloned.read( c_p1, c_q );
        ASSERT( c_q.pointer() );
        cloned.read( c_q.cooked(), c_p2 );
        ASSERT( vm::GenericPointer( c_p1 ) == c_p2.cooked() );
    }

    TEST(compare)
    {
        vm::SmallHeap heap, cloned;
        auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
        heap.write( p, PointerV( q ) );
        heap.write( q, PointerV( p ) );
        auto c_p = vm::heap::clone( heap, cloned, p );
        ASSERT_EQ( vm::heap::compare( heap, cloned, p, c_p ), 0 );
        p.offset( 8 );
        heap.write( p, vm::value::Int< 32 >( 1 ) );
        ASSERT_LT( 0, vm::heap::compare( heap, cloned, p, c_p ) );
    }

    TEST(hash)
    {
        vm::SmallHeap heap, cloned;
        auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
        heap.write( p, PointerV( q ) );
        heap.write( p + vm::PointerBytes, PointerV( p ) );
        heap.write( q, PointerV( p ) );
        auto c_p = vm::heap::clone( heap, cloned, p );
        ASSERT_EQ( vm::heap::hash( heap, p ).first,
                   vm::heap::hash( cloned, c_p ).first );
    }

    TEST(cow_hash)
    {
        vm::CowHeap heap;
        auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
        heap.write( p, PointerV( q ) );
        heap.write( p + vm::PointerBytes, IntV( 5 ) );
        heap.write( q, PointerV( p ) );
        heap.snapshot();
        ASSERT( vm::heap::hash( heap, p ).first !=
                vm::heap::hash( heap, q ).first );
    }

};

struct CowHeap
{
    using IntV = vm::value::Int< 32, true >;
    using PointerV = vm::value::Pointer;

    TEST(basic)
    {
        vm::CowHeap heap;
        auto p = heap.make( 16 ).cooked();
        heap.write( p, PointerV( p ) );
        auto snap = heap.snapshot();
        heap.write( p, PointerV( vm::nullPointer() ) );
        PointerV check;
        heap.restore( snap );
        heap.read( p, check );
        ASSERT_EQ( check.cooked(), p );
    }
};

}

}
