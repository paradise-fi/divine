/* TAGS: min c++ */
/* CC_OPTS: -std=c++14 -I$SRC_ROOT/bricks */
#include <dios.h>
#include <util/map.hpp>
#include <cassert>

using namespace __dios;

template < typename T >
void checkEmpty( const T& x ) {
    assert( x.empty() );
    assert( x.size() == 0 );
}

template < typename T >
void checkNotEmpty( const T& x, int size = -1 ) {
    assert( !x.empty() );
    if ( size == -1 )
        assert( x.size() > 0 );
    else
        assert( x.size() == size );
}

int main() {
    ArrayMap< int, int > x;
    checkEmpty( x );
    x.emplace( 1, 42 );
    checkNotEmpty( x, 1 );
    assert( *x.find( 1 ) == std::make_pair( 1, 42 ) );
    x.emplace( 3, 24 );
    checkNotEmpty( x, 2 );
    assert( *x.find( 3 ) == std::make_pair( 3, 24 ) );
    x.emplace( 2, 12 );
    checkNotEmpty( x, 3 );
    assert( *x.find( 2 ) == std::make_pair( 2, 12 ) );
    x.erase( 2 );
    checkNotEmpty( x, 2 );
    assert( x.find( 2 ) == x.end() );
    assert( *x.find( 1 ) == std::make_pair( 1, 42 ) );
    assert( *x.find( 3 ) == std::make_pair( 3, 24 ) );

    AutoIncMap< int, int > y;
    y.push( 42 );
    y.push( 43 );
    assert( y.find( 42 )->second == 0 );
    assert( y.find( 43 )->second == 1 );
    y.erase( 43 );
    y.push( 44 );
    assert( y.find( 44 )->second == 2 );
}
