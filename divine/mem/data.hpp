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
#include <divine/vm/types.hpp>

namespace divine::mem
{

    template< typename Next >
    struct Data : Next
    {
        /* TODO do we want to be able to use distinct pool types here? */
        using typename Next::Pool;
        using typename Next::Loc;
        using typename Next::Internal;
        using typename Next::Pointer;

        using SnapPool = Pool;
        using Snapshot = typename Pool::Pointer;
        using typename Next::PointerV;

        Pool &objects() const { return this->_objects; }

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
            SnapItem *snap_begin = nullptr;
            int snap_size = 0;
        } _l;

        std::pair< uint64_t, uint64_t > hash_data( Internal i ) const
        {
            brq::hash_state data, ptr;
            auto ptr_cb = [&]( uint32_t obj ) { ptr.update_aligned( obj ); };
            hash( i, size( i ), data, ptr_cb );
            TRACE( "hash_data", std::hex, data.hash(), ptr.hash() );
            return { data.hash(), ptr.hash() };
        }

        Internal detach( Loc l ) { return l.object; }

        template< typename S, typename F >
        void hash( Internal i, int bytes, S &state, F ptr_cb ) const;

        template< typename F >
        int compare( Internal a, Internal b, F ptr_cb, int bytes ) const;

        void reset() { _l.exceptions.clear(); _l.snap_size = 0; _l.snap_begin = nullptr; }
        void rollback() { _l.exceptions.clear(); } /* fixme leak */

        using Next::loc;
        Loc loc( Pointer p ) const { return loc( p, ptr2i( p ) ); }

        uint8_t *unsafe_ptr2mem( Internal i ) const
        {
            return objects().template machinePointer< uint8_t >( i );
        }

        Internal ptr2i( Pointer p ) const
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

        SnapItem *snap_begin() const { return _l.snap_begin; }
        SnapItem *snap_begin( Pool &p, Snapshot s ) const
        {
            return p.template machinePointer< SnapItem >( s );
        }

        SnapItem *snap_end() const { return _l.snap_begin + _l.snap_size; }
        SnapItem *snap_end( Pool &p, Snapshot s ) const
        {
            return snap_begin( p, s ) + p.size( s ) / sizeof( SnapItem );
        }

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
                    ASSERT( valid( pivot->second ) );
                    return pivot;
                }
            }

            return begin;
        }

        Loc make( int size, uint32_t hint, bool overwrite );
        bool resize( Pointer p, int sz_new );
        bool free( Pointer p );

        bool valid( Pointer p ) const
        {
            if ( !p.object() )
                return false;
            return ptr2i( p ).slab();
        }

        bool valid( uint32_t o ) const { return valid( ptr2i( o ) ); }
        bool valid( Internal i ) const { return objects().valid( i ); }
        int size( Internal i ) const { return objects().size( i ); }
        void free( Internal i ) const { objects().free( i ); Next::free( i ); }

        template< typename T > void read( Loc p, T &t ) const;
        template< typename T > void write( Loc p, T t );

        template< typename FromH, typename ToH >
        static bool copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int bytes, bool );

        template< typename T >
        T *unsafe_deref( Pointer p, Internal i ) const
        {
            return objects().template machinePointer< T >( i, p.offset() );
        }

        template< typename T >
        void unsafe_read( Pointer p, T &t, Internal i ) const
        {
            t.raw( *unsafe_deref< typename T::Raw >( p, i ) );
        }

        void restore( Pool &, Snapshot ) { UNREACHABLE( "restore() is not available" ); }
        Snapshot snapshot( Pool & ) { return Snapshot(); }
    };

}
