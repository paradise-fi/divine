/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <numeric>

#include "common.h"
#include "lazy.h"

namespace {

struct Item {
    Item() {
        FAIL( "nonparametric constructor must not be called" );
    }
    Item( int value ) :
        _value( value )
    {}

    int get() const {
        return _value;
    }

private:
    int _value;
};

bool operator==( const Item &lhs, const Item &rhs ) {
    return lhs.get() == rhs.get();
}
bool operator!=( const Item &lhs, const Item &rhs ) {
    return !( lhs == rhs );
}

template< template< typename > class Struct >
void emptyStructure() {
    Struct< Item > data;
    int executions = 0;

    auto m = lazy::map( data.begin(), data.end(), [&]( Item ) {
        ++executions;
        return 1;
    } );

    for ( int i : m ) {
        REQUIRE( false );
    }

    REQUIRE( 0 == std::accumulate( m.begin(), m.end(), 0 ) );
    REQUIRE( 0 == executions );
    REQUIRE( m.begin() == m.end() );
}
}

TEST_CASE( "map::empty", "[map::empty]" ) {
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

template< template< typename > class Struct >
void arrow() {
    Struct< std::string > texts{
        "a", "b", "hello", "world", "fuchsie"
    };

    size_t executions = 0;
    auto m = lazy::map( texts.begin(), texts.end(), [&]( const std::string &s ) {
        ++executions;
        return "[" + s + "]";
    } );

    auto oi = texts.begin();
    for ( auto i = m.begin(); i != m.end(); ++i, ++oi ) {
        REQUIRE( oi->size() + 2 == i->size() );
        REQUIRE( "[" + *oi + "]" == *i );
    }
    REQUIRE( executions == texts.size() );
}

}

TEST_CASE( "map::arrow", "[map::arrow]" ) {
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
    auto m = lazy::map( data.begin(), data.end(), []( const std::string &i ) {
        return "@" + i;
    } );
    ::tests::iteratorManipulation( m.begin() );
    ::tests::iteratorContent( m.begin(), "@" + *data.begin() );
    ::tests::iteratorContent( ++m.begin(), "@" + *++data.begin() );
}
}

TEST_CASE( "map::operators", "[map::operators]" ) {
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
void changeType( Struct< int > data ) {

    tests::Vector< double > desired( data.size() );

    std::transform( data.begin(), data.end(), desired.begin(), []( int i ) {
        return i + 0.5;
    } );

    int executions = 0;
    auto m = lazy::map( data.begin(), data.end(), [&]( int i ) -> double {
        ++executions;
        return i + 0.5;
    } );

    auto d = desired.begin();
    size_t entrace = 0;
    volatile double value = 0;
    for ( auto i = m.begin(); i != m.end(); ++i, ++d ) {
        REQUIRE( entrace++ < data.size() );
        value = *i;
        REQUIRE( *d == *i );
    }
    REQUIRE( executions == data.size() );
}
}

TEST_CASE( "map::change_type", "[map::change_type]" ) {
    SECTION( "vector" ) {
        ::changeType< ::tests::Vector >( { 1,2,3,4 } );
    }
    SECTION( "list" ) {
        ::changeType< ::tests::List >( { 1,2,3,4 } );
    }
    SECTION( "set" ) {
        ::changeType< ::tests::Set >( { 1,2,3,4 } );
    }
}

