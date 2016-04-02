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

enum class ShadowET { Pointer, GhostSM, GhostOI, Direct, InitRun, UninitRun };

/*
 * Shadow entries track initialisation status and pointer locations. The shadow
 * map (see below) is made up of ShadowEntries that cover the entire object.
 * Non-pointer data is covered by Direct/InitRun/UninitRun entries, each
 * pointer, on the other hand, must be covered by a Pointer entry.
 *
 * The shadow map supports tracking fragmented pointers (this can happen during
 * byte-level copies, for example). However, such pointers are *expensive*:
 * each such pointer needs a pair of Ghost entries, and up to 8 fragment
 * entries (that is 80 bytes for a single pointer in the worst case). Each
 * fragmented pointer in a given shadow map comes with an 8 bit ID. For Ghost
 * entries, the ID is stored in the bit offset field, in the fragments, it is
 * stored in the type-specific area. The GhostSM entry holds the ShadowMap
 * object for the pointer, while the GhostOI holds the original object part of
 * the pointer. Each fragment contains a single byte of the fragmented pointer
 * (which byte this is is stored in the 'type or fragment id' field). The
 * content of that byte
 *
 * A normal (continuous) Pointer entry only needs 2 bits for tracking
 * definedness (object id, offset), the remaining 30 bits of the
 * 'type-specific' field are used to store the shadowmap object id that goes
 * along with this pointer.
 *
 * Since we only use 28 bits for the bit offset field, a single shadowmap can
 * cover at most 32M (this means that objects larger than 32M need a different
 * shadowmap representation).
 */
struct ShadowEntry
{
    using Bits = bitlevel::BitTuple<
        bitlevel::BitField< bool, 1 >, // fragmented?
        bitlevel::BitField< uint32_t, 3 >, // type or fragment
        bitlevel::BitField< uint32_t, 28 >, // bit offset
        bitlevel::BitField< uint32_t, 32 > >; // type-specific
    Bits _bits;
    auto fragmented() { return bitlevel::get< 0 >( _bits ); }
    auto type() { return bitlevel::get< 1 >( _bits ); }
    auto offset() { return bitlevel::get< 2 >( _bits ); }
    auto data() { return bitlevel::get< 3 >( _bits ); }

    int size()
    {
        switch ( ShadowET( type().get() ) )
        {
            case ShadowET::Pointer: return PointerBits;
            case ShadowET::UninitRun:
            case ShadowET::InitRun: return data();
            case ShadowET::Direct: return 32;
            default: NOT_IMPLEMENTED();
        }
    }
};

/*
 * The shadow map is an offset-sorted vector (for now, at least) of shadow
 * entries (see above). For now, all operations scan through the entire vector,
 * although for shadowmaps larger than ~8 entries, binary searching might be
 * more suitable.
 */
template< typename Heap >
struct ShadowMap
{
    using Ptr = typename Heap::ShadowPtr;
    using PointerV = typename Heap::PointerV;

    Heap &_h;
    Ptr _p;

    ShadowMap( Heap &h, Ptr p ) : _h( h ), _p( p ) {}

    void copy( ... );

    template< typename V >
    void update( int offset, V v ) {}

    template< typename V > /* value::_ */
    void query( int offset, V &v )
    {
    }

    void update_entry( ShadowEntry *e, int offset, PointerV v )
    {
        e->type() = int( ShadowET::Pointer );
        e->offset() = offset;
        e->data() = v._s.raw() & ~3;
        e->data() |= (v._obj_defined & 1);
        e->data() |= (v._off_defined & 1) << 1;
    }

    void update( int offset, PointerV v )
    {
        bool done = false;
        auto s = _h.shadow_begin( v._s ), last = _h.shadow_end( v._s );
        do {
            if ( s->offset() == offset && s->type() == int( ShadowET::Pointer ) )
            {
                update_entry( s, offset, v );
                done = true;
                break;
            }
        } while ( ++s < last );

        auto n = _h.shadow_extend( v._s, 1 );
        update_entry( n, offset, v );
    }

    void query( int offset, PointerV &v )
    {
        v._s = Ptr();
        v._obj_defined = v._off_defined = false;
        auto s = _h.shadow_begin( _p ), last = _h.shadow_end( _p );
        do {
            if ( s->offset() == offset && s->type() == int( ShadowET::Pointer ) )
            {
                v._obj_defined = s->data() & 1;
                v._off_defined = s->data() & 2;
                v._s = s->data() & ~3;
            }
            if ( s->offset() > offset )
                return;
        } while ( ++s < last );
    }
};

using namespace brick;

struct MutableHeap
{
    mem::Pool< 32, 16, uint64_t, 1, 2, 0 > _objects;
    mem::Pool< 20, 10, uint32_t > _shadows;
    using Internal = decltype( _objects )::Pointer;
    using ShadowPtr = decltype( _shadows )::Pointer;
    using ShadowMap = vm::ShadowMap< MutableHeap >;

    struct Shadow { int _size; ShadowPtr _next; };

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
        using Shadow = ShadowPtr;
        Pointer() : GenericPointer( PointerType::Heap ) {}
        Pointer operator+( int off ) const
        {
            Pointer r = *this;
            r.offset() += off;
            return r;
        }
    };

    using PointerV = value::Pointer< Pointer >;

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
        auto s = _shadows.allocate( sizeof( Shadow )
                                    + std::max( size_t( size ), sizeof( ShadowEntry ) ) );
        auto p = PointerV( i2p( i ) );
        p._v.type() = PointerType::Heap;
        p._s = s;
        ShadowEntry &entry = *shadow_begin( s );
        int &s_size = *_shadows.machinePointer< int >( s );
        s_size = 1;
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

    int &shadow_size( ShadowPtr p ) { return *_shadows.machinePointer< int >( p ); }

    ShadowEntry *shadow_extend( ShadowPtr s, int count )
    {
        auto e = shadow_end( s );
        shadow_size( s ) += count;
        ASSERT_LEQ( shadow_size( s ) * sizeof( ShadowEntry ), _shadows.size( s ) );
        return e;
    }

    ShadowEntry *shadow_begin( ShadowPtr s )
    {
        return _shadows.machinePointer< ShadowEntry >( s, sizeof( int ) );
    }

    ShadowEntry *shadow_end( ShadowPtr s )
    {
        return _shadows.machinePointer< ShadowEntry >( s, sizeof( int ) )
            + *_shadows.machinePointer< int >( s );
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
        ShadowMap sm( *this, p._s );
        sm.query( pv.offset(), r );
        /*
        std::cerr << "read " << r << " from " << p2i( p )
                  << ", offset = " << p.offset().get() << ", size = " << size( p ) << std::endl;
        */
        return r;
    }

    template< typename T >
    void write( PointerV p, T t )
    {
        using Raw = typename T::Raw;
        Pointer pv = p.v();
        /*
        std::cerr << "write " << t << " to " << p2i( p )
                  << ", offset = " << p.offset().get()  << ", size = " << size( p ) << std::endl;
        */
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p ) - pv.offset() );
        ShadowMap sm( *this, p._s );
        sm.update( pv.offset(), t );
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
        /* TODO flags */
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
