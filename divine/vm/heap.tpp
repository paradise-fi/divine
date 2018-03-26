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

#include <divine/vm/heap.hpp>

namespace divine::vm::mem::heap
{

    template< typename H1, typename H2, typename MarkedComparer >
    int compare( H1 &h1, H2 &h2, HeapPointer r1, HeapPointer r2,
                std::unordered_map< HeapPointer, int > &v1,
                std::unordered_map< HeapPointer, int > &v2, int &seq,
                MarkedComparer &markedComparer )
    {
        r1.offset( 0 ); r2.offset( 0 );

        auto v1r1 = v1.find( r1 );
        auto v2r2 = v2.find( r2 );

        if ( v1r1 != v1.end() && v2r2 != v2.end() )
            return v1r1->second - v2r2->second;

        if ( v1r1 != v1.end() )
            return -1;
        if ( v2r2 != v2.end() )
            return 1;

        v1[ r1 ] = seq;
        v2[ r2 ] = seq;
        ++ seq;

        if ( h1.valid( r1 ) != h2.valid( r2 ) )
            return h1.valid( r1 ) - h2.valid( r2 );

        if ( !h1.valid( r1 ) )
            return 0;

        if ( h1.shared( r1 ) != h2.shared( r2 ) )
            return h1.shared( r1 ) - h2.shared( r2 );

        auto i1 = h1.ptr2i( r1 ), i2 = h2.ptr2i( r2 );
        int s1 = h1.size( r1, i1 ), s2 = h2.size( r2, i2 );
        if ( s1 - s2 )
            return s1 - s2;

        int shadow_cmp = h2.shadows().compare( h1.shadows(), i1, i2, s1 );
        if ( shadow_cmp )
            return shadow_cmp;

        auto b1 = h1.unsafe_bytes( r1, i1 ), b2 = h2.unsafe_bytes( r2, i2 );
        auto p1 = h1.pointers( r1, i1 ), p2 = h2.pointers( r2, i2 );
        int offset = 0;
        auto p1i = p1.begin(), p2i = p2.begin();

        while ( true )
        {
            int end = p1i == p1.end() ? s1 : p1i->offset();
            while ( offset < end )
            {
                if ( b1[ offset ] != b2[ offset ] )
                    return b1[ offset ] - b2[ offset ];
                ++ offset;
            }

            if ( p1i == p1.end() )
                return 0;

            if ( p1i->size() != p2i->size() )
                return p1i->size() - p2i->size();

            offset += p1i->size();

            ASSERT_EQ( p1i->offset(), p2i->offset() );
            r1.offset( p1i->offset() );
            r2.offset( p1i->offset() );

            /* recurse */
            int pdiff = 0;
            if ( p1i->size() == 8 )
            {
                value::Pointer p1p, p2p;
                h1.unsafe_read( r1, p1p, i1 );
                h2.unsafe_read( r2, p2p, i2 );
                auto p1pp = p1p.cooked(), p2pp = p2p.cooked();
                if ( p1pp.type() == p2pp.type() )
                {
                    if ( p1pp.offset() != p2pp.offset() )
                        return p1pp.offset() - p2pp.offset();
                    else if ( p1pp.type() == PointerType::Heap )
                        pdiff = compare( h1, h2, p1pp, p2pp, v1, v2, seq, markedComparer );
                    else if ( p1pp.type() == PointerType::Marked )
                        markedComparer( p1pp, p2pp );
                    else if ( p1pp.heap() ); // Weak
                    else
                        pdiff = p1pp.object() - p2pp.object();
                } else pdiff = int( p1pp.type() ) - int( p2pp.type() );
            }
            else
            {
                GenericPointer p1pp( PointerType::Heap ),
                               p2pp( PointerType::Heap );
                if ( p1i->size() == 1 )
                {
                    p1pp.object( p1i->fragment() );
                    p2pp.object( p2i->fragment() );
                }
                else if ( p1i->size() == 4 )
                {
                    uint32_t obj1 = *h1.template unsafe_deref< uint32_t >( r1, i1 ),
                             obj2 = *h2.template unsafe_deref< uint32_t >( r2, i2 );
                    p1pp.object( obj1 );
                    p2pp.object( obj2 );
                }
                else
                    NOT_IMPLEMENTED();

                pdiff = compare( h1, h2, p1pp, p2pp, v1, v2, seq, markedComparer );
            }

            if ( pdiff )
                return pdiff;
            ++ p1i; ++ p2i;
        }

        UNREACHABLE( "heap comparison fell through" );
    }

