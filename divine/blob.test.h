// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/blob.h>

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

    Test comparison() {
        Blob b1( 4 ), b2( 4 ), b3( 4 );
        b1.get< int >() = 32;
        b2.get< int >() = 32;
        b3.get< int >() = 42;
        assert_eq( b1.get< int >(), 32 );
        assert_eq( b2.get< int >(), 32 );
        assert_eq( b3.get< int >(), 42 );
        assert( b1 < b3 );
        assert( !(b1 < b2) );
        assert( !(b2 < b1) );
        assert( b1 == b2 );
        assert( !(b1 == b3) );
        assert( !(b2 == b3) );
    }
};
