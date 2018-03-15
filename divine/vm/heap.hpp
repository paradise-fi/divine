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

namespace divine::vm::heap
{
    template< typename H1, typename H2, typename MarkedComparer >
    int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2,
                std::unordered_map< HeapPointer, int > &v1,
                std::unordered_map< HeapPointer, int > &v2, int &seq,
                MarkedComparer &markedComparer );

    template< typename Heap >
    int hash( Heap &heap, HeapPointer root,
            std::unordered_map< int, int > &visited,
            brick::hash::jenkins::SpookyState &state, int depth );

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
                    CloneType ct, bool overwrite );

    template< typename FromH, typename ToH >
    HeapPointer clone( FromH &f, ToH &t, HeapPointer root, CloneType ct = CloneType::All,
                    bool over = false )
    {
        std::map< HeapPointer, HeapPointer > visited;
        return clone( f, t, root, visited, ct, over );
    }

    template< typename Heap, typename F, typename... Roots >
    void leaked( Heap &h, F f, Roots... roots );
}

namespace divine::vm
{

    namespace mem = brick::mem;

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
    struct SimpleHeap : HeapMixin< Self, PooledTaintShadow< PooledShadow, mem::Pool< PR > >,
                                   typename mem::Pool< PR >::Pointer >
    {
        Self &self() { return *static_cast< Self * >( this ); }

        using ObjPool = mem::Pool< PR >;
        using SnapPool = mem::Pool< PR >;

        using Internal = typename ObjPool::Pointer;
        using Snapshot = typename SnapPool::Pointer;

        using Shadows = PooledTaintShadow< PooledShadow, ObjPool >;
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

        Internal ptr2i( HeapPointer p ) const
        {
            auto hp = _l.exceptions.find( p.object() );
            if ( hp != _l.exceptions.end() )
                return hp->second;

            auto si = snap_find( p.object() );
            return si && si != snap_end() && si->first == p.object() ? si->second : Internal();
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
        SnapItem *snap_find( uint32_t obj ) const;

        PointerV make( int size, uint32_t hint = 1, bool overwrite = false );
        bool resize( HeapPointer p, int sz_new );
        bool free( HeapPointer p );

        bool valid( HeapPointer p ) const
        {
            if ( !p.object() )
                return false;
            return ptr2i( p ).slab();
        }

        bool shared( HeapPointer p ) const { return _shadows.shared( ptr2i( p ) ); }
        bool shared( GenericPointer gp, bool sh );
        int size( HeapPointer, Internal i ) const { return _objects.size( i ); }
        int size( HeapPointer p ) const { return size( p, ptr2i( p ) ); }

        void write( HeapPointer, value::Void ) {}
        void write( HeapPointer, value::Void, Internal ) {}
        void read( HeapPointer, value::Void& ) const {}
        void read( HeapPointer, value::Void&, Internal ) const {}

        template< typename T >
        void read( HeapPointer p, T &t, Internal i ) const;

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

        template< typename T > Internal write( HeapPointer p, T t, Internal i );

        template< typename T >
        void write( HeapPointer p, T t ) { write( p, t, ptr2i( p ) ); }

        template< typename FromH >
        bool copy( FromH &from_h, HeapPointer _from, typename FromH::Internal _from_i,
                   HeapPointer _to, Internal &_to_i, int bytes );

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

            brick::hash::hash64_t content_only( Internal i );
            brick::hash::hash128_t hash( Internal i );
            bool equal( Internal a, Internal b );
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

        Internal detach( HeapPointer p, Internal i );
        SnapItem dedup( SnapItem si ) const;
        Snapshot snapshot() const;

        bool is_shared( Snapshot s ) const { return s == _l.snapshot; }
        void restore( Snapshot s )
        {
            _l.exceptions.clear();
            _ext.writable.clear();
            _l.snapshot = s;
        }
    };

}

#ifdef BRICK_UNITTEST_REG
namespace divine::t_vm
{

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
#endif
