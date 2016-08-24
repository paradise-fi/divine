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
#include <divine/vm/value.hpp>
#include <divine/vm/shadow.hpp>

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

template< typename H1, typename H2 >
int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2, std::set< HeapPointer > &visited )
{
    if ( visited.count( r1 ) )
        return 0;
    visited.insert( r1 );

    if ( h1.valid( r1 ) != h2.valid( r2 ) )
        return h1.valid( r1 ) - h2.valid( r2 );
    if ( !h1.valid( r1 ) )
        return 0;

    int s1 = h1.size( r1 ), s2 = h2.size( r2 );
    if ( s1 - s2 )
        return s1 - s2;
    r1.offset( 0 ); r2.offset( 0 );
    auto b1 = h1.unsafe_bytes( r1 ), b2 = h2.unsafe_bytes( r2 );
    auto p1 = h1.pointers( r1 ), p2 = h2.pointers( r2 );
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
            ++ offset;
        }

        if ( p1i == p1.end() )
            return 0;

        /* recurse */
        value::Pointer p1p, p2p;
        r1.offset( p1i->offset() );
        r2.offset( p1i->offset() );
        h1.read( r1, p1p );
        h2.read( r2, p2p );
        int pdiff = 0;
        auto p1pp = p1p.cooked(), p2pp = p2p.cooked();
        if ( p1pp.type() == p2pp.type() )
        {
            if ( p1pp.type() == PointerType::Heap )
                pdiff = compare( h1, h2, p1pp, p2pp, visited );
            else if ( p1pp.object() == p2pp.object() )
                pdiff = p1pp.offset() - p2pp.offset();
            else
                pdiff = p1pp.object() - p2pp.object();
        } else pdiff = int( p1pp.type() ) - int( p2pp.type() );
        if ( pdiff )
            return pdiff;
        offset += p1i->size();
        ++ p1i; ++ p2i;
    }
    UNREACHABLE( "heap comparison fell through" );
}

template< typename H1, typename H2 >
int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2 )
{
    std::set< HeapPointer > visited;
    return compare( h1, h2, r1, r2, visited );
}

template< typename FromH, typename ToH >
HeapPointer clone( FromH &f, ToH &t, HeapPointer root,
                   std::map< HeapPointer, HeapPointer > &visited )
{
    if ( root.null() )
        return root;
    auto seen = visited.find( root );
    if ( seen != visited.end() )
        return seen->second;

    auto result = t.make( f.size( root ) ).cooked();
    visited.emplace( root, result );
    auto fb = f.unsafe_bytes( root ), tb = t.unsafe_bytes( result );
    auto fd = f.defined( root ), td = t.defined( result );

    for ( int i  = 0; i < f.size( root ); ++i ) /* TODO speed */
    {
        tb[ i ] = fb[ i ];
        td[ i ] = fd[ i ];
    }

    for ( auto pos : f.pointers( root ) )
    {
        value::Pointer ptr, ptr_c;
        root.offset( pos.offset() );
        result.offset( pos.offset() );
        f.read( root, ptr );
        auto obj = ptr.cooked();
        obj.offset( 0 );
        auto cloned = obj.type() == PointerType::Heap ? clone( f, t, obj, visited ) : obj;
        cloned.offset( ptr.cooked().offset() );
        t.write( result, value::Pointer( cloned ) );
    }
    result.offset( 0 );
    return result;
}

