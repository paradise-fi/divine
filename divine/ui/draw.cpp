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

#include <divine/vm/explore.hpp>
#include <divine/vm/bitcode.hpp>
#include <divine/vm/debug.hpp>
#include <divine/ss/search.hpp>
#include <divine/ui/cli.hpp>

namespace divine {
namespace ui {

std::string escape( std::string s )
{
    std::string buf;
    buf.resize( s.length() * 2 );
    int i = 0, j = 0;
    while ( i < int( s.length() ) ) {
        char c = s[ i ++ ];
        if ( c == '\\' || c == '\n' || c == '"' )
            buf[ j++ ] = '\\';
        if ( c == '\n' )
            buf[ j++ ] = 'n';
        else
            buf[ j++ ] = c;
    }
    return std::string( buf, 0, j );
}

template< typename DN >
int dump( DN dn, std::map< vm::GenericPointer, int > &dumped, int &seq, std::string prefix )
{
    if ( dn._address.type() != vm::PointerType::Heap )
        return 0;
    if ( dumped.find( dn._address ) != dumped.end() )
        return dumped[ dn._address ];
    int hid = ++seq;
    dumped.emplace( dn._address, hid );
    std::cout << prefix << hid << " [ shape=rectangle label=\"";
    dn.attributes( []( std::string k, std::string v )
                   { if ( k == "@raw" ) std::cout << escape( v ) << std::endl; } );
    std::cout << "\" ]" << std::endl;
    dn.related( [&]( std::string k, auto rel )
                {
                    if ( int t = dump( rel, dumped, seq, prefix ) )
                        std::cout << prefix << hid << " -> "  << prefix << t
                                  << " [ label=" << k << " ]" << std::endl;
                } );
    return hid;
}

void Draw::run()
{
    NOT_IMPLEMENTED();
#if 0
    vm::Explore ex( _bc );
    int edgecount = 0, statecount = 0;

    std::map< vm::explore::State, int > _ids;

    int seq = 0;

    std::cout << "digraph { node [ fontname = Courier ]\n";

    ss::search(
        ss::Order::PseudoBFS, ex, 1, ss::passive_listen(
            [&]( auto f, auto t, auto trace )
            {
                int f_id = _ids[ f ], t_id = _ids[ t ];
                if ( !t_id )
                    t_id = _ids[ t ] = ++seq;
                std::cout << f_id << " -> " << t_id
                          << " [ label = \"" << escape( brick::string::fmt( trace ) ) << "\"]"
                          << std::endl;
                ++statecount;
            },
            [&]( auto st )
            {
                int id = _ids[ st ];
                vm::DebugNode< vm::explore::Context > dn( ex._ctx, ex._ctx.heap().snapshot(),
                                                          st.root, 0, vm::DNKind::Object, nullptr, nullptr );
                std::map< vm::GenericPointer, int > _dumped;
                int hseq = 0;
                std::cerr << "# new state" << std::endl;
                std::cout << id << " [ style=filled fillcolor=yellow ]" << std::endl;
                std::cout << id << " -> " << id << ".1 [ label=root ]" << std::endl;
                dump( dn, _dumped, hseq, brick::string::fmt( id ) + "." );
                ++edgecount;
            } ) );
    std::cout << "}";
    std::cerr << "found " << statecount << " states and " << edgecount << " edges" << std::endl;
#endif
}

}
}
