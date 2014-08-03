// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/pool.h>

using namespace divine;

struct TestBlob {

    Test basic() {
        Pool p;
        Blob b = p.allocate( 32 );
        p.get< int >( b ) = 42;
        assert_eq( p.get< int >( b ), 42 );

        Blob c = p.allocate( sizeof( int ) );
        p.get< int >( c ) = 42;
        assert_eq( p.get< int >( c ), 42 );
    }

    template< typename T >
    void comparison() {
        Pool p;
        Blob b1 = p.allocate( sizeof( T ) ),
             b2 = p.allocate( sizeof( T ) ),
             b3 = p.allocate( sizeof( T ) );
        BlobComparerEQ ecomp( p );
        BlobComparerLT lecomp( p );
        p.get< T >( b1 ) = 32;
        p.get< T >( b2 ) = 32;
        p.get< T >( b3 ) = 42;
        assert_eq( p.get< T >( b1 ), 32 );
        assert_eq( p.get< T >( b2 ), 32 );
        assert_eq( p.get< T >( b3 ), 42 );
        assert( ecomp( b1, b2 ) );
        assert( !ecomp(b1, b3) );
        assert( !ecomp(b2, b3) );
        assert( !lecomp(b1, b2) );
        assert( !lecomp(b2, b1) );
        assert( lecomp( b1, b3 ) );
    }

    Test comparisonShort() {
        comparison< short >();
    }

    Test comparisonInt() {
        comparison< int >();
    }

    Test comparisonChar() {
        comparison< char >();
    }

    Test align() {
        for ( int i = 0; i < 100; ++i )
            assert_leq( i, divine::align( i, sizeof( void * ) ) );
    }

};
