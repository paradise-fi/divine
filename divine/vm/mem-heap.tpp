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

#include <divine/vm/mem-heap.hpp>

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

        auto i1 = h1.ptr2i( r1 ), i2 = h2.ptr2i( r2 );
        int s1 = h1.size( i1 ), s2 = h2.size( i2 );
        if ( s1 - s2 )
            return s1 - s2;

        int shadow_cmp = h2.compare( h1, i1, i2, s1 );
        if ( shadow_cmp )
            return shadow_cmp;

        auto l1 = h1.loc( r1, i1 ), l2 = h2.loc( r2, i2 );
        auto b1 = h1.unsafe_bytes( l1 ), b2 = h2.unsafe_bytes( l2 );
        auto p1 = h1.pointers( l1, s1 ), p2 = h2.pointers( l2, s2 );
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
        int size = heap.size( i );
        uint32_t content_hash = i.tag();

        visited.emplace( root.object(), content_hash );

        if ( depth > 8 )
            return content_hash;

        if ( size > 16 * 1024 )
            return content_hash; /* skip following pointers in objects over 16k, not worth it */

        int ptr_data[2];
        auto pointers = heap.pointers( heap.loc( root, i ), size );
        for ( auto pos : pointers )
        {
            value::Pointer ptr;
            ASSERT_LT( pos.offset(), heap.size( root ) );
            root.offset( pos.offset() );
            heap.unsafe_read( root, ptr, i );
            auto obj = ptr.cooked();
            ptr_data[0] = 0;
            ptr_data[1] = obj.offset();
            if ( obj.type() == PointerType::Heap )
            {
                auto vis = visited.find( obj.object() );
                if ( vis == visited.end() )
                {
                    obj.offset( 0 );
                    if ( heap.valid( obj ) )
                        ptr_data[0] = hash( heap, obj, visited, state, depth + 1 );
                    /* else freed object, ignore */
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

        auto root_i = f.loc( root );
        if ( overwrite )
            t.free( root );
        /* FIXME make the result weak if root is */
        auto result = t.make( f.size( root ), root.object(), true ).cooked();
        if ( overwrite )
            ASSERT_EQ( root.object(), result.object() );
        auto result_i = t.loc( result );
        visited.emplace( root, result );

        t.copy( f, root, result, f.size( root ) );

        for ( auto pos : f.pointers( root ) )
        {
            value::Pointer ptr, ptr_c;
            GenericPointer cloned;
            root_i.offset = pos.offset();
            result_i.offset = pos.offset();

            f.read( root_i, ptr );
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
            t.write( result_i, value::Pointer( cloned ) );
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
        auto root_i = h.loc( root );

        for ( auto pos : h.pointers( root ) )
        {
            value::Pointer ptr;
            root_i.offset = pos.offset();
            h.read( root_i, ptr );
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

    template< typename Next >
    auto Storage< Next >::snap_find( uint32_t obj ) const -> SnapItem *
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

    template< typename Next >
    typename Storage< Next >::Loc Storage< Next >::make( int size, uint32_t hint, bool overwrite )
    {
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
        ASSERT( !ptr2i( hint ).slab() );
        auto obj = _l.exceptions[ hint ] = objects().allocate( size );
        Next::materialise( obj, size );
        return Loc( obj, hint, 0 );
    }

    template< typename Next >
    bool Storage< Next >::resize( HeapPointer p, int sz_new )
    {
        if ( p.offset() || !valid( p ) )
            return false;
        auto obj_old = ptr2i( p );
        int sz_old = size( obj_old );
        auto obj_new = objects().allocate( sz_new );

        Next::materialise( obj_new, sz_new );
        copy( *this, loc( p, obj_old ), *this, loc( p, obj_new ), std::min( sz_new, sz_old ) );
        _l.exceptions[ p.object() ] = obj_new;
        return true;
    }

    template< typename Next >
    bool Storage< Next >::free( HeapPointer p )
    {
        if ( !valid( p ) )
            return false;
        auto ex = _l.exceptions.find( p.object() );
        if ( ex == _l.exceptions.end() )
            _l.exceptions.emplace( p.object(), Internal() );
        else
        {
            Next::free( ex->second );
            objects().free( ex->second );
            ex->second = Internal();
        }
        if ( p.offset() )
            return false;
        return true;
    }

    template< typename Next > template< typename T >
    void Storage< Next >::write( Loc l, T t )
    {
        using Raw = typename T::Raw;
        ASSERT_LEQ( sizeof( Raw ), size( l.object ) - l.offset );
        Next::write( l, t );
        *objects().template machinePointer< Raw >( l.object, l.offset ) = t.raw();
    }

    template< typename Next > template< typename T >
    void Storage< Next >::read( Loc l, T &t ) const
    {
        using Raw = typename T::Raw;
        ASSERT_LEQ( sizeof( Raw ), size( l.object ) - l.offset );
        t.raw( *objects().template machinePointer< Raw >( l.object, l.offset ) );
        Next::read( l, t );
    }

    template< typename Next > template< typename FromH, typename ToH >
    bool Storage< Next >::copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int bytes )
    {
        int  from_s = from_h.size( from.object ),
             to_s   = to_h.size( to.object );
        auto from_b = from_h.unsafe_ptr2mem( from.object ),
             to_b   =   to_h.unsafe_ptr2mem( to.object );
        int  from_off = from.offset,
             to_off   = to.offset;

        if ( !from_b || !to_b || from_off + bytes > from_s || to_off + bytes > to_s )
            return false;

        Next::copy( from_h, from, to_h, to, bytes );
        std::copy( from_b + from_off, from_b + from_off + bytes, to_b + to_off );

        return true;
    }

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
        auto res = this->copy( *this, loc, *this, Loc( newobj, loc.objid, 0 ), sz );
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
