/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <vector>
#include <initializer_list>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "lazy.h"

namespace {

template< typename I, typename T >
void check( I first, I last, std::initializer_list< T > expected ) {
    bool result = std::equal( first, last, expected.begin(), expected.end() );
    REQUIRE( result );
}

}

TEST_CASE( "students tests of iterators", "[student]" ) {

    std::vector< int > data{ 1,2,3,4,5,6,7,8,9,1 };

    SECTION( "map" ) {
        auto m = lazy::map( data.begin(), data.end(), []( int i ) {
            return i + 1;
        } );
        check( m.begin(), m.end(), { 2,3,4,5,6,7,8,9,10,2 } );
    }
    SECTION( "filter" ) {
        auto f = lazy::filter( data.begin(), data.end(), []( int i ) {
            return i % 2 == 0;
        } );
        check( f.begin(), f.end(), { 2,4,6,8 } );
    }
    SECTION( "zip" ) {
        auto z = lazy::zip( data.begin(), data.end(), data.begin(), data.end(), []( int a, int b ) {
            return a + b;
        } );
        check( z.begin(), z.end(), { 2,4,6,8,10,12,14,16,18,2 } );
    }
    SECTION( "unique" ) {
        auto u = lazy::unique( data.begin(), data.end() );
        check( u.begin(), u.end(), { 1,2,3,4,5,6,7,8,9 } );
    }
}


