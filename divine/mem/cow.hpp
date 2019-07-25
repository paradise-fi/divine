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
#include <brick-hash>
#include <brick-hashset>
#include <brick-mem>
#include <unordered_set>

namespace divine::mem
{

    using brq::hash64_t;
    template< typename Next >
    struct Cow : Next
    {
        using typename Next::Internal;
        using typename Next::SnapItem;
        using typename Next::Snapshot;
        using typename Next::Pointer;
        using typename Next::Loc;
        using typename Next::Pool;
        using Next::_l; /* FIXME */

        mutable brick::mem::RefPool< Pool, uint8_t, true > _obj_refcnt;

        struct ObjHasher : brq::hash_adaptor< Internal >
        {
            using HA = brq::hash_adaptor< Internal >;

            Cow< Next > *_heap;
            auto &heap() const { return *_heap; }
            auto &objects() const { return _heap->_objects; }

            hash64_t hash( Internal i ) const
            {
                auto [ data, ptr ] = heap().hash_data( i );
                return ( ptr & 0xFFFFFFFFul ) ^ data;
            }

            template< typename Cell >
            typename Cell::pointer match( Cell &, Internal, hash64_t ) const;

            template< typename Cell >
            void invalidate( const Cell & ) const;

            template< typename cell, typename X >
            typename HA::Erase erase( cell &c, const X &t, hash64_t ) const;
        };

        mutable struct Ext
        {
            ObjHasher hasher;
            brq::concurrent_hash_set< Internal > objects;
            Pool *_free_pool = nullptr;
            Snapshot _free_snap;
        } _ext;

        void setupHT() { _ext.hasher._heap = this; }

        Cow() : _obj_refcnt( this->_objects ) { setupHT(); }
        Cow( const Cow &o ) : Next( o ), _obj_refcnt( o._obj_refcnt ), _ext( o._ext )
        {
            setupHT();
            ASSERT( _l.exceptions.empty() );
        }

        Cow &operator=( const Cow &o )
        {
            Next::operator=( o );
            _obj_refcnt = o._obj_refcnt;
            _ext = o._ext;
            setupHT();
            ASSERT( _l.exceptions.empty() );
            return *this;
        }

        template< typename FromH, typename ToH >
        static bool copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to,
                          int bytes, bool internal )
        {
            return Next::copy( from_h, from, to_h, to, bytes, internal );
        }

        Internal detach( Loc l );
        Snapshot snapshot( Pool &p ) const;
        SnapItem snap_dedup( SnapItem si ) const;
        void snap_put( Pool &p, Snapshot s );
        void snap_put() const;

        SnapItem *snap_get( SnapItem *si ) const
        {
            _obj_refcnt.get( si->second );
            return si;
        }

        bool is_shared( Pool &p, Snapshot s ) const
        {
            return p.template machinePointer< SnapItem >( s ) == _l.snap_begin;
        }

        void restore( Pool &p, Snapshot s )
        {
            snap_put();
            _l.snap_size = p.size( s ) / sizeof( SnapItem );
            _l.snap_begin = p.template machinePointer< SnapItem >( s );
            _l.exceptions.clear();
        }

        static constexpr bool can_snapshot() { return true; }
    };
}
