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
        auto count = objects().size( i );
        auto word = objects().template machinePointer< uint32_t >( i );

        brick::hash::State high;

        auto comp = heap().compressed( Loc( i, 0, 0 ), count / 4 );
        auto c = comp.begin();

        while ( count >= 4 )
        {
            if ( ! Next::is_pointer_or_exception( *c ) ) /* NB. assumes little endian */
                high.update_aligned( *word );
            count -= 4;
            ++c, ++word;
        }

        auto byte = reinterpret_cast< uint8_t * >( word );
        while ( count > 0 )
            high.update_aligned( *byte++ ), --count;

        return high.hash();
    }

    template< typename Next >
    hash64_t Cow< Next >::ObjHasher::hash( Internal i )
    {
        brick::hash::State state;
        auto size = objects().size( i );
        state.update_aligned( objects().template machinePointer< uint8_t >( i ), size );
        state.realign();
        state.update_aligned( heap().meta().template machinePointer< uint8_t >( i ),
                              heap().meta_size( size ) );
        return ( content_only( i ) & 0xFFFFFFF000000000 ) | ( state.hash() & 0x0000000FFFFFFFF );
    }

    template< typename Next >
    bool Cow< Next >::ObjHasher::equal( Internal a, Internal b )
    {
        int size = objects().size( a );
        if ( objects().size( b ) != size )
            return false;
        if ( ::memcmp( objects().dereference( a ), objects().dereference( b ), size ) )
            return false;
        if ( heap().compare( heap(), a, b, size, false ) )
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
    auto Cow< Next >::snap_dedup( SnapItem si ) const -> SnapItem
    {
        auto r = _ext.objects.insert( si.second );
        if ( !r.isnew() )
        {
            ASSERT_NEQ( *r, si.second );
            this->free( si.second );
        }
        si.second = *r;
        _obj_refcnt.get( si.second );
        return si;
    }

    template< typename Next >
    void Cow< Next >::snap_put( Pool &p, Snapshot s )
    {
        auto erase = [&]( auto p ) { _ext.objects.erase( p ); };
        for ( auto si = this->snap_begin( p, s ); si != this->snap_end( p, s ); ++si )
            _obj_refcnt.put( si->second, erase );
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

        _l.exceptions.clear();
        _ext.writable.clear();
        _l.snap_begin = newsnap;
        _l.snap_size = count;

        return s;
    }

}

// vim: ft=cpp
