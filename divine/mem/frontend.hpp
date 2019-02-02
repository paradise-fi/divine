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
    struct Frontend : Next
    {
        using typename Next::Loc;
        using typename Next::Pointer;
        using typename Next::PointerV;
        using typename Next::ByteV;

        using Next::pointers;
        auto pointers( Pointer p, int from = 0, int sz = 0 )
        {
            auto l = this->loc( p + from );
            return Next::pointers( l, sz ? sz : Next::size( l.object ) );
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
            ByteV c;
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
            Pointer pv = p.cooked();
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

        HeapBytes unsafe_bytes( Pointer p, int off = 0, int sz = 0 ) const
        {
            return unsafe_bytes( this->loc( p + off ), sz );
        }

        int size( Pointer p ) const { return Next::size( this->ptr2i( p ) ); }
        using Next::size;

        PointerV make( int size, uint32_t hint = _VM_PL_Alloca + 1, bool over = false )
        {
            auto l = Next::make( size, hint, over );
            return PointerV( Pointer( l.objid ) );
        }

        template< typename T >
        void read( Loc l, T &t ) const
        {
            ASSERT_EQ( l.object, this->ptr2i( l.objid ) );
            Next::read( l, t );
        }

        template< typename T >
        void read( Pointer p, T &t ) const
        {
            ASSERT( this->valid( p ), p );
            Next::read( this->loc( p ), t );
        }

        template< typename T >
        auto write( Pointer p, T t )
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

        using Next::peek;
        typename Next::UIntV peek( Pointer p, int key )
        {
            return Next::peek( this->loc( p ), key );
        }

        template< typename T >
        auto poke( Loc l, int key, T v )
        {
            l.object = this->detach( l );
            Next::poke( l, key, v );
            return l.object;
        }

        template< typename FromH >
        bool copy( FromH &from_h, Pointer from, Pointer to, int bytes )
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
            return Next::copy( from_h, from, *this, to, bytes, false );
        }

        bool copy( Pointer f, Pointer t, int b ) { return copy( *this, f, t, b ); }

        bool equal( typename Next::Internal a, typename Next::Internal b, int sz, bool skip_objids )
        {
            return Next::compare( *this, a, b, sz, skip_objids ) == 0;
        }
    };

}
