// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2011-2018 Petr Ročkai <code@fixp.eu>
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
    struct Frontend
    {
    public:
        using Loc = typename Next::Loc;
        using Internal = typename Next::Internal;
        using Pointer = typename Next::Pointer;
        using PointerV = typename Next::PointerV;
        using Snapshot = typename Next::Snapshot;
        using Pool = typename Next::Pool;
        using SnapPool = typename Next::SnapPool;

        auto ht_stats()  { return n._ext.objects.stats(); }
        auto mem_stats() { return n._objects.stats(); }

        auto pointers( Loc l, int sz = 0 )
        {
            return n.pointers( l, sz ? sz : n.size( l.object ) );
        }

        auto pointers( Pointer p, int from = 0, int sz = 0 )
        {
            auto l = n.loc( p + from );
            return n.pointers( l, sz ? sz : n.size( l.object ) );
        }

        template< typename T >
        void write_shift( PointerV &p, T t )
        {
            write( p.cooked(), t );
            skip( p, t.size() );
        }

        template< typename T >
        void read_shift( PointerV &p, T &t ) const
        {
            read( p.cooked(), t );
            skip( p, t.size() );
        }

        template< typename T >
        void read_shift( Pointer &p, T &t ) const
        {
            read( p, t );
            p = p + t.size();
        }

        std::string read_string( Pointer ptr ) const
        {
            std::string str;
            typename Next::ByteV c;
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

        bool valid( Pointer p ) const { return n.valid( p ); }
        bool valid( Internal i ) const { return n.valid( i ); }
        auto hash_data( Internal i ) const { return n.hash_data( i ); }

        template< typename S, typename F >
        void hash( uint32_t obj, S &state, F ptr_cb ) const
        {
            auto i = n.ptr2i( obj );
            auto bytes = n.size( i );
            n.hash( i, bytes, state, ptr_cb );
        }

        template< typename S, typename F >
        void hash( Pointer p, S &state, F ptr_cb ) const
        {
            hash( p.object(), state, ptr_cb );
        }

        static constexpr bool can_snapshot() { return Next::can_snapshot(); }
        Snapshot snapshot( Pool &p ) { return n.snapshot( p ); }
        void restore( Pool &p, Snapshot s ) { n.restore( p, s ); }
        bool is_shared( Pool &p, Snapshot s ) const { return n.is_shared( p, s ); }
        void reset() { n.reset(); }
        void snap_put( Pool &p, Snapshot s ) { n.snap_put( p, s ); }

        auto snap_begin() const { return n.snap_begin(); }
        auto snap_end() const { return n.snap_end(); }
        auto snap_begin( Pool &p, Snapshot s ) const { return n.snap_begin( p, s ); }
        auto snap_end( Pool &p, Snapshot s ) const { return n.snap_end( p, s ); }
        auto &exceptions() { return n._l.exceptions; }

        void skip( PointerV &p, int bytes ) const
        {
            Pointer pv = p.cooked();
            pv.offset( pv.offset() + bytes );
            p.v( pv );
        }

        template< typename T >
        T *unsafe_deref( Pointer p, Internal i ) { return n.template unsafe_deref< T >( p, i ); }
        uint8_t *unsafe_ptr2mem( Internal i ) { return n.unsafe_ptr2mem( i ); }

        template< typename T >
        void unsafe_read( Pointer p, T &t, Internal i ) const
        {
            return n.unsafe_read( p, t, i );
        }

        HeapBytes unsafe_bytes( Loc l, int sz = 0 ) const
        {
            sz = sz ? sz : n.size( l.object ) - l.offset;
            ASSERT_LEQ( l.offset + sz, n.size( l.object ) );
            auto start = n.unsafe_ptr2mem( l.object );
            return HeapBytes( start + l.offset, start + l.offset + sz );
        }

        HeapBytes unsafe_bytes( Pointer p, int off = 0, int sz = 0 ) const
        {
            return unsafe_bytes( n.loc( p + off ), sz );
        }

        Loc loc( Pointer p ) const { return n.loc( p ); }
        Loc loc( Pointer p, Internal i ) const { return n.loc( p, i ); }
        Internal ptr2i( Pointer p ) const { return n.ptr2i( p ); }
        int size( Pointer p ) const { return n.size( n.ptr2i( p ) ); }
        int size( Internal p ) const { return n.size( p ); }

        bool resize( Pointer p, int size ) { return n.resize( p, size ); }

        PointerV make( int size, uint32_t hint = _VM_PL_Alloca + 1, bool over = false )
        {
            auto l = n.make( size, hint, over );
            return PointerV( Pointer( l.objid ) );
        }

        bool free( Pointer p ) { return n.free( p ); }
        bool free( Internal i ) { return n.free( i ); }

        template< typename T >
        void read( Loc l, T &t ) const
        {
            ASSERT_EQ( l.object, n.ptr2i( l.objid ) );
            n.read( l, t );
        }

        template< typename T >
        void read( Pointer p, T &t ) const
        {
            ASSERT( n.valid( p ), p );
            n.read( n.loc( p ), t );
        }

        template< typename T >
        auto write( Pointer p, T t )
        {
            ASSERT( n.valid( p ), p );
            return write( n.loc( p ), t );
        }

        template< typename T >
        auto write( Loc l, T t )
        {
            ASSERT_EQ( l.object, n.ptr2i( l.objid ) );
            l.object = n.detach( l );
            n.write( l, t );
            return l.object;
        }

        typename Next::UIntV peek( Pointer p, int key ) { return n.peek( n.loc( p ), key ); }
        typename Next::UIntV peek( Loc p, int key ) { return n.peek( p, key ); }

        template< typename T >
        auto poke( Loc l, int key, T v )
        {
            l.object = n.detach( l );
            n.poke( l, key, v );
            return l.object;
        }

        auto compressed( Loc l, unsigned w ) const { return n.compressed( l, w ); }
        static bool is_pointer( typename Next::Compressed c ) { return Next::is_pointer( c ); }

        template< typename FromH >
        bool copy( FromH &from_h, Pointer from, Pointer to, int bytes )
        {
            if ( from.null() || to.null() )
                return false;

            auto to_l = n.loc( to );
            return copy( from_h, from_h.loc( from ), to_l, bytes );
        }

        template< typename FromH >
        bool copy( FromH &from_h, typename FromH::Loc from, Loc &to, int bytes )
        {
            ASSERT_EQ( from.object, from_h.ptr2i( from.objid ) );
            ASSERT_EQ( to.object, n.ptr2i( to.objid ) );
            to.object = n.detach( to );
            return n.copy( from_h.n, from, n, to, bytes, false );
        }

        bool copy( Pointer f, Pointer t, int b ) { return copy( *this, f, t, b ); }

        bool equal( typename Next::Internal a, typename Next::Internal b, int sz, bool skip_objids )
        {
            return n.compare( n, a, b, sz, skip_objids ) == 0;
        }

        int compare( typename Next::Internal a, typename Next::Internal b, int sz, bool skip_objids )
        {
            return n.compare( n, a, b, sz, skip_objids );
        }

        template< typename H2 >
        int compare( H2 &h2, typename Next::Internal a, typename Next::Internal b, int sz,
                     bool skip_objids )
        {
            return n.compare( h2.n, a, b, sz, skip_objids );
        }

        Next n;
    };

}