    template< typename Heap >
    int hash( Heap &heap, HeapPointer root,
            std::unordered_map< int, int > &visited,
            brick::hash::jenkins::SpookyState &state, int depth )
    {
        auto i = heap.ptr2i( root );
        int size = heap.size( root, i );
        uint32_t content_hash = i.tag();

        visited.emplace( root.object(), content_hash );

        if ( depth > 8 )
            return content_hash;

        if ( size > 16 * 1024 )
            return content_hash; /* skip following pointers in objects over 16k, not worth it */

        int ptr_data[2];
        auto pointers = heap.shadows().pointers(
                typename Heap::Shadows::Loc( i, 0 ), size );
        for ( auto pos : pointers )
        {
            value::Pointer ptr;
            ASSERT_LT( pos.offset(), heap.size( root ) );
            root.offset( pos.offset() );
            heap.unsafe_read( root, ptr, i );
            auto obj = ptr.cooked();
            ptr_data[1] = obj.offset();
            if ( obj.type() == PointerType::Heap )
            {
                auto vis = visited.find( obj.object() );
                if ( vis == visited.end() )
                {
                    obj.offset( 0 );
                    if ( heap.valid( obj ) )
                        ptr_data[0] = hash( heap, obj, visited, state, depth + 1 );
                    else
                        ptr_data[0] = 0; /* freed object, ignore */
                }
                else
                    ptr_data[0] = vis->second;
            }
            else if ( obj.heap() ); // Weak or Marked
            else
                ptr_data[0] = obj.object();
            state.update( ptr_data, sizeof( ptr_data ) );
        }

        return content_hash;
    }

    template< typename FromH, typename ToH >
    HeapPointer clone( FromH &f, ToH &t, HeapPointer root,
                    std::map< HeapPointer, HeapPointer > &visited,
                    CloneType ct, bool overwrite )
    {
        if ( root.null() )
            return root;
        if ( !f.valid( root ) )
            return root;
        auto seen = visited.find( root );
        if ( seen != visited.end() )
            return seen->second;

        auto root_i = f.ptr2i( root );
        if ( overwrite )
            t.free( root );
        auto result = t.make( f.size( root ), root.object(), true ).cooked();
        if ( overwrite )
            ASSERT_EQ( root.object(), result.object() );
        auto result_i = t.ptr2i( result );
        visited.emplace( root, result );

        t.copy( f, root, result, f.size( root ) );

        for ( auto pos : f.pointers( root ) )
        {
            value::Pointer ptr, ptr_c;
            GenericPointer cloned;
            root.offset( pos.offset() );
            result.offset( pos.offset() );
            f.read( root, ptr, root_i );
            auto obj = ptr.cooked();
            obj.offset( 0 );
            if ( obj.heap() )
                ASSERT( ptr.pointer() );
            if ( ct == CloneType::SkipWeak && obj.type() == PointerType::Weak )
                cloned = obj;
            else if ( ct == CloneType::HeapOnly && obj.type() != PointerType::Heap )
                cloned = obj;
            else if ( obj.heap() )
                cloned = clone( f, t, obj, visited, ct, overwrite );
            else
                cloned = obj;
            cloned.offset( ptr.cooked().offset() );
            t.write( result, value::Pointer( cloned ), result_i );
        }
        result.offset( 0 );
        return result;
    }

