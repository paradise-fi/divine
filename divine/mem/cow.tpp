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

#include <divine/mem/cow.hpp>

namespace divine::mem
{

    template< typename Next > template< typename Cell >
    typename Cell::pointer Cow< Next >::ObjHasher::match( Cell &cell, Internal x, hash64_t hash ) const
    {
        if ( !cell.match( hash ) && !cell.tombstone( hash ) )
            return nullptr;

        auto a = cell.fetch(), b = x;
        int size = objects().size( a );
        if ( objects().size( b ) != size )
            return nullptr;
        if ( std::memcmp( objects().dereference( a ), objects().dereference( b ), size ) )
            return nullptr;

        if ( heap().compare( heap(), a, b, size, false ) )
            return nullptr;

        if ( cell.tombstone() )
            if ( cell.revive() )
                TRACE( "revive", x, "refcnt =",  _heap->_obj_refcnt.count( a ) );

        return cell.value();
    }

    template< typename Next > template< typename Cell >
    void Cow< Next >::ObjHasher::invalidate( const Cell &cell ) const
    {
        if ( !cell.tombstone() )
            return;

        auto v = cell.fetch();
        TRACE( "invalidate", v, "refcnt =", _heap->_obj_refcnt.count( v ) );
        _heap->_obj_refcnt.put( v );
    }

    template< typename Next > template< typename Cell, typename X >
    auto Cow< Next >::ObjHasher::erase( Cell &cell, const X &t, hash64_t h ) const -> typename HA::Erase
    {
        if ( cell.fetch() != t )
            return TRACE( "erase mismatch", cell.fetch(), "vs", t, "hash", h ), HA::Mismatch;

        TRACE( "erase", t, "refcnt =", _heap->_obj_refcnt.count( t ) );
        return HA::Bury;
    }

    template< typename Next >
    auto Cow< Next >::detach( Loc loc ) -> Internal
    {
        if ( _l.exceptions.count( loc.objid ) )
            return loc.object;

        int sz = this->size( loc.object );
        auto newobj = this->objects().allocate( sz ); /* FIXME layering violation? */

        _l.exceptions[ loc.objid ] = newobj;
        Next::materialise( newobj, sz );

        loc.offset = 0;
        auto res = this->copy( *this, loc, *this, Loc( newobj, loc.objid, 0 ), sz, true );
        ASSERT( res );
        return newobj;
    }

    template< typename Next >
    auto Cow< Next >::snap_dedup( SnapItem si ) const -> SnapItem
    {
        auto r = _ext.objects.insert( si.second, _ext.hasher );
        if ( r->load() == si.second )
            _obj_refcnt.get( si.second ); /* for the hash table reference */
        else
            this->free( si.second );
        si.second = r->load();
        _obj_refcnt.get( si.second );
        return si;
    }

    template< typename Next >
    void Cow< Next >::snap_put( Pool &p, Snapshot s )
    {
        _ext._free_pool = &p;
        _ext._free_snap = s;
        if ( !is_shared( p, s ) )
            snap_put();
    }

    template< typename Next >
    void Cow< Next >::snap_put() const
    {
        if ( !_ext._free_pool )
            return;

        auto &p = *_ext._free_pool;
        auto s = _ext._free_snap;
        _ext._free_pool = nullptr;

        auto erase = [&]( auto x, int refcnt )
        {
            if ( refcnt == 1 )
                _ext.objects.erase( x, _ext.hasher );
            return true;
        };

        for ( auto si = this->snap_begin( p, s ); si != this->snap_end( p, s ); ++si )
            _obj_refcnt.put( si->second, erase );

        p.free( s );
    }

    template< typename Next >
    auto Cow< Next >::snapshot( Pool &p ) const -> Snapshot
    {
        int count = 0;
        auto snap = this->snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != this->snap_end() && snap->first < except.first )
                ++ snap, ++ count;
            if ( snap != this->snap_end() && snap->first == except.first )
                snap++;
            if ( this->valid( except.second ) )
                ++ count;
        }

        while ( snap != this->snap_end() )
            ++ snap, ++ count;

        if ( !count )
            return Snapshot();

        auto s = p.allocate( count * sizeof( SnapItem ) );
        auto si = p.template machinePointer< SnapItem >( s );
        snap = this->snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != this->snap_end() && snap->first < except.first )
                *si++ = *snap_get( snap++ );
            if ( snap != this->snap_end() && snap->first == except.first )
                snap++;
            if ( this->valid( except.second ) )
                *si++ = snap_dedup( except );
        }

        while ( snap != this->snap_end() )
            *si++ = *snap_get( snap++ );

        auto newsnap = p.template machinePointer< SnapItem >( s );
        ASSERT_EQ( si, newsnap + count );
        for ( auto s = newsnap; s < newsnap + count; ++s )
            ASSERT( this->valid( s->second ) );

        snap_put();
        _l.exceptions.clear();
        _l.snap_begin = newsnap;
        _l.snap_size = count;

        Next::notify_snapshot();

        return s;
    }

}

// vim: ft=cpp
