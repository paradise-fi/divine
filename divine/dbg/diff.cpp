// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/dbg/node.hpp>
#include <optional>

namespace divine::dbg
{

    using DNSet = std::set< DNKey >;
    using Path = std::vector< std::string >;

    Path operator+( Path a, std::string v )
    {
        a.push_back( v );
        return a;
    }

    std::ostream &operator<<( std::ostream &o, Path p )
    {
        for ( auto c : p ) o << c;
        return o;
    }

    template< typename Program, typename Heap >
    void diff( std::ostream &out, Node< Program, Heap > a, Node< Program, Heap > b,
               Path p, DNSet &visited )
    {
        using DNode = Node< Program, Heap >;

        if ( visited.count( a.sortkey() ) ) return;
        visited.insert( a.sortkey() );

        std::map< std::string_view, std::pair< std::string_view, std::string_view > > attrs;

        a.attributes( [&]( auto k, auto v ) { attrs[ k ].first = v; } );
        b.attributes( [&]( auto k, auto v ) { attrs[ k ].second = v; } );

        bool announced = false;
        auto ann = [&]
        {
            if ( announced ) return;
            announced = true;
            out << p << ":" << std::endl;
        };

        if ( attrs[ "raw" ].first != attrs[ "raw" ].second ) /* TODO print details */
            ann(), out << "  (content differences)" << std::endl;
        attrs.erase( "raw" );

        for ( auto i : attrs )
            if ( i.second.first != i.second.second && !i.second.first.empty() )
                ann(), out << "  - " << i.first << ": " << i.second.first << std::endl;

        for ( auto i : attrs )
            if ( i.second.first != i.second.second && !i.second.second.empty() )
                ann(), out << "  + " << i.first << ": " << i.second.second << std::endl;

        std::map< std::string, std::pair< std::optional< DNode >, std::optional< DNode > > > subs;

        a.components( [&]( std::string k, auto n ) { subs[ "." + k ].first = n; } );
        b.components( [&]( std::string k, auto n ) { subs[ "." + k ].second = n; } );
        a.related( [&]( std::string k, auto n ) { subs[ ":" + k ].first = n; } );
        b.related( [&]( std::string k, auto n ) { subs[ ":" + k ].second = n; } );

        for ( auto i : subs )
        {
            auto a = i.second.first;
            auto b = i.second.second;
            if ( a.has_value() && !b.has_value() )
                ann(), out << "  - " << i.first << std::endl;
            if ( !a.has_value() && b.has_value() )
                ann(), out << "  + " << i.first << std::endl;
        }

        for ( auto i : subs )
        {
            auto a = i.second.first;
            auto b = i.second.second;
            if ( a.has_value() && b.has_value() )
                diff( out, a.value(), b.value(), p + i.first, visited );
        }
    }

    template< typename Program, typename Heap >
    void diff( std::ostream &out, Node< Program, Heap > a, Node< Program, Heap > b )
    {
        DNSet visited;
        diff( out, a, b, {}, visited );
    }

    using DNode = Node< vm::Program, vm::CowHeap >;
    template void diff< vm::Program, vm::CowHeap >( std::ostream &out, DNode a, DNode b );
}
