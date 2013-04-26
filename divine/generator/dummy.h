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
        return pool().get< Content >( b, slack() );
    }

    template< typename Yield >
    void initials( Yield yield ) {
        Blob b = this->makeBlob( sizeof( Content ) );
        content( b ) = std::make_pair( 0, 0 );
        yield( Node(), b, Label() );
    }

    template< typename N, typename Yield >
    void successors( N st, Yield yield )
    {
        return successors( st.getNode( this->pool() ), yield );
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        Node r;

        if ( !pool().valid( st ) )
            return;

        if ( content( st ).first < 512 ) {
            r = this->makeBlob( sizeof( Content ) );
            content( r ) = content( st );
            content( r ).first ++;
            yield( r, Label( 7 ) );
        }

        if ( content( st ).second < 512 ) {
            r = this->makeBlob( sizeof( Content ) );
            content( r ) = content( st );
            content( r ).second ++;
            yield( r, Label( 3 ) );
        }
    }

    void release( Node s ) {
        pool().free( s );
    }

    bool isGoal( Node s ) {
        Content f = pool().get< Content >( s, this->slack() );
        return f.first == 512;
    }

    bool isAccepting( Node s ) { return false; }
    std::string showNode( Node s ) {
        if ( !pool().valid( s ) )
            return "[]";
        Content f = pool().get< Content >( s, this->slack() );
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

    void read( std::string, std::vector< std::string >, Dummy * = nullptr ) {}
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, Dummy::Label l ) { return bs << l.probability; }

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, Dummy::Label &l ) { return bs >> l.probability; }

}
}

#endif