template< typename FromH, typename ToH >
HeapPointer clone( FromH &f, ToH &t, HeapPointer root )
{
    std::map< HeapPointer, HeapPointer > visited;
    return clone( f, t, root, visited );
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

template< typename Self >
struct HeapMixin
{
    using PointerV = value::Pointer;

    Self &self() { return *static_cast< Self * >( this ); }

    auto &shadows() { return self()._shadows; }
    auto shloc( HeapPointer &p, int &from, int &sz )
    {
        sz = sz ? sz : self().size( p ) - from;
        p.offset( from );
        return self().shloc( p );
    }

    auto pointers( HeapPointer p, int from = 0, int sz = 0 )
    {
        return shadows().pointers( shloc( p, from, sz ), sz );
    }

    auto defined( HeapPointer p, int from = 0, int sz = 0 )
    {
        return shadows().defined( shloc( p, from, sz ), sz );
    }

    auto type( HeapPointer p, int from = 0, int sz = 0 )
    {
        return shadows().type( shloc( p, from, sz ), sz );
    }

    template< typename T >
    void write_shift( PointerV &p, T t )
    {
        self().write( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    template< typename T >
    void read_shift( PointerV &p, T &t )
    {
        self().read( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    std::string read_string( PointerV ptr )
    {
        std::string str;
        value::Int< 8, false > c;
        unsigned sz = self().size( ptr.cooked() );
        do {
            if ( ptr.cooked().offset() >= sz )
                return "<out of bounds>";
            read_shift( ptr, c );
            if ( c.cooked() )
                str += c.cooked();
        } while ( c.cooked() );
        return str;
    }

    void skip( PointerV &p, int bytes )
    {
        HeapPointer pv = p.cooked();
        pv.offset( pv.offset() + bytes );
        p.v( pv );
    }

    HeapBytes unsafe_bytes( HeapPointer p, int off = 0, int sz = 0 )
    {
        sz = sz ? sz : self().size( p ) - off;
        ASSERT_LEQ( off + sz, self().size( p ) );
        auto start = self().unsafe_ptr2mem( p );
        return HeapBytes( start + off, start + off + sz );
    }
};

struct SimpleHeapShared
{
    std::atomic< int > seq;
};

template< typename Self, typename Shared >
struct SimpleHeap : HeapMixin< Self >
{
    Self &self() { return *static_cast< Self * >( this ); }

    using Pool = mem::Pool<>;
    using Internal = Pool::Pointer;
    using Shadows = PooledShadow< Internal >;
    using PointerV = value::Pointer;

    Pool _objects;
    Shadows _shadows;

    struct Local
    {
        std::map< int, mem::Pool<>::Pointer > objmap;
    } _l;

    std::shared_ptr< Shared > _s;

    void detach( HeapPointer ) {}

    SimpleHeap() { _s = std::make_shared< Shared >(); _s->seq = 1; }

    Shadows::Loc shloc( HeapPointer p )
    {
        return Shadows::Loc( ptr2i( p ), Shadows::Anchor(), p.offset() );
    }

    uint8_t *unsafe_ptr2mem( HeapPointer p )
    {
        return _objects.machinePointer< uint8_t >( ptr2i( p ) );
    }

    Internal ptr2i( HeapPointer p )
    {
        auto hp = _l.objmap.find( p.object() );
        ASSERT( hp != _l.objmap.end() );
        return hp->second;
    }

    PointerV make( int size )
    {
        HeapPointer p;
        p.object( _s->seq++ );
        p.offset( 0 );
        auto obj = _l.objmap[ p.object() ] = _objects.allocate( size );
        _shadows.make( _objects, obj, size );
        return PointerV( p );
    }

    bool free( HeapPointer p )
    {
        if ( !valid( p ) )
            return false;
        _l.objmap.erase( p.object() );
        return true;
    }

    bool valid( HeapPointer p ) { return p.object() && int( p.object() ) < _s->seq &&
                                         _l.objmap.count( p.object() ); }

    int size( HeapPointer p ) { return _objects.size( ptr2i( p ) ); }

    void write( HeapPointer, value::Void ) {}
    void read( HeapPointer, value::Void& ) {}

    template< typename T >
    void read( HeapPointer p, T &t )
    {
        using Raw = typename T::Raw;
        ASSERT( valid( p ) );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - p.offset() );

        t.raw( *_objects.machinePointer< typename T::Raw >( ptr2i( p ), p.offset() ) );
        _shadows.read( shloc( p ), t );
    }

    template< typename T >
    void write( HeapPointer p, T t )
    {
        self().detach( p );
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - p.offset() );
        _shadows.write( shloc( p ), t, []( auto, auto ) {} );
        *_objects.machinePointer< typename T::Raw >( ptr2i( p ), p.offset() ) = t.raw();
    }

    template< typename FromH >
    bool copy( FromH &from_h, HeapPointer _from, HeapPointer _to, int bytes )
    {
        if ( _from.null() || _to.null() )
            return false;
        self().detach( _to );
        auto from = from_h.unsafe_bytes( _from ), to = self().unsafe_bytes( _to );
        int from_s( from_h.size( _from ) ), to_s( size( _to ) );
        int from_off( _from.offset() ), to_off( _to.offset() );
        if ( !from.begin() || !to.begin() || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;
        std::copy( from.begin() + from_off, from.begin() + from_off + bytes, to.begin() + to_off );
        _shadows.copy( from_h.shadows(), from_h.shloc( _from ),
                       shloc( _to ), bytes,  []( auto, auto ) {} );
        return true;
    }

    bool copy( HeapPointer f, HeapPointer t, int b ) { return copy( *this, f, t, b ); }
};

struct MutableHeap : SimpleHeap< MutableHeap, SimpleHeapShared >
{
};

struct CowHeap : SimpleHeap< CowHeap, SimpleHeapShared >
{
    struct Ext
    {
        std::set< int > readonly;
    } _ext;

    using SnapItem = std::pair< int, mem::Pool<>::Pointer >;
    using Snapshot = mem::Pool<>::Pointer;

    void detach( HeapPointer p )
    {
        if ( _ext.readonly.count( p.object() ) == 0 )
            return;
        p.offset( 0 );
        int sz = size( p );
        auto oldloc = shloc( p );
        auto oldbytes = unsafe_bytes( p );

        auto obj = _objects.allocate( sz );
        _shadows.make( _objects, obj, sz );

        _l.objmap[ p.object() ] = obj;
        auto newloc = shloc( p );
        auto newbytes = unsafe_bytes( p );
        _shadows.copy( _shadows, oldloc, newloc, sz, []( auto, auto ) {} );
        std::copy( oldbytes.begin(), oldbytes.end(), newbytes.begin() );
    }

    Snapshot snapshot()
    {
        _ext.readonly.clear(); /* need to clean up at some point, so why not now */
        auto s = _objects.allocate( _l.objmap.size() * sizeof( SnapItem ) );
        auto si = _objects.machinePointer< SnapItem >( s );
        for ( auto i : _l.objmap )
        {
            _ext.readonly.insert( i.first );
            *si++ = i;
        }
        return s;
    }

    void restore( Snapshot s )
    {
        _l.objmap.clear();
        _ext.readonly.clear();
        auto si = _objects.machinePointer< SnapItem >( s );
        int count = _objects.size( s ) / sizeof( SnapItem );
        for ( int i = 0; i < count; ++i )
        {
            _ext.readonly.insert( si[ i ].first );
            _l.objmap.insert( si[ i ] );
        }
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
        vm::MutableHeap heap;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), IntV( 10 ) );
        IntV q;
        heap.read( p.cooked(), q );
        ASSERT_EQ( q.cooked(), 10 );
    }

    TEST(conversion)
    {
        vm::MutableHeap heap;
        auto p = heap.make( 16 );
        ASSERT_EQ( vm::HeapPointer( p.cooked() ),
                   vm::HeapPointer( vm::ConstPointer( p.cooked() ) ) );
    }

    TEST(write_read)
    {
        vm::MutableHeap heap;
        PointerV p, q;
        p = heap.make( 16 );
        heap.write( p.cooked(), p );
        heap.read( p.cooked(), q );
        ASSERT( p.cooked() == q.cooked() );
    }

    TEST(clone_int)
    {
        vm::MutableHeap heap, cloned;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), IntV( 33 ) );
        auto c = vm::clone( heap, cloned, p.cooked() );
        IntV i;
        cloned.read( c, i );
        ASSERT( ( i == IntV( 33 ) ).cooked() );
        ASSERT( ( i == IntV( 33 ) ).defined() );
    }

    TEST(clone_ptr)
    {
        vm::MutableHeap heap, cloned;
        auto p = heap.make( 16 ), q = heap.make( 16 );
        heap.write( p.cooked(), q );
        heap.write( q.cooked(), p );
        auto c_p1 = vm::clone( heap, cloned, p.cooked() );
        PointerV c_q, c_p2;
        IntV i;
        cloned.read( c_p1, c_q );
        cloned.read( c_q.cooked(), c_p2 );
        ASSERT( vm::GenericPointer( c_p1 ) == c_p2.cooked() );
    }

    TEST(compare)
    {
        vm::MutableHeap heap, cloned;
        auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
        heap.write( p, PointerV( q ) );
        heap.write( q, PointerV( p ) );
        auto c_p = vm::clone( heap, cloned, p );
        ASSERT_EQ( vm::compare( heap, cloned, p, c_p ), 0 );
        p.offset( 8 );
        heap.write( p, vm::value::Int< 32 >( 1 ) );
        ASSERT_LT( 0, vm::compare( heap, cloned, p, c_p ) );
    }

    TEST(cow)
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
