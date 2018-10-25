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

#include <divine/mem/util.hpp>
#include <unordered_map>

namespace divine::mem
{

    template< typename H1, typename H2, typename CB >
    int compare( H1 &h1, H2 &h2, typename H1::Pointer r1, typename H1::Pointer r2,
                 std::unordered_map< int, int > &v1,
                 std::unordered_map< int, int > &v2, int &seq, CB &cb )
    {
        using Pointer = typename H1::Pointer;

        r1.offset( 0 ); r2.offset( 0 );

        auto v1r1 = v1.find( r1.object() );
        auto v2r2 = v2.find( r2.object() );

        if ( v1r1 != v1.end() && v2r2 != v2.end() )
        {
            auto d = v1r1->second - v2r2->second;
            if ( d )
                cb.structure( r1, r2, v1r1->second, v2r2->second );
            return d;
        }

        if ( v1r1 != v1.end() )
            return cb.structure( r1, r2, v1r1->second, 0 ), -1;
        if ( v2r2 != v2.end() )
            return cb.structure( r1, r2, 0, v2r2->second ), 1;

        v1[ r1.object() ] = seq;
        v2[ r2.object() ] = seq;
        ++ seq;

        if ( int d = h1.valid( r1 ) - h2.valid( r2 ) )
            return cb.structure( r1, r2, -h1.valid( r1 ), -h2.valid( r2 ) ), d;

        if ( !h1.valid( r1 ) )
            return 0;

        auto i1 = h1.ptr2i( r1 ), i2 = h2.ptr2i( r2 );
        int s1 = h1.size( i1 ), s2 = h2.size( i2 );

        if ( auto d = s1 - s2 )
            return cb.size( r1, r2, s1, s2 ), d;

        if ( int d = h2.compare( h1, i1, i2, s1 ) )
            return cb.shadow( r1, r2 ), d;

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
                if ( int d = b1[ offset ] - b2[ offset ] )
                    return cb.bytes( r1, r2, offset ), d;
                ++ offset;
            }

            if ( p1i == p1.end() )
                return 0;

            if ( int d = p1i->size() - p2i->size() )
                return cb.pointer( r1, r2, offset ), d;

            offset += p1i->size();

            ASSERT_EQ( p1i->offset(), p2i->offset() );
            r1.offset( p1i->offset() );
            r2.offset( p1i->offset() );

            /* recurse */
            typename H1::PointerV::Cooked p1pp, p2pp;

            if ( p1i->size() == 8 )
            {
                typename H1::PointerV p1p, p2p;
                h1.unsafe_read( r1, p1p, i1 );
                h2.unsafe_read( r2, p2p, i2 );
                p1pp = p1p.cooked();
                p2pp = p2p.cooked();
            } else if ( p1i->size() == 1 )
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


            /* offsets and types must always match */
            if ( int d = int( p1pp.type() ) - int( p2pp.type() ) )
                return cb.pointer( r1, r2, offset ), d;
            if ( int d = p1pp.offset() - p2pp.offset() )
                return cb.pointer( r1, r2, offset ), d;

            if ( p1pp.type() == Pointer::Type::Marked )
                cb.marked( p1pp, p2pp );

            if ( p1pp.type() == Pointer::Type::Heap )
                if ( int d = compare( h1, h2, p1pp, p2pp, v1, v2, seq, cb ) )
                    return d;

            if ( !p1pp.heap() )
                if ( int d = p1pp.object() - p2pp.object() )
                    return cb.pointer( r1, r2, offset ), d;

            ++ p1i; ++ p2i;
        }

        UNREACHABLE( "heap comparison fell through" );
    }

    template< typename Heap >
    int hash( Heap &heap, typename Heap::Pointer root,
              std::unordered_map< int, int > &visited,
              brick::hash::jenkins::SpookyState &state, int depth )
    {
        using PointerV = typename Heap::PointerV;

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
            PointerV ptr;
            ASSERT_LT( pos.offset(), heap.size( root ) );
            root.offset( pos.offset() );
            heap.unsafe_read( root, ptr, i );
            auto obj = ptr.cooked();
            ptr_data[0] = 0;
            ptr_data[1] = obj.offset();
            if ( obj.type() == Heap::Pointer::Type::Heap )
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
    auto clone( FromH &f, ToH &t, typename FromH::Pointer root,
                std::map< typename FromH::Pointer, typename FromH::Pointer > &visited,
                CloneType ct ) -> typename FromH::Pointer
    {
        using PointerV = typename FromH::PointerV;
        using Pointer = typename FromH::Pointer;

        if ( root.null() )
            return root;
        if ( !f.valid( root ) )
            return root;
        auto seen = visited.find( root );
        if ( seen != visited.end() )
            return seen->second;

        auto root_i = f.loc( root );
        /* FIXME make the result weak if root is */
        auto result = t.make( f.size( root ), root.object(), true ).cooked();
        auto result_i = t.loc( result );
        visited.emplace( root, result );

        t.copy( f, root, result, f.size( root ) );

        for ( auto pos : f.pointers( root ) )
        {
            PointerV ptr, ptr_c;
            root_i.offset = pos.offset();
            result_i.offset = pos.offset();

            f.read( root_i, ptr );
            auto obj = ptr.cooked();
            decltype( obj ) cloned;
            obj.offset( 0 );
            if ( obj.heap() )
                ASSERT( ptr.pointer() );
            if ( ct == CloneType::SkipWeak && obj.type() == Pointer::Type::Weak )
                cloned = obj;
            else if ( ct == CloneType::HeapOnly && obj.type() != Pointer::Type::Heap )
                cloned = obj;
            else if ( obj.heap() )
                cloned = clone( f, t, obj, visited, ct );
            else
                cloned = obj;
            cloned.offset( ptr.cooked().offset() );
            t.write( result_i, PointerV( cloned ) );
        }
        result.offset( 0 );
        return result;
    }

    using ObjSet = std::unordered_set< uint32_t >;

    template< typename Heap >
    void reachable( Heap &h, typename Heap::Pointer root, ObjSet &leakset, ObjSet &visited )
    {
        using PointerV = typename Heap::PointerV;

        if ( !h.valid( root ) ) return; /* don't care */
        if ( visited.count( root.object() ) ) return;
        visited.insert( root.object() );
        leakset.erase( root.object() );
        auto root_i = h.loc( root );

        for ( auto pos : h.pointers( root ) )
        {
            PointerV ptr;
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
            if ( h.valid( typename Heap::Pointer( s.first, 0 ) ) )
                leakset.insert( s.first );
        };

        for ( auto s = h.snap_begin(); s != h.snap_end(); ++s )
            check( *s );
        for ( auto s : h._l.exceptions )
            check( s );

        for ( auto r : { roots... } )
            reachable( h, r, leakset, visited );

        for ( auto o : leakset )
            leak( typename Heap::Pointer( o, 0 ) );
    }
}

// vim: ft=cpp
