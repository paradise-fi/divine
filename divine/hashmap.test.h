// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/hashmap.h>

namespace divine {
template<> bool valid( int t ) {
    return t != 0;
}
template<> hash_t hash( int t ) {
    return t;
}
}

namespace {

using namespace divine;

struct TestHashmap {
    Test basic() {
        HashMap< int, int > map( 32, 2 );
        map.insert( std::make_pair( 1, 2 ) );
        assert( map.has( 1 ) );
        assert_eq( map.value( map.get( 1 ) ), 2 );
    }
};

}
