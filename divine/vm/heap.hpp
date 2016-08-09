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
int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H2::Pointer r2,
             std::set< typename H1::Pointer > &visited )
{
    if ( visited.count( r1 ) )
        return 0;
    visited.insert( r1 );
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
        typename H1::PointerV p1p;
        typename H2::PointerV p2p;
        r1.offset( p1i->offset() );
        r2.offset( p1i->offset() );
        h1.read( r1, p1p );
        h2.read( r2, p2p );
        int pdiff = compare( h1, h2, p1p.cooked(), p2p.cooked(), visited );
        if ( pdiff )
            return pdiff;
        offset += p1i->size();
        ++ p1i; ++ p2i;
    }
    UNREACHABLE( "heap comparison fell through" );
}

template< typename H1, typename H2 >
int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H2::Pointer r2 )
{
    std::set< typename H1::Pointer > visited;
    return compare( h1, h2, r1, r2, visited );
}

template< typename FromH, typename ToH >
typename ToH::Pointer clone( FromH &f, ToH &t, typename FromH::Pointer root,
                             std::map< typename FromH::Pointer, typename ToH::Pointer > &visited )
{
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
        typename FromH::PointerV ptr;
        typename ToH::PointerV ptr_c;
        root.offset( pos.offset() );
        result.offset( pos.offset() );
        f.read( root, ptr );
        auto obj = ptr.cooked();
        obj.offset( 0 );
        auto cloned = clone( f, t, obj, visited );
        cloned.offset( ptr.cooked().offset() );
        t.write( result, typename ToH::PointerV( cloned ) );
    }
    return result;
}

template< typename FromH, typename ToH >
typename ToH::Pointer clone( FromH &f, ToH &t, typename FromH::Pointer root )
{
    std::map< typename FromH::Pointer, typename ToH::Pointer > visited;
    return clone( f, t, root, visited );
}

struct MutableHeap
{
    /*
     * The evaluator-facing pointer structure is laid over the Internal
     * (pool-derived) pointer type. The Internal pointer has tag bits at the
     * start, so that the type tag of GenericPointer and the offset are stored
     * within the tag area of the Pool Pointer.
     */
    struct Pointer : GenericPointer
    {
        bool null() { return object() == 0 && offset() == 0; }
        Pointer() : GenericPointer( PointerType::Heap ) {}
        Pointer operator+( int off ) const
        {
            Pointer r = *this;
            r.offset( r.offset() + off );
            return r;
        }
    };

    using Pool = mem::Pool<>;
    using Internal = Pool::Pointer;
    using Shadows = MutableShadow< Internal >;
    using PointerV = value::Pointer< Pointer >;

    Pool _objects;
    Shadows _shadows;

    Shadows &shadows() { return _shadows; }

    struct Bytes
    {
        Internal _p;
        Pool &_o;
        int _start, _end;
        Bytes( Pool &o, Internal p, int off, int end ) : _o( o ), _p( p ), _start( off ), _end( end ) {}
        int size() { return _end - _start; }
        uint8_t *begin() { return _o.machinePointer< uint8_t >( _p, _start ); }
        uint8_t *end() { return _o.machinePointer< uint8_t >( _p, _end ); }
        uint8_t &operator[]( int i ) { return *( begin() + i ); }
    };

    Internal p2i( Pointer p ) { Internal i; i.raw( p.object() ); return i; }
    Shadows::Loc shloc( Pointer p ) { return Shadows::Loc( p2i( p ), Shadows::Anchor(), p.offset() ); }
    Shadows::Loc shloc( Pointer &p, int &from, int &sz )
    {
        sz = sz ?: size( p ) - from;
        p.offset( from );
        return shloc( p );
    }

    auto pointers( Pointer p, int from = 0, int sz = 0 )
    {
        return shadows().pointers( shloc( p, from, sz ), sz );
    }

    auto defined( Pointer p, int from = 0, int sz = 0 )
    {
        return shadows().defined( shloc( p, from, sz ), sz );
    }

    auto type( Pointer p, int from = 0, int sz = 0 )
    {
        return shadows().type( shloc( p, from, sz ), sz );
    }

    PointerV make( int size )
    {
        auto i = _objects.allocate( size );
        _shadows.make( _objects, i, size );

        Pointer p;
        p.object( i.raw() );
        p.offset( 0 );
        return PointerV( p );
    }

    bool free( Pointer p )
    {
        auto i = p2i( p );
        if ( !_objects.valid( i ) )
            return false;
        _shadows.free( shloc( p ) );
        _objects.free( i );
        return true;
    }