    using ObjSet = std::unordered_set< uint32_t >;

    template< typename Heap >
    void reachable( Heap &h, HeapPointer root, ObjSet &leakset, ObjSet &visited )
    {
        if ( !h.valid( root ) ) return; /* don't care */
        if ( visited.count( root.object() ) ) return;
        visited.insert( root.object() );
        leakset.erase( root.object() );
        auto root_i = h.ptr2i( root );

        for ( auto pos : h.pointers( root ) )
        {
            value::Pointer ptr;
            root.offset( pos.offset() );
            h.read( root, ptr, root_i );
            auto obj = ptr.cooked();
            obj.offset( 0 );
            if ( obj.heap() )
                reachable( h, obj, leakset, visited );
        }
    }

    template< typename Heap, typename F, typename... Roots >
    void leaked( Heap &h, F leak, Roots... roots )
    {
        ObjSet leakset, visited;
        auto check = [&]( auto s )
        {
            if ( h.valid( HeapPointer( s.first, 0 ) ) )
                leakset.insert( s.first );
        };

        for ( auto s = h.snap_begin(); s != h.snap_end(); ++s )
            check( *s );
        for ( auto s : h._l.exceptions )
            check( s );

        for ( auto r : { roots... } )
            reachable( h, r, leakset, visited );

        for ( auto o : leakset )
            leak( HeapPointer( o, 0 ) );
    }
}

namespace divine::vm::mem
{

