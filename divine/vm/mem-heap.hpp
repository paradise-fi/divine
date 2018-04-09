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
#include <unordered_map>

#include <divine/vm/value.hpp>
#include <divine/vm/mem-shadow.hpp>
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

namespace divine::vm::mem::heap
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

namespace divine::vm::mem
{

    using brick::hash::hash64_t;
    using brick::hash::hash128_t;

    struct HeapBytes
    {
        uint8_t *_start, *_end;
        HeapBytes( uint8_t *start, uint8_t *end ) : _start( start ), _end( end ) {}
        int size() { return _end - _start; }
        uint8_t *begin() { return _start; }
        uint8_t *end() { return _end; }
        uint8_t &operator[]( int i ) { return *( _start + i ); }
    };

    template< typename Next >
    struct Frontend : Next
    {
        using typename Next::Loc;
        using PointerV = value::Pointer;

        using Next::pointers;
        auto pointers( HeapPointer p, int from = 0, int sz = 0 )
        {
            auto l = this->loc( p + from );
            return Next::pointers( l, sz ? sz : Next::size( l.object ) );
        }

        template< typename T >
        void write_shift( PointerV &p, T t )
        {
            write( p.cooked(), t );
            skip( p, sizeof( typename T::Raw ) );
        }

        template< typename T >
        void read_shift( PointerV &p, T &t ) const
        {
            read( p.cooked(), t );
            skip( p, sizeof( typename T::Raw ) );
        }

        template< typename T >
        void read_shift( HeapPointer &p, T &t ) const
        {
            read( p, t );
            p = p + sizeof( typename T::Raw );
        }

        std::string read_string( HeapPointer ptr ) const
        {
            std::string str;
            value::Int< 8, false > c;
            unsigned sz = size( ptr );
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

        HeapBytes unsafe_bytes( Loc l, int sz = 0 ) const
        {
            sz = sz ? sz : Next::size( l.object ) - l.offset;
            ASSERT_LEQ( l.offset + sz, Next::size( l.object ) );
            auto start = Next::unsafe_ptr2mem( l.object );
            return HeapBytes( start + l.offset, start + l.offset + sz );
        }

        HeapBytes unsafe_bytes( HeapPointer p, int off = 0, int sz = 0 ) const
        {
            return unsafe_bytes( this->loc( p + off ), sz );
        }

        int size( HeapPointer p ) const { return Next::size( this->ptr2i( p ) ); }
        using Next::size;

        PointerV make( int size, uint32_t hint = 1, bool over = false )
        {
            auto l = Next::make( size, hint, over );
            return PointerV( HeapPointer( l.objid ) );
        }

        template< typename T >
        void read( Loc l, T &t ) const
        {
            ASSERT_EQ( l.object, this->ptr2i( l.objid ) );
            Next::read( l, t );
        }

        template< typename T >
        void read( HeapPointer p, T &t ) const
        {
            ASSERT( this->valid( p ), p );
            Next::read( this->loc( p ), t );
        }

        template< typename T >
        auto write( HeapPointer p, T t )
        {
            ASSERT( this->valid( p ), p );
            return write( this->loc( p ), t );
        }

        template< typename T >
        auto write( Loc l, T t )
        {
            ASSERT_EQ( l.object, this->ptr2i( l.objid ) );
            l.object = this->detach( l );
            Next::write( l, t );
            return l.object;
        }

        template< typename FromH >
        bool copy( FromH &from_h, HeapPointer from, HeapPointer to, int bytes )
        {
            if ( from.null() || to.null() )
                return false;

            auto to_l = this->loc( to );
            return copy( from_h, from_h.loc( from ), to_l, bytes );
        }

        template< typename FromH >
        bool copy( FromH &from_h, typename FromH::Loc from, Loc &to, int bytes )
        {
            ASSERT_EQ( from.object, from_h.ptr2i( from.objid ) );
            ASSERT_EQ( to.object, this->ptr2i( to.objid ) );
            to.object = Next::detach( to );
            return Next::copy( from_h, from, *this, to, bytes );
        }

        bool copy( HeapPointer f, HeapPointer t, int b ) { return copy( *this, f, t, b ); }
    };

    template< typename Next >
    struct Storage : Next
    {
        /* TODO do we want to be able to use distinct pool types here? */
        using typename Next::Pool;
        using typename Next::Loc;
        using typename Next::Internal;

        using SnapPool = Pool;
        using Snapshot = typename Pool::Pointer;
        using PointerV = value::Pointer;

        Pool &objects() const { return this->_objects; }
        Pool &snapshots() const { return this->_snapshots; }

        struct SnapItem
        {
            uint32_t first;
            Internal second;
            operator std::pair< uint32_t, Internal >() { return std::make_pair( first, second ); }
            SnapItem( std::pair< const uint32_t, Internal > p ) : first( p.first ), second( p.second ) {}
            bool operator==( SnapItem si ) const { return si.first == first && si.second == second; }
        } __attribute__((packed));

        mutable struct Local
        {
            std::map< uint32_t, Internal > exceptions;
            Snapshot snapshot;
        } _l;

