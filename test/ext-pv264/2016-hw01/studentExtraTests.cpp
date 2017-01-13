/* VERIFY_OPTS: -o nofail:malloc */
/* PROGRAM_OPTS: --use-colour no */
#include <vector>
#include <initializer_list>
#include <algorithm>
#include <iostream>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "lazy.h"

namespace {

template< typename T, typename I >
void check( I first, I last, std::initializer_list< T > expected ) {
    bool result = std::equal( first, last, expected.begin(), expected.end() );
    REQUIRE( result );
}

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

struct SequenceGenerator {

    struct iterator : std::iterator< std::forward_iterator_tag, int, std::ptrdiff_t, const int *, const int & > {
        iterator() :
            _value( 1 ),
            _sequence( 1 ),
            _limit( 0 )
        {}
        iterator( int limit ) :
            _value( 0 ),
            _sequence( 0 ),
            _limit( limit )
        {}

        const int &operator*() const {
            return _value;
        }
        iterator &operator++() {
            ++_limit;
            ++_value;
            if ( _value > _sequence ) {
                ++_sequence;
                _value = 1;
            }
            return *this;
        }
        iterator operator++( int ) {
            iterator self( *this );
            ++( *this );
            return self;
        }
        bool operator==( const iterator &other ) const {
            return _limit == other._limit;
        }
        bool operator!=( const iterator &other ) const {
            return _limit != other._limit;
        }

    private:
        int _value;
        int _sequence;
        int _limit;
    };

    SequenceGenerator( int stop ) :
        _stop( stop )
    {}

    iterator begin() const {
        return iterator();
    }
    iterator end() const {
        return iterator( _stop );
    }

private:
    int _stop;
};

}

TEST_CASE( "students tests of extra required behaviour", "[student extra]" ) {

    SECTION( "nonparametric map" ) {
        std::vector< Item > data{ 1,2,3,4 };
        auto m = lazy::map( data.begin(), data.end(), []( const Item &i ) {
            return Item( i.get() * 2 );
        } );
        check< Item >( m.begin(), m.end(), { 2,4,6,8 } );
    }
    SECTION( "nonparametric zip" ) {
        std::vector< Item > data{ 1,2,3,4 };
        auto z = lazy::zip( data.begin(), data.end(),
                            data.begin(), data.end(),
                            []( const Item &a, const Item &b ) {
            return Item( a.get() * b.get() );
        } );
        check< Item >( z.begin(), z.end(), { 1,4,9,16 } );
    }
    SECTION( "this test has to pass in reasonable amout of time (15 seconds at maximum)" ) {
        SequenceGenerator sg( 42 );
        auto u = lazy::unique( sg.begin(), sg.end() );
        REQUIRE( 8 == *std::max_element( u.begin(), u.end() ) );
    }
}