    template< typename Self, typename PR >
    auto SimpleHeap< Self, PR >::snap_find( uint32_t obj ) const -> SnapItem *
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
                ASSERT( _objects.valid( pivot->second ) );
                return pivot;
            }
        }

        return begin;
    }

    template< typename Self, typename PR >
    PointerV SimpleHeap< Self, PR >::make( int size, uint32_t hint, bool overwrite )
    {
        HeapPointer p;
        SnapItem *search = snap_find( hint );
        bool found = false;
        while ( !found )
        {
            found = true;
            if ( _l.exceptions.count( hint ) )
                found = false;
            if ( search && search != snap_end() && search->first == hint )
                ++ search, found = false;
            if ( overwrite && _l.exceptions.count( hint ) )
                found = !_l.exceptions[ hint ].slab();
            if ( !found )
                ++ hint;
        }
        p.object( hint );
        p.offset( 0 );
        ASSERT( !ptr2i( p ).slab() );
        auto obj = _l.exceptions[ p.object() ] = _objects.allocate( size );
        _shadows.make( obj, size );
        self().made( p );
        return PointerV( p );
    }

    template< typename Self, typename PR >
    bool SimpleHeap< Self, PR >::resize( HeapPointer p, int sz_new )
    {
        if ( p.offset() || !valid( p ) )
            return false;
        int sz_old = size( p );
        auto obj_old = ptr2i( p );
        auto obj_new = _objects.allocate( sz_new );
        _shadows.make( obj_new, sz_new );
        copy( *this, p, obj_old, p, obj_new, std::min( sz_new, sz_old ) );
        _l.exceptions[ p.object() ] = obj_new;
        self().made( p ); /* fixme? */
        return true;
    }

    template< typename Self, typename PR >
    bool SimpleHeap< Self, PR >::free( HeapPointer p )
    {
        if ( !valid( p ) )
            return false;
        auto ex = _l.exceptions.find( p.object() );
        if ( ex == _l.exceptions.end() )
            _l.exceptions.emplace( p.object(), Internal() );
        else
        {
            _shadows.free( ex->second );
            _objects.free( ex->second );
            ex->second = Internal();
        }
        if ( p.offset() )
            return false;
        return true;
    }

    template< typename Self, typename PR >
    bool SimpleHeap< Self, PR >::shared( GenericPointer gp, bool sh )
    {
        if ( !gp.heap() || !valid( gp ) )
            return false;

        HeapPointer p = gp;
        auto i = ptr2i( p );

        if ( _shadows.shared( i ) == sh )
            return false; /* nothing to be done */

        auto detached = self().detach( p, i );
        _shadows.shared( detached ) = sh;
        bool rv = i != detached;

        if ( !sh )
            return rv;

        for ( auto pos : this->pointers( p, detached ) ) /* flood */
        {
            value::Pointer ptr;
            p.offset( pos.offset() );
            read( p, ptr, detached );
            rv = shared( ptr.cooked(), true ) || rv;
        }

        return rv;
    }

    template< typename Self, typename PR > template< typename T >
    auto SimpleHeap< Self, PR >::write( HeapPointer p, T t, Internal i ) -> Internal
    {
        i = self().detach( p, i );
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p, i ) - p.offset() );
        _shadows.write( shloc( p, i ), t );
        *_objects.template machinePointer< typename T::Raw >( i, p.offset() ) = t.raw();
        if ( t.pointer() && shared( p ) )
            if ( shared( value::Pointer( t ).cooked(), true ) )
                return Internal();
        return i;
    }

    template< typename Self, typename PR > template< typename T >
    void SimpleHeap< Self, PR >::read( HeapPointer p, T &t, Internal i ) const
    {
        using Raw = typename T::Raw;
        ASSERT( valid( p ), p );
        ASSERT_LEQ( sizeof( Raw ), size( p, i ) - p.offset() );

        t.raw( *_objects.template machinePointer< typename T::Raw >( i, p.offset() ) );
        _shadows.read( shloc( p, i ), t );
    }

    template< typename Self, typename PR > template< typename FromH >
    bool SimpleHeap< Self, PR >::copy( FromH &from_h, HeapPointer _from,
                                       typename FromH::Internal _from_i,
                                       HeapPointer _to, Internal &_to_i, int bytes )
    {
        if ( _from.null() || _to.null() )
            return false;
        _to_i = self().detach( _to, _to_i );
        int from_s( from_h.size( _from, _from_i ) ), to_s( size( _to, _to_i ) );
        auto from = from_h.unsafe_bytes( _from, _from_i, 0, from_s ),
            to = self().unsafe_bytes( _to, _to_i, 0, to_s );
        int from_off( _from.offset() ), to_off( _to.offset() );
        if ( !from.begin() || !to.begin() || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;

        auto from_heap_reader = [&from_h]( typename FromH::Internal obj, int off ) -> uint32_t {
            return *from_h._objects.template machinePointer< uint32_t >( obj, off );
        };
        auto this_heap_reader = [this]( Internal obj, int off ) -> uint32_t {
            return *_objects.template machinePointer< uint32_t >( obj, off );
        };
        _shadows.copy( from_h.shadows(), from_h.shloc( _from, _from_i ),
                       shloc( _to, _to_i ), bytes, from_heap_reader, this_heap_reader );

        std::copy( from.begin() + from_off, from.begin() + from_off + bytes, to.begin() + to_off );

        if ( _shadows.shared( _to_i ) )
            for ( auto pos : _shadows.pointers( shloc( _to, _to_i ), bytes ) )
            {
                value::Pointer ptr;
                if ( pos.size() == PointerBytes )
                {
                    _to.offset( to_off + pos.offset() );
                    read( _to, ptr, _to_i );
                }
                else
                {
                    ptr.cooked().object( pos.fragment() );
                }
                shared( ptr.cooked(), true );
            }

        return true;
    }

    inline brick::hash::hash64_t CowHeap::ObjHasher::content_only( Internal i )
    {
        auto size = objects().size( i );
        auto base = objects().dereference( i );

        brick::hash::jenkins::SpookyState high( 0, 0 );

        auto types = shadows().type( ShadowLoc( i, 0 ), size );
        auto t = types.begin();
        int offset = 0;

        while ( offset + 4 <= size )
        {
            if ( *t != ShadowType::Pointer ) /* NB. assumes little endian */
                high.update( base + offset, 4 );
            offset += 4;
            t += 4;
        }

        high.update( base + offset, size - offset );
        ASSERT_LEQ( offset, size );

        return high.finalize().first;
    }

    inline brick::hash::hash128_t CowHeap::ObjHasher::hash( Internal i )
    {
        /* TODO also hash some shadow data into low for better precision? */
        auto low = brick::hash::spooky( objects().dereference( i ), objects().size( i ) );
        return std::make_pair( ( content_only( i ) & 0xFFFFFFF000000000 ) | /* high 28 bits */
                            ( low.first & 0x0000000FFFFFFFF ), low.second );
    }

    inline bool CowHeap::ObjHasher::equal( Internal a, Internal b )
    {
        int size = objects().size( a );
        if ( objects().size( b ) != size )
            return false;
        if ( ::memcmp( objects().dereference( a ), objects().dereference( b ), size ) )
            return false;
        if ( shadows().shared( a ) != shadows().shared( b ) )
            return false;
        if ( !shadows().equal( a, b, size ) )
            return false;
        return true;
    }

    inline auto CowHeap::detach( HeapPointer p, Internal i ) -> Internal
    {
        if ( _ext.writable.count( p.object() ) )
            return i;
        ASSERT_EQ( _l.exceptions.count( p.object() ), 0 );
        _ext.writable.insert( p.object() );
        p.offset( 0 );
        int sz = size( p, i );
        auto oldloc = shloc( p, i );
        auto oldbytes = unsafe_bytes( p, i, 0, sz );

        auto obj = _objects.allocate( sz );
        _shadows.make( obj, sz );

        _l.exceptions[ p.object() ] = obj;
        auto newloc = shloc( p, obj );
        auto newbytes = unsafe_bytes( p, obj, 0, sz );

        auto heap_reader = [this]( Internal obj, int off ) -> uint32_t {
            return *_objects.template machinePointer< uint32_t >( obj, off );
        };
        _shadows.copy( oldloc, newloc, sz, heap_reader );
        std::copy( oldbytes.begin(), oldbytes.end(), newbytes.begin() );
        _shadows.shared( obj ) = _shadows.shared( i );
        return obj;
    }

    inline auto CowHeap::dedup( SnapItem si ) const -> SnapItem
    {
        auto r = _ext.objects.insert( si.second );
        if ( !r.isnew() )
        {
            ASSERT_NEQ( *r, si.second );
            _shadows.free( si.second );
            _objects.free( si.second );
        }
        si.second = *r;
        return si;
    }

    inline auto CowHeap::snapshot() const -> Snapshot
    {
        int count = 0;

        if ( _l.exceptions.empty() )
            return _l.snapshot;

        auto snap = snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != snap_end() && snap->first < except.first )
                ++ snap, ++ count;
            if ( snap != snap_end() && snap->first == except.first )
                snap++;
            if ( _objects.valid( except.second ) )
                ++ count;
        }

        while ( snap != snap_end() )
            ++ snap, ++ count;

        if ( !count )
            return Snapshot();

        auto s = _snapshots.allocate( count * sizeof( SnapItem ) );
        auto si = _snapshots.template machinePointer< SnapItem >( s );
        snap = snap_begin();

        for ( auto &except : _l.exceptions )
        {
            while ( snap != snap_end() && snap->first < except.first )
                *si++ = *snap++;
            if ( snap != snap_end() && snap->first == except.first )
                snap++;
            if ( _objects.valid( except.second ) )
                *si++ = dedup( except );
        }

        while ( snap != snap_end() )
            *si++ = *snap++;

        auto newsnap = _snapshots.template machinePointer< SnapItem >( s );
        ASSERT_EQ( si, newsnap + count );
        for ( auto s = newsnap; s < newsnap + count; ++s )
            ASSERT( _objects.valid( s->second ) );

        _l.exceptions.clear();
        _ext.writable.clear();
        _l.snapshot = s;

        return s;
    }

}

// vim: ft=cpp
