/* CC_OPTS: -std=c++14 -I$SRC_ROOT/bricks */
#include <dios.h>
#include <dios/core/scheduling.hpp>
#include <cassert>

struct Item {
    int val;

    int getId() { return val; }
    Item( int i ) : val( i ) { }
    Item( const Item& i ) { assert( false ); }
    Item& operator=( const Item& i ) { assert( false ); return *this; }
};

bool contains( __dios::SortedStorage< Item >& s, int v ) {
    for ( int i = 0; i != s.size(); i++ )
        if ( s[i]->val == v ) return true;
    return false;
}

int main( ... ) {
    __dios::SortedStorage< Item > s;
    assert( s.empty() );

    s.emplace( 42 );
    assert( s.size() == 1 );
    assert( !s.empty() );
    assert( s[0]->val == 42 );
    assert( !s.find( 12 ) );
    assert( s.find( 42 )->val == 42 );

    s.emplace( 24 );
    assert( s.size() == 2 );
    assert( !s.empty() );
    assert( contains( s, 42 ) );
    assert( contains( s, 24 ) );
    assert( s.find( 42 )->val == 42 );
    assert( s.find( 24 )->val == 24 );

    if ( __vm_choose( 2 ) ) {
        s.remove( 24 );
        assert( s.size() == 1 );
        assert( !s.empty() );
        assert( s.find( 42 )->val == 42 );
        assert( contains( s, 42 ) );
        assert( !contains( s, 24 ) );

        s.remove( 42 );
        assert( s.size() == 0 );
        assert( s.empty() );
        assert( !s.find( 42 ) );
    }
    else {
        s.erase( s.begin(), s.end() );
        assert( s.size() == 0 );
        assert( s.empty() );
        assert( !s.find( 42 ) );
        assert( !s.find( 24 ) );
    }
}
