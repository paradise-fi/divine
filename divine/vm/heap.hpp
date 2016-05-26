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

struct MutableHeap
{
    using ExceptionPool = mem::Pool<>;
    using ObjectPool = mem::Pool<>;

    ExceptionPool _exceptions;
    ObjectPool _objects, _shadows;

    using Internal = ObjectPool::Pointer;

    struct Bytes
    {
        Internal _p;
        ObjectPool &_o;
        Bytes( ObjectPool &o, Internal p ) : _o( o ), _p( p ) {}
        int size() { return _o.size( _p ); }
        char *begin() { return _o.machinePointer< char >( _p ); }
        char *end() { return _o.machinePointer< char >( _p, size() ); }
    };

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

    using PointerV = value::Pointer< Pointer >;

    Internal p2i( Pointer p ) { Internal i; i.raw( p.object() ); return i; }

    PointerV make( int size )
    {
        auto i = _objects.allocate( size );
        int shsz = sizeof( ExceptionPool::Pointer ) + ( size % 4 ? 1 + size / 4 : size / 4 );
        _shadows.materialise( i, shsz, _objects );

        Pointer p;
        p.object( i.raw() );
        p.offset( 0 );
        return PointerV( p );
    }

    bool free( PointerV p )
    {
        auto i = p2i( p.v() );
        if ( !_objects.valid( i ) )
            return false;
        _objects.free( i );
        return true;
    }

    bool valid( PointerV p )
    {
        if ( _objects.valid( p2i( p.v() ) ) )
            return true;
        return false;
    }

    int size( PointerV p ) { return _objects.size( p2i( p.v() ) ); }

    auto shadow( Pointer pv )
    {
        using Shadow = vm::Shadow< PointerV >;
        using ExPtr = ExceptionPool::Pointer;
        auto ex_pptr = *_shadows.machinePointer< ExPtr >( p2i( pv ) );
        auto sh_ptr = _shadows.machinePointer< ShadowByte >( p2i( pv ), sizeof( ExPtr ) );
        auto ex_ptr = _exceptions.valid( ex_pptr ) ?
                      _exceptions.machinePointer< ShadowException >( ex_pptr ) : nullptr;
        return Shadow( sh_ptr, ex_ptr, _objects.size( p2i( pv ) ) );
    }

    template< typename T >
    T read( PointerV p )
    {
        using Raw = typename T::Raw;
        Pointer pv = p.v();
        ASSERT( valid( p ) );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - pv.offset() );

        auto r = T( *_objects.machinePointer< typename T::Raw >( p2i( pv ), pv.offset() ) );
        shadow( pv ).query( pv.offset(), r );
        return r;
    }

    template< typename T >
    void write( PointerV p, T t )
    {
        using Raw = typename T::Raw;
        Pointer pv = p.v();
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - pv.offset() );
        shadow( pv ).update( pv.offset(), t );
        *_objects.machinePointer< typename T::Raw >( p2i( pv ), pv.offset() ) = t.v();
    }

    template< typename T >
    void shift( PointerV &p, T t )
    {
        write( p, t );
        Pointer pv = p.v();
        pv.offset( pv.offset() + sizeof( typename T::Raw ) );
        p.v( pv );
    }

    template< typename T >
    auto shift( PointerV &p )
    {
        auto r = read< T >( p );
        skip( p, sizeof( typename T::Raw ) );
        return r;
    }

    void skip( PointerV &p, int bytes )
    {
        Pointer pv = p.v();
        pv.offset( pv.offset() + bytes );
        p.v( pv );
    }

    auto unsafe_bytes( PointerV p )
    {
        return Bytes( _objects, p2i( p.v() ) );
    }

    template< typename H >
    bool copy( H &_from_heap, PointerV _from, PointerV _to, int bytes )
    {
        auto from = _from_heap.unsafe_bytes( _from );
        auto to = unsafe_bytes( _to );
        Pointer from_v = _from.v(), to_v = _to.v();
        int from_s( size( _from ) ), to_s ( size( _to ) );
        int from_off( from_v.offset() ), to_off( to_v.offset() );
        if ( !from.begin() || !to.begin() || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;
        std::copy( from.begin() + from_off, from.begin() + from_off + bytes, to.begin() + to_off );
        auto from_sh = _from_heap.shadow( _from.v() ), to_sh = shadow( _to.v() );
        to_sh.update( from_sh, from_off, to_off, bytes );
        return true;
    }

    bool copy( PointerV f, PointerV t, int b ) { return copy( *this, f, t, b ); }
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
    TEST(alloc)
    {
        using I = vm::value::Int< 32, true >;
        vm::MutableHeap heap;
        auto p = heap.make( 16 );
        heap.write( p, I( 10 ) );
        auto q = heap.read< I >( p );
        ASSERT_EQ( q.v(), 10 );
    }

    TEST(conversion)
    {
        vm::MutableHeap heap;
        using HP = vm::MutableHeap::Pointer;
        auto p = heap.make( 16 );
        ASSERT_EQ( HP( p.v() ), HP( vm::ConstPointer( p.v() ) ) );
    }

    TEST(write_read)
    {
        vm::MutableHeap heap;
        auto p = heap.make( 16 );
        heap.write( p, p );
        auto q = heap.read< vm::MutableHeap::PointerV >( p );
        ASSERT( p.v() == q.v() );
    }
};

}

}
