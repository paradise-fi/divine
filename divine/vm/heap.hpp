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
    using Shadows = mem::Pool< 20, 12, uint32_t >;
    using Objects = mem::Pool< 32, 16, uint64_t, 1, 2, 0 >;

    Shadows _shadows;
    Objects _objects;
    using Internal = Objects::Pointer;

    struct ShadowEntries
    {
        using Ptr = Shadows::Pointer;
        using Hdr = struct { int _size; Ptr _next; };
        Ptr _p;
        Shadows &_s;
        ShadowEntries( Shadows &s, Ptr p ) : _s( s ), _p( p ) {}

        int &size() { return *_s.machinePointer< int >( _p ); }

        void reserve( int count )
        {
            ASSERT_LEQ( size() * sizeof( ShadowEntry ), _s.size( _p ) );
            size() += count;
        }

        ShadowEntry *begin()
        {
            return _s.machinePointer< ShadowEntry >( _p, sizeof( int ) );
        }

        ShadowEntry *end()
        {
            return _s.machinePointer< ShadowEntry >( _p, sizeof( int ) )
                + *_s.machinePointer< int >( _p );
        }

    };

    /*
     * The evaluator-facing pointer structure is laid over the Internal
     * (pool-derived) pointer type. The Internal pointer has tag bits at the
     * start, so that the type tag of GenericPointer and the offset are stored
     * within the tag area of the Pool Pointer.
     */
    struct Pointer : GenericPointer< PointerField< 30 >, PointerField< 32 > >
    {
        auto offset() { return this->template field< 1 >(); }
        auto object() { return this->template field< 2 >(); }
        bool null() { return object() == 0 && offset() == 0; }
        using Shadow = ShadowEntries::Ptr;
        Pointer() : GenericPointer( PointerType::Heap ) {}
        Pointer operator+( int off ) const
        {
            Pointer r = *this;
            r.offset() += off;
            return r;
        }
    };

    using PointerV = value::Pointer< Pointer >;
    using Shadow = vm::Shadow< ShadowEntries, PointerV >;

    template< typename T, typename F >
    T convert( F f )
    {
        T t;
        std::copy( f._v.storage, f._v.storage + sizeof( f._v.storage ), t._v.storage );
        return t;
    }

    Internal p2i( Pointer p ) { return convert< Internal >( p ); }
    Pointer i2p( Internal p ) { return convert< Pointer >( p ); }

    PointerV make( int size )
    {
        auto i = _objects.allocate( size );
        auto s = _shadows.allocate( sizeof( ShadowEntries::Hdr )
                                    + std::max( size_t( size ), sizeof( ShadowEntry ) ) );
        auto p = PointerV( i2p( i ) );
        p._v.type() = PointerType::Heap;
        p._s = s;
        ShadowEntries se( _shadows, s );
        se.size() = 1;
        ShadowEntry &entry = *se.begin();
        entry.fragmented() = false;
        entry.type() = uint32_t( ShadowET::UninitRun );
        entry.offset() = 0;
        entry.data() = 8 * size;
        return p;
    }

    bool free( PointerV p )
    {
        auto i = p2i( p.v() );
        if ( !_objects.valid( i ) )
            return false;
        ASSERT( _shadows.valid( p._s ) );
        _objects.free( i );
        _shadows.free( p._s );
        return true;
    }

    bool valid( PointerV p )
    {
        if ( _objects.valid( p2i( p.v() ) ) )
            return true;
        return false;
    }

    int size( PointerV p ) { return _objects.size( p2i( p.v() ) ); }

    template< typename T >
    T read( PointerV p )
    {
        using Raw = typename T::Raw;
        Pointer pv = p.v();
        ASSERT( valid( p ) );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - pv.offset() );
        auto r = T( *_objects.machinePointer< typename T::Raw >( p2i( pv ), pv.offset() ) );
        /*
        std::cerr << "read " << r << " from " << p2i( p )
                  << ", offset = " << p.offset().get() << ", size = " << size( p ) << std::endl;
        */
        Shadow sh( _shadows, p._s );
        sh.query( pv.offset(), r );
        return r;
    }

    template< typename T >
    void write( PointerV p, T t )
    {
        using Raw = typename T::Raw;
        Pointer pv = p.v();
        // std::cerr << "write " << t << " to " << p << ", size = " << size( p ) << std::endl;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - pv.offset() );
        Shadow sh( _shadows, p._s );
        sh.update( pv.offset(), t );
        *_objects.machinePointer< typename T::Raw >( p2i( pv ), pv.offset() ) = t.v();
    }

    template< typename T >
    void shift( PointerV &p, T t )
    {
        write( p, t );
        Pointer pv = p.v();
        pv.offset() += sizeof( typename T::Raw );
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
        pv.offset() += bytes;
        p.v( pv );
    }

    bool copy( PointerV _from, PointerV _to, int bytes )
    {
        char *from = _objects.dereference( p2i( _from.v() ) );
        char *to = _objects.dereference( p2i( _to.v() ) );
        Pointer from_v = _from.v(), to_v = _to.v();
        int from_s( size( _from ) ), to_s ( size( _to ) );
        int from_off( from_v.offset() ), to_off( to_v.offset() );
        if ( !from || !to || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;
        memcpy( to + to_off, from + from_off, bytes );
        Shadow from_sh( _shadows, _from._s ), to_sh( _shadows, _to._s );
        to_sh.update( from_sh, from_off, to_off, bytes );
        return true;
    }
};

static inline std::ostream &operator<<( std::ostream &o, MutableHeap::Pointer p )
{
    return o << "[heap " << p.object().get() << " @" << p.offset().get() << "]";
}

struct PersistentHeap
{
    // ShapeStore, DataStore, commit(), ...
};

}

namespace t_vm {

struct MutableHeap
{
    TEST(pointer)
    {
        vm::MutableHeap heap;
        using P = vm::MutableHeap::Pointer;
        P p;
        ASSERT_EQ( p.offset().get(), 0 );
        p.offset() = 10;
        p = heap.i2p( heap.p2i( p ) );
        ASSERT_EQ( p.offset().get(), 10 );
    }

    TEST(internal)
    {
        vm::MutableHeap heap;
        using I = vm::MutableHeap::Internal;
        I i( 10, 20 );
        auto p = heap.i2p( i );
        ASSERT_EQ( p.offset().get(), 0 );
        p.offset() = 10;
    }

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
        ASSERT_EQ( p._s, q._s );
    }
};

}

}
