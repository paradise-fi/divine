// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_DUMMY_H
#define DIVINE_GENERATOR_DUMMY_H

namespace divine {
namespace generator {

struct Dummy : Common< Blob > {
    typedef std::pair< short, short > Content;
    typedef Blob Node;

    struct Label {
        short probability;
        Label( short p = 0 ) : probability( p ) {}
    };

    Content &content( Blob b ) {
        return b.get< Content >( alloc._slack );
    }

    Node initial() {
        Blob b = alloc.new_blob( sizeof( Content ) );
        content( b ) = std::make_pair( 0, 0 );
        return b;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial(), Label() );
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        Node r;

        if ( !st.valid() )
            return;

        if ( content( st ).first < 512 ) {
            r = alloc.new_blob( sizeof( Content ) );
            content( r ) = content( st );
            content( r ).first ++;
            yield( r, Label( 7 ) );
        }

        if ( content( st ).second < 512 ) {
            r = alloc.new_blob( sizeof( Content ) );
            content( r ) = content( st );
            content( r ).second ++;
            yield( r, Label( 3 ) );
        }
    }

    void release( Node s ) {
        s.free( pool() );
    }

    bool isGoal( Node s ) {
        Content f = s.get< Content >( alloc._slack );
        return f.first == 512;
    }

    bool isAccepting( Node s ) { return false; }
    std::string showNode( Node s ) {
        if ( !s.valid() )
            return "[]";
        Content f = s.get< Content >( alloc._slack );
        std::stringstream stream;
        stream << "[" << f.first << ", " << f.second << "]";
        return stream.str();
    }

    /// currently only dummy method
    std::string showTransition( Node from, Node to, Label act ) {
        if (act.probability == 3)
	  return "3";
	else
	  return "7";
    }

    void read( std::string ) {}
};

}
}

#endif
