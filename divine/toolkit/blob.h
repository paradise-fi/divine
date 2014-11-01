// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net

#include <cstdint>

#include <brick-unittest.h>
#include <brick-string.h>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/rpc.h>

#ifndef DIVINE_BLOB_H
#define DIVINE_BLOB_H

namespace divine {

inline std::string fmtblob( Pool &p, Blob b ) {
    divine::bitstream bs;
    bs.pool = &p;
    bs << b;
    bs.bits.pop_front(); /* remove size */
    return brick::string::fmt( bs.bits );
}

}

namespace divine_test {

using namespace divine;

struct TestBlob {

    TEST(basic) {
        Pool p;
        Blob b = p.allocate( 32 );
        p.get< int >( b ) = 42;
        ASSERT_EQ( p.get< int >( b ), 42 );

        Blob c = p.allocate( sizeof( int ) );
        p.get< int >( c ) = 42;
        ASSERT_EQ( p.get< int >( c ), 42 );
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
        ASSERT_EQ( p.get< T >( b1 ), 32 );
        ASSERT_EQ( p.get< T >( b2 ), 32 );
        ASSERT_EQ( p.get< T >( b3 ), 42 );
        ASSERT( ecomp( b1, b2 ) );
        ASSERT( !ecomp(b1, b3) );
        ASSERT( !ecomp(b2, b3) );
        ASSERT( !lecomp(b1, b2) );
        ASSERT( !lecomp(b2, b1) );
        ASSERT( lecomp( b1, b3 ) );
    }

    TEST(comparisonShort) {
        comparison< short >();
    }

    TEST(comparisonInt) {
        comparison< int >();
    }

    TEST(comparisonChar) {
        comparison< char >();
    }

    TEST(align) {
        for ( int i = 0; i < 100; ++i )
            ASSERT_LEQ( i, divine::align( i, sizeof( void * ) ) );
    }

};

}
#endif