    bool valid( Pointer p ) { return _objects.valid( p2i( p ) ); }
    int size( Pointer p ) { return _objects.size( p2i( p ) ); }

    void write( Pointer, value::Void ) {}
    void read( Pointer, value::Void& ) {}

    template< typename T >
    void read( Pointer p, T &t )
    {
        using Raw = typename T::Raw;
        ASSERT( valid( p ) );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - p.offset() );

        t.raw( *_objects.machinePointer< typename T::Raw >( p2i( p ), p.offset() ) );
        _shadows.read( shloc( p ), t );
    }

    template< typename T >
    void write( Pointer p, T t )
    {
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - p.offset() );
        _shadows.write( shloc( p ), t, []( auto, auto ) {} );
        *_objects.machinePointer< typename T::Raw >( p2i( p ), p.offset() ) = t.raw();
    }

    template< typename T >
    void write_shift( PointerV &p, T t )
    {
        write( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    template< typename T >
    void read_shift( PointerV &p, T &t )
    {
        read( p.cooked(), t );
        skip( p, sizeof( typename T::Raw ) );
    }

    void skip( PointerV &p, int bytes )
    {
        Pointer pv = p.cooked();
        pv.offset( pv.offset() + bytes );
        p.v( pv );
    }

    auto unsafe_bytes( Pointer p, int off = 0, int sz = 0 )
    {
        return Bytes( _objects, p2i( p ), off, sz ? off + sz : size( p ) );
    }

    template< typename FromH >
    bool copy( FromH &from_h, Pointer _from, Pointer _to, int bytes )
    {
        auto from = from_h.unsafe_bytes( _from ), to = unsafe_bytes( _to );
        if ( _from.null() || _to.null() )
            return false;
        int from_s( from_h.size( _from ) ), to_s( size( _to ) );
        int from_off( _from.offset() ), to_off( _to.offset() );
        if ( !from.begin() || !to.begin() || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;
        std::copy( from.begin() + from_off, from.begin() + from_off + bytes, to.begin() + to_off );
        _shadows.copy( from_h.shadows(), from_h.shloc( _from ),
                       shloc( _to ), bytes,  []( auto, auto ) {} );
        return true;
    }

    bool copy( Pointer f, Pointer t, int b ) { return copy( *this, f, t, b ); }
};

static inline std::ostream &operator<<( std::ostream &o, MutableHeap::Pointer p )
{
    return o << "[heap " << std::hex << p.object() << std::dec << " " << p.offset() << "]";
}

struct PersistentHeap
{
    // ShapeStore, DataStore, commit(), ...
};

}

namespace t_vm {

struct MutableHeap
{
    using I = vm::value::Int< 32, true >;

    TEST(alloc)
    {
        vm::MutableHeap heap;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), I( 10 ) );
        I q;
        heap.read( p.cooked(), q );
        ASSERT_EQ( q.cooked(), 10 );
    }

    TEST(conversion)
    {
        vm::MutableHeap heap;
        using HP = vm::MutableHeap::Pointer;
        auto p = heap.make( 16 );
        ASSERT_EQ( HP( p.cooked() ), HP( vm::ConstPointer( p.cooked() ) ) );
    }

    TEST(write_read)
    {
        vm::MutableHeap heap;
        vm::MutableHeap::PointerV p, q;
        p = heap.make( 16 );
        heap.write( p.cooked(), p );
        heap.read( p.cooked(), q );
        ASSERT( p.cooked() == q.cooked() );
    }

    TEST(clone_int)
    {
        vm::MutableHeap heap, cloned;
        auto p = heap.make( 16 );
        heap.write( p.cooked(), I( 33 ) );
        auto c = vm::clone( heap, cloned, p.cooked() );
        I i;
        cloned.read( c, i );
        ASSERT( ( i == I( 33 ) ).cooked() );
        ASSERT( ( i == I( 33 ) ).defined() );
    }

    TEST(clone_ptr)
    {
        vm::MutableHeap heap, cloned;
        auto p = heap.make( 16 ), q = heap.make( 16 );
        heap.write( p.cooked(), q );
        heap.write( q.cooked(), p );
        auto c_p1 = vm::clone( heap, cloned, p.cooked() );
        vm::MutableHeap::PointerV c_q, c_p2;
        I i;
        cloned.read( c_p1, c_q );
        cloned.read( c_q.cooked(), c_p2 );
        ASSERT( vm::GenericPointer( c_p1 ) == c_p2.cooked() );
    }

    TEST(compare)
    {
        using PointerV = vm::MutableHeap::PointerV;
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
};

}

}
