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

    using brick::hash::hash64_t;
    using brick::hash::hash128_t;

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

        mutable brick::mem::RefPool< Pool, uint8_t > _obj_refcnt;

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
        static bool copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to,
                          int bytes, bool internal )
        {
            ASSERT( to_h._ext.writable.count( to.objid ) );
            return Next::copy( from_h, from, to_h, to, bytes, internal );
        }

        Internal detach( Loc l );
        SnapItem dedup( SnapItem si ) const;
        Snapshot snapshot( Pool &p ) const;
        void unref( Pool &p, Snapshot s );

        bool resize( Pointer p, int sz_new )
        {
            auto rv = Next::resize( p, sz_new );
            _ext.writable.insert( p.object() );
            return rv;
        }

        bool is_shared( Pool &p, Snapshot s ) const
        {
            return p.template machinePointer< SnapItem >( s ) == _l.snap_begin;
        }

        void restore( Pool &p, Snapshot s )
        {
            _l.snap_size = p.size( s ) / sizeof( SnapItem );
            _l.snap_begin = p.template machinePointer< SnapItem >( s );
            _l.exceptions.clear();
            _ext.writable.clear();
        }

        static constexpr bool can_snapshot() { return true; }
    };
}
