/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <numeric>
#include "common.h"
#include "lazy.h"

namespace {

template< template< typename > class Struct >
void emptyStructure() {
    Struct< int > data{1};
    int executions = 0;

    auto f = lazy::filter( data.begin(), data.begin(), [&]( int ) {
        ++executions;
        return true;
    } );

    for ( int i : f ) {
        REQUIRE( false );
    }

    REQUIRE( 0 == std::accumulate( f.begin(), f.end(), 0 ) );
    REQUIRE( 0 == executions );
    REQUIRE( f.begin() == f.end() );
}

template< template< typename > class Struct >
void filteredStructure() {
    Struct< int > data{1,2,3,4,5,6,7,8,9,0};

    auto f = lazy::filter( data.begin(), data.end(), []( int ) {
        return false;
    } );

    REQUIRE( f.begin() == f.end() );

    for ( int i : f ) {
        REQUIRE( false );
    }

    REQUIRE( 0 == std::accumulate( f.begin(), f.end(), 0 ) );
}
}

TEST_CASE( "filter::empty" ) {
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
TEST_CASE( "filer::filtered" ) {
    SECTION( "vector" ) {
        ::filteredStructure< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::filteredStructure< ::tests::List >();
    }
    SECTION( "set" ) {
        ::filteredStructure< ::tests::Set >();
    }
}

namespace {

template< template< typename > class Struct >
void arrow() {
    Struct< std::string > texts{
        "a", "b", "hello", "world", "fuchsie"
    };

    size_t executions = 0;
    auto f = lazy::filter( texts.begin(), texts.end(), [&]( const std::string &s ) {
        ++executions;
        return s.size() > 2;
    } );

    for ( auto i = f.begin(); i != f.end(); ++i ) {
        REQUIRE( i->size() > 2 );
        REQUIRE( (*i).size() > 2 );
    }
    REQUIRE( executions == texts.size() );
}

}

TEST_CASE( "filter::arrow" ) {
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
    auto f = lazy::filter( data.begin(), data.end(), []( const std::string & ) {
        return true;
    } );
    ::tests::iteratorManipulation( f.begin() );
    ::tests::iteratorContent( f.begin(), *data.begin() );
    ::tests::iteratorContent( ++f.begin(), *++data.begin() );
}
}

TEST_CASE( "filter::operators" ) {
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
    Struct< int > data{ 1,2,3,4,5,6,7,8,9,10 };

    size_t executions = 0;
    auto f = lazy::filter( data.begin(), data.end(), [&]( int i ) {
        ++executions;
        return i % 2 == 1;
    } );
    tests::check( f.begin(), f.end(), { 1,3,5,7,9 } );
    REQUIRE( executions == data.size() );
}
}

TEST_CASE( "filter::functioning" ) {
    SECTION( "vector" ) {
        ::filtering< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::filtering< ::tests::List >();
    }
    SECTION( "set" ) {
        ::filtering< ::tests::Set >( );
    }
}

