// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/hashset.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/pool.h>

using namespace divine;

struct TestHashset {
    Test basic() {
        HashSet< int > set;
        set.insert( 1 );
        assert( set.has( 1 ) );
    }

    Test stress() {
        HashSet< int > set;
        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            assert( set.has( i ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.has( i ) );
        }
    }

    Test set() {
        HashSet< int > set;

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( !set.has( i ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            set.insert( i );
            assert( set.has( i ) );
            assert( !set.has( i + 1 ) );
        }

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.has( i ) );
        }

        for ( int i = 32*1024; i < 64 * 1024; ++i ) {
            assert( !set.has( i ) );
        }
    }

    Test blobish() {
        Pool p;
        HashSet< Blob > set( ( typename HashSet< Blob >::Hasher( p ) ) );
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b( sizeof( int ) );
            p.get< int >( b ) = i;
            set.insert( b );
            assert( set.has( b ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b( sizeof( int ) );
            p.get< int >( b ) = i;
            assert( set.has( b ) );
        }
    }
};
