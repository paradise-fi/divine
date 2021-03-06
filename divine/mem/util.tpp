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

        auto ptr_cb = [&]( auto p1_id, auto p2_id )
        {
            vm::GenericPointer p1( p1_id ), p2( p2_id );
            if ( int d = int( p1.type() ) - int( p2.type() ) )
                return cb.pointer( p1, p2 ), d;

            if ( p1.type() == Pointer::Type::Marked && h1.valid( p1 ) && h2.valid( p2 ) )
                cb.marked( p1, p2 );

            if ( p1.type() == Pointer::Type::Heap || p1.type() == Pointer::Type::Alloca )
                if ( int d = compare( h1, h2, p1, p2, v1, v2, seq, cb ) )
                    return d;

            if ( !p1.heap() )
                if ( int d = p1.object() - p2.object() )
                    return cb.pointer( r1, r2 ), d;

            return 0;
        };

        if ( int d = h2.compare( i1, i2, ptr_cb, s1 ) )
            return d;

        return 0;
    }

    struct NopState
    {
        template< typename T > void update_aligned( T ) {}
        template< bool = false > void update_aligned( uint8_t *, size_t ) {}
        void realign() {}
        hash64_t hash() const { return 0; }
    };

    template< typename Heap >
    void hash( Heap &heap, uint32_t root, std::unordered_map< int, int > &visited,
               brq::hash_state &state, int depth )
    {
        if ( auto seen = visited.find( root ); seen != visited.end() )
        {
            state.update_aligned( seen->second );
            return;
        }

        auto i = heap.ptr2i( root );
        if ( !heap.valid( i ) )
            return;

        int size = heap.size( i );
        uint32_t content_hash = i.tag();

        visited.emplace( root, content_hash );
        state.update_aligned( content_hash );

        if ( size > 64 * 1024 )
            return; /* skip the huge constants blobs */

        auto ptr_cb = [&]( uint32_t obj )
        {
            vm::GenericPointer ptr( obj, 0 );

            if ( ptr.type() == Heap::Pointer::Type::Heap ||
                 ptr.type() == Heap::Pointer::Type::Alloca )
                hash( heap, obj, visited, state, depth + 1 );

            if ( !ptr.heap() )
                state.update_aligned( ptr.object() );
        };

        NopState nop;
        heap.hash( root, nop, ptr_cb );
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
        for ( auto s : h.exceptions() )
            check( s );

        for ( auto r : { roots... } )
            reachable( h, r, leakset, visited );

        for ( auto o : leakset )
            leak( typename Heap::Pointer( o, 0 ) );
    }
}

// vim: ft=cpp
