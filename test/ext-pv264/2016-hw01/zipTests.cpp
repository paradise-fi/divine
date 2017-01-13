/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <numeric>
#include <map>
#include "common.h"
#include "lazy.h"
#include "sequence.h"

namespace {

template< template< typename > class Struct >
void emptyStructure() {
    Struct< int > data{ 1 };
    Struct< int > data2;
    int executions = 0;

    auto z = lazy::zip( data.begin(), data.end(),
                        data2.begin(), data2.end(),
                        [&]( int, int ) {
        ++executions;
        return 1;
    } );

    for ( int i : z ) {
        REQUIRE( false );
    }

    REQUIRE( 0 == std::accumulate( z.begin(), z.end(), 0 ) );
    REQUIRE( 0 == executions );
    REQUIRE( z.begin() == z.end() );
}

}

TEST_CASE( "zip::empty" ) {
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

namespace {
template< template< typename > class A, template< typename > class B >
void zippedStructures() {
    A< int > a{ 1,-2,-3,4,5 };
    B< char > b{ '+', '-', '+', '-' };
    const int expected[] = { 1, -2, 3, -4 };

    int executions = 0;
    auto z = lazy::zip( a.begin(), a.end(),
                        b.begin(), b.end(), 
                        [&]( int a, char b ) {
        ++executions;
        if ( b == '-' )
            return -std::abs( a );
        if ( b == '+' )
            return std::abs( a );
        return 0;
    } );

    auto i = z.begin();
    const int *ex = expected;
    for ( ; i != z.end(); ++i, ++ex ) {
        REQUIRE( *i != 0 );
        REQUIRE( *i == *ex );
    }
    REQUIRE( executions == std::min( a.size(), b.size() ) );
}
}

TEST_CASE( "zip::zipped" ) {
    SECTION( "vector+list" ) {
        ::zippedStructures< ::tests::Vector, ::tests::List >();
        ::zippedStructures< ::tests::List, ::tests::Vector >();
    }
    SECTION( "set" ) {
        ::tests::Set< int > numbers{ 1,2,3,4,5 };
        ::tests::Vector< int > s{1,1,1,1,1};
        // auto s = sequence::infinite( []( sequence::Parameter< int > ) {
            // return 1;
        // }, 1 );

        auto z = lazy::zip( numbers.begin(), numbers.end(),
                            s.begin(), s.end(),
                            []( int n, int l ) {
            return n + l;
        } );
        std::map< int, int > occurences;
        for ( int i : z ) {
            occurences[ i ]++;
        }
        for ( int i = 2; i <= 6; ++i ) {
            REQUIRE( occurences[ i ] == 1 );
        }
        REQUIRE( occurences.size() == 5 );
    }
}

namespace {

template< template< typename > class Struct >
void arrow() {
    Struct< std::string > texts{
        "a", "b", "hello", "world", "fuchsie"
    };
    Struct< size_t > length{ 2, 2, 3, 4, 5 };
    const size_t expected[] = { 1,1,3,4,5 };
    
    size_t executions = 0;
    auto f = lazy::zip( texts.begin(), texts.end(),
                        length.begin(), length.end(),
                        [&]( const std::string &s, size_t l ) -> std::string {
        ++executions;
        return l >= s.size() ? s : s.substr( 0, l );
    } );

    const size_t *l = expected;
    for ( auto i = f.begin(); i != f.end(); ++i, ++l ) {
        REQUIRE( i->size() == *l );
        REQUIRE( ( *i ).size() == *l );
    }
    REQUIRE( executions == texts.size() );
}

}

TEST_CASE( "zip::arrow" ) {
    SECTION( "vector" ) {
        ::arrow< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::arrow< ::tests::List >();
    }
}


namespace {
template< template< typename > class Struct >
void operators() {
    Struct< std::string > data{ "hel", "dol" };
    Struct< std::string > suff{ "lo", "ly" };
    auto z = lazy::zip( data.begin(), data.end(),
                        suff.begin(), suff.end(),
                        []( const std::string &p, const std::string &s ) {
        return p + s;
    } );
    ::tests::iteratorManipulation( z.begin() );
    ::tests::iteratorContent( z.begin(), *data.begin() + *suff.begin() );
    ::tests::iteratorContent( ++z.begin(), *++data.begin() + *++suff.begin() );
}
}

TEST_CASE( "zip::operators" ) {
    SECTION( "vector" ) {
        ::operators< ::tests::Vector >();
    }
    SECTION( "list" ) {
        ::operators< ::tests::List >();
    }
}

