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

    template< typename Next>
    hash64_t Cow< Next >::ObjHasher::content_only( Internal i )
    {
        auto size = objects().size( i );
        auto base = objects().dereference( i );

        brick::hash::jenkins::SpookyState high( 0, 0 );

        auto comp = heap().compressed( Loc( i, 0, 0 ), size / 4 );
        auto c = comp.begin();
        int offset = 0;

        while ( offset + 4 <= size )
        {
            if ( ! Next::is_pointer_or_exception( *c ) ) /* NB. assumes little endian */
                high.update( base + offset, 4 );
            offset += 4;
            ++c;
        }

        high.update( base + offset, size - offset );
        ASSERT_LEQ( offset, size );

        return high.finalize().first;
    }

    template< typename Next >
    hash128_t Cow< Next >::ObjHasher::hash( Internal i )
    {
        /* TODO also hash some shadow data into low for better precision? */
        auto low = brick::hash::spooky( objects().dereference( i ), objects().size( i ) );
        return std::make_pair( ( content_only( i ) & 0xFFFFFFF000000000 ) | /* high 28 bits */
                            ( low.first & 0x0000000FFFFFFFF ), low.second );
    }

    template< typename Next >
    bool Cow< Next >::ObjHasher::equal( Internal a, Internal b )
    {
        int size = objects().size( a );
        if ( objects().size( b ) != size )
            return false;
        if ( ::memcmp( objects().dereference( a ), objects().dereference( b ), size ) )
            return false;
        if ( !heap().equal( a, b, size ) )
            return false;
        return true;
    }

    template< typename Next >
    auto Cow< Next >::detach( Loc loc ) -> Internal
    {
        if ( _ext.writable.count( loc.objid ) )
            return loc.object;
        ASSERT_EQ( _l.exceptions.count( loc.objid ), 0 );
        _ext.writable.insert( loc.objid );

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
    auto Cow< Next >::dedup( SnapItem si ) const -> SnapItem
    {
        auto r = _ext.objects.insert( si.second );
        if ( !r.isnew() )
        {
            ASSERT_NEQ( *r, si.second );
            this->free( si.second );
        }
        si.second = *r;
        return si;
    }

    template< typename Next >
    auto Cow< Next >::snapshot() const -> Snapshot
    {
        int count = 0;

        if ( _l.exceptions.empty() )
            return _l.snapshot;

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

        auto s = this->snapshots().allocate( count * sizeof( SnapItem ) );
        auto si = this->snapshots().template machinePointer< SnapItem >( s );
        snap = this->snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != this->snap_end() && snap->first < except.first )
                *si++ = *snap++;
            if ( snap != this->snap_end() && snap->first == except.first )
                snap++;
            if ( this->valid( except.second ) )
                *si++ = dedup( except );
        }

        while ( snap != this->snap_end() )
            *si++ = *snap++;

        auto newsnap = this->snapshots().template machinePointer< SnapItem >( s );
        ASSERT_EQ( si, newsnap + count );
        for ( auto s = newsnap; s < newsnap + count; ++s )
            ASSERT( this->valid( s->second ) );

        _l.exceptions.clear();
        _ext.writable.clear();
        _l.snapshot = s;

        return s;
    }

}

// vim: ft=cpp