        uint64_t objhash( Internal ) { return 0; }
        Internal detach( Loc l ) { return l.object; }

        void reset() { _l.exceptions.clear(); _l.snapshot = Snapshot(); }
        void rollback() { _l.exceptions.clear(); } /* fixme leak */

        using Next::loc;
        Loc loc( HeapPointer p ) const { return loc( p, ptr2i( p ) ); }

        uint8_t *unsafe_ptr2mem( Internal i ) const
        {
            return objects().template machinePointer< uint8_t >( i );
        }

        Internal ptr2i( HeapPointer p ) const
        {
            return ptr2i( p.object() );
        }

        Internal ptr2i( uint32_t object ) const
        {
            auto hp = _l.exceptions.find( object );
            if ( hp != _l.exceptions.end() )
                return hp->second;

            auto si = snap_find( object );
            return si && si != snap_end() && si->first == object ? si->second : Internal();
        }

        int snap_size( Snapshot s ) const
        {
            if ( !snapshots().valid( s ) )
                return 0;
            return snapshots().size( s ) / sizeof( SnapItem );
        }

        SnapItem *snap_begin( Snapshot s ) const
        {
            if ( !snapshots().valid( s ) )
                return nullptr;
            return snapshots().template machinePointer< SnapItem >( s );
        }

        SnapItem *snap_begin() const { return snap_begin( _l.snapshot ); }
        SnapItem *snap_end( Snapshot s ) const { return snap_begin( s ) + snap_size( s ); }
        SnapItem *snap_end() const { return snap_end( _l.snapshot ); }
        SnapItem *snap_find( uint32_t obj ) const;

        Loc make( int size, uint32_t hint = 1, bool overwrite = false );
        bool resize( HeapPointer p, int sz_new );
        bool free( HeapPointer p );

        bool valid( HeapPointer p ) const
        {
            if ( !p.object() )
                return false;
            return ptr2i( p ).slab();
        }

        bool valid( Internal i ) const { return objects().valid( i ); }
        int size( Internal i ) const { return objects().size( i ); }
        void free( Internal i ) const { objects().free( i ); }

        void write( Loc, value::Void ) {}
        void read( Loc, value::Void& ) const {}

        template< typename T > void read( Loc p, T &t ) const;
        template< typename T > void write( Loc p, T t );

        template< typename FromH, typename ToH >
        static bool copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int bytes );

        template< typename T >
        T *unsafe_deref( HeapPointer p, Internal i ) const
        {
            return objects().template machinePointer< T >( i, p.offset() );
        }

        template< typename T >
        void unsafe_read( HeapPointer p, T &t, Internal i ) const
        {
            t.raw( *unsafe_deref< typename T::Raw >( p, i ) );
        }

        void restore( Snapshot ) { UNREACHABLE( "restore() is not available" ); }
        Snapshot snapshot() { UNREACHABLE( "snapshot() is not available" ); }
    };

    template< typename Next >
    struct Cow : Next
    {
        using typename Next::Internal;
        using typename Next::SnapItem;
        using typename Next::Snapshot;
        using typename Next::Loc;
        using Next::_l; /* FIXME */

        struct ObjHasher
        {
            Cow< Next > *_heap;
            auto &heap() { return *_heap; }
            auto &objects() { return _heap->_objects; }

            hash64_t content_only( Internal i );
            hash128_t hash( Internal i );
            bool equal( Internal a, Internal b );
        };

        auto objhash( Internal i ) { return _ext.objects.hasher.content_only( i ); }

        mutable struct Ext
        {
            std::unordered_set< int > writable;
            brick::hashset::Concurrent< Internal, ObjHasher > objects;
        } _ext;

        void setupHT() { _ext.objects.hasher._heap = this; }

        Cow() { setupHT(); }
        Cow( const Cow &o ) : Next( o ), _ext( o._ext )
        {
            setupHT();
            restore( o.snapshot() );
        }

        Cow &operator=( const Cow &o )
        {
            Next::operator=( o );
            _ext = o._ext;
            setupHT();
            restore( o.snapshot() );
            return *this;
        }

        Loc make( int size, uint32_t hint, bool over )
        {
            auto l = Next::make( size, hint, over );
            _ext.writable.insert( l.objid );
            return l;
        }

        void reset()
        {
            Next::reset();
            _ext.writable.clear();
        }

        void rollback()
        {
            Next::rollback();
            _ext.writable.clear();
        }

        template< typename T >
        auto write( Loc l, T t )
        {
            ASSERT( _ext.writable.count( l.objid ) );
            return Next::write( l, t );
        }

        template< typename FromH, typename ToH >
        static bool copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int bytes )
        {
            ASSERT( to_h._ext.writable.count( to.objid ) );
            return Next::copy( from_h, from, to_h, to, bytes );
        }

        Internal detach( Loc l );
        SnapItem dedup( SnapItem si ) const;
        Snapshot snapshot() const;

        bool resize( HeapPointer p, int sz_new )
        {
            auto rv = Next::resize( p, sz_new );
            _ext.writable.insert( p.object() );
            return rv;
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
