// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/blob.h>
#include <divine/pool.h>

using namespace divine;

struct TestBlob {
    Test basic() {
        Blob b( 32 );
        b.get< int >() = 42;
        assert_eq( b.get< int >(), 42 );

        Blob c( sizeof( int ) );
        c.get< int >() = 42;
        assert_eq( c.get< int >(), 42 );
    }

    template< typename T >
    void comparison() {
        Blob b1( sizeof( T ) ), b2( sizeof( T ) ), b3( sizeof( T ) );
        b1.get< T >() = 32;
        b2.get< T >() = 32;
        b3.get< T >() = 42;
        assert_eq( b1.get< T >(), 32 );
        assert_eq( b2.get< T >(), 32 );
        assert_eq( b3.get< T >(), 42 );
        assert( b1 < b3 );
        assert( !(b1 < b2) );
        assert( !(b2 < b1) );
        assert( b1 == b2 );
        assert( !(b1 == b3) );
        assert( !(b2 == b3) );
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

    Test allocSize() {
        for ( int i = 0; i < 100; ++i )
            assert( i <= Blob::allocationSize( i ) );
    }

    Test writeAndRead() {
        Pool p;
        Blob b1( p, sizeof( int ) ), b2;
        int32_t buf[ 2 ];

        b1.get< int >() = 42;
        b1.write32( buf );
        int32_t *end = b2.read32( &p, buf );
        assert_eq( b1.get< int >(), 42 );
        assert_eq( b1.get< int >(), b2.get< int >() );
        assert( b1 == b2 );

        assert_eq( buf + 2, end );
    }
};
