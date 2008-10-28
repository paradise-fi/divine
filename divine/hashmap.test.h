// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/hashmap.h>

namespace {

using namespace divine;

struct TestHashmap {
    Test basic() {
        HashMap< int, int > map( 32, 2 );
        map.insert( std::make_pair( 1, 2 ) );
        assert( map.has( 1 ) );
        assert_eq( map.value( map.get( 1 ) ), 2 );
    }

    Test stress() {
        HashMap< int, int > map( 32, 2 );
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
};

}
