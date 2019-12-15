// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/dbg/node.hpp>
#include <divine/vm/memory.hpp>
#include <brick-proc>

namespace divine {

namespace {

std::string text2dot( std::string_view s )
{
    std::string buf;
    buf.resize( s.length() * 2 );
    int i = 0, j = 0;
    while ( i < int( s.length() ) ) {
        char c = s[ i ++ ];
        if ( c == '\\' || c == '\n' || c == '"' )
            buf[ j++ ] = '\\';
        if ( c == '\n' )
            buf[ j++ ] = 'l';
        else
            buf[ j++ ] = c;
    }
    return std::string( buf, 0, j );
}

}

namespace dbg
{

using DNMap = std::map< DNKey, int >;

template< typename DN >
int dotDN( std::ostream &o, DN dn, DNMap &visited, int &seq, std::string prefix )
{
    if ( !dn._address.heap() )
        return 0;
    if ( visited.find( dn.sortkey() ) != visited.end() )
        return visited[ dn.sortkey() ];
    int hid = ++seq;
    visited.emplace( dn.sortkey(), hid );

    auto related =
        [&]( std::string k, auto rel )
        {
            if ( int t = dotDN( o, rel, visited, seq, prefix ) )
                o << prefix << hid << " -> "  << prefix << t
                  << " [ label=\"" << k << "\" ]" << std::endl;
        };

    std::function< void( std::string, DN ) > component =
        [&]( std::string ck, auto comp )
        {
            comp.related( [&]( std::string rk, auto rel )
                          { related( ck + ":" + rk, rel ); }, false );
            comp.components( [&]( std::string sk, auto scomp )
                             { component( ck + "." + sk, scomp ); } );
        };

    brq::string_builder str;
    dn.format( str, 10 );
    o << prefix << hid << " [ shape=rectangle label=\"" << text2dot( str.data() )
      << "\" ]" << std::endl;

    dn.related( related, false );
    dn.components( component );

    return hid;
}

template< typename DN >
std::string dotDN( DN dn, bool standalone, std::string prefix = "n" )
{
    std::stringstream str;
    DNMap visited;
    int seq = 0;
    if ( standalone )
        str << "digraph { node [ fontname = Courier ]\n";
    dotDN( str, dn, visited, seq, prefix );
    if ( standalone )
        str << "}";
    return str.str();
}

}
}
