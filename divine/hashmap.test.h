// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/hashmap.h>
#include <divine/blob.h>

using namespace divine;

struct TestHashmap {
    Test basic() {
        HashMap< int, int > map;
        map.insert( std::make_pair( 1, 2 ) );
        assert( map.has( 1 ) );
        assert_eq( map.value( map.get( 1 ) ), 2 );
    }

    Test stress() {
        HashMap< int, int > map;
        for ( int i = 1; i < 32*1024; ++i ) {
            map.insert( std::make_pair( i, 2*i ) );
            assert( map.has( i ) );
            assert_eq( map.value( map.get( i ) ), 2*i );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            assert( map.has( i ) );
            assert_eq( map.value( map.get( i ) ), 2*i );
        }
    }

    Test set() {
        HashMap< int, Unit > set;
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
        HashMap< Blob, Unit > set;
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b( sizeof( int ) );
            b.get< int >() = i;
            set.insert( b );
            assert( set.has( b ) );
        }
        for ( int i = 1; i < 32*1024; ++i ) {
            Blob b( sizeof( int ) );
            b.get< int >() = i;
            assert( set.has( b ) );
        }
    }
};
