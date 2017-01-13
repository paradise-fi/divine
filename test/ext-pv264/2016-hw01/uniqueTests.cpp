/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <numeric>
#include "common.h"
#include "lazy.h"

namespace {

template< template< typename > class Struct >
void emptyStructure() {
    Struct< int > data{ 1 };

    auto u = lazy::unique( data.begin(), data.begin() );

    for ( int i : u ) {
        REQUIRE( false );
    }

    REQUIRE( 0 == std::accumulate( u.begin(), u.end(), 0 ) );
    REQUIRE( u.begin() == u.end() );
}

template< template< typename > class Struct >
void oneStructure() {
    Struct< int > data{ 1,1,1,1,1 };

    auto u = lazy::unique( data.begin(), data.end() );

    REQUIRE( ++u.begin() == u.end() );

    REQUIRE( 1 == std::accumulate( u.begin(), u.end(), 0 ) );
}
}

TEST_CASE( "unique::empty" ) {
    SECTION( "vector" ) {
        ::emptyStructure< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::emptyStructure< ::tests::List >();
    }
    SECTION( "set" ) {
        ::emptyStructure< ::tests::Set >();
    }
}
TEST_CASE( "unique::ones" ) {
    SECTION( "vector" ) {
        ::oneStructure< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::oneStructure< ::tests::List >();
    }
    SECTION( "set" ) {
        ::oneStructure< ::tests::Set >();
    }
}

namespace {

template< template< typename > class Struct >
void arrow() {
    Struct< std::string > texts{
        "a", "b", "hello", "world", "fuchsie"
    };

    auto u = lazy::unique( texts.begin(), texts.end() );

    for ( auto i = u.begin(); i != u.end(); ++i ) {
        REQUIRE( i->size() >= 1 );
        REQUIRE( ( *i ).size() >= 1 );
        REQUIRE( i->size() == ( *i ).size() );
    }
}

}

TEST_CASE( "unique::arrow" ) {
    SECTION( "vector" ) {
        ::arrow< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::arrow< ::tests::List >();
    }
    SECTION( "set" ) {
        ::arrow< ::tests::Set >();
    }
}


namespace {
template< template< typename > class Struct >
void operators() {
    Struct< std::string > data{ "hello", "dolly" };
    auto u = lazy::unique( data.begin(), data.end() );
    ::tests::iteratorManipulation( u.begin() );
    ::tests::iteratorContent( u.begin(), *data.begin() );
    ::tests::iteratorContent( ++u.begin(), *++data.begin() );
}
}

TEST_CASE( "unique::operators" ) {
    SECTION( "vector" ) {
        ::operators< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::operators< ::tests::List >();
    }
    SECTION( "set" ) {
        ::operators< ::tests::Set >();
    }
}

namespace {

template< template< typename > class Struct >
void filtering() {
    Struct< int > data{ 1,1,2,1,3,1,4,1,5,1,6,1,7,1,8,1,9,1,10 };

    auto u = lazy::unique( data.begin(), data.end() );
    tests::check( u.begin(), u.end(), { 1,2,3,4,5,6,7,8,9,10 } );
}
}

TEST_CASE( "unique::functioning" ) {
    SECTION( "vector" ) {
        ::filtering< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::filtering< ::tests::List >();
    }
    SECTION( "set" ) {
        ::filtering< ::tests::Set >();
    }
}

