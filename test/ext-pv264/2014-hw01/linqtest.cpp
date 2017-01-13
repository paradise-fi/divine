/* VERIFY_OPTS: -o nofail:malloc */
#include "linq.h"
#include <cassert>

struct Unit { };

using namespace query;

template struct query::Vector< int >;

int main() {
    const Vector< int > vi{ 1, 2, 3 };
    const Vector< Unit > vu{ Unit{}, Unit{}, Unit{} };

    assert( vi.map( []( int ) { return 0; } ).size() == 3 );
    assert( vi.average() == 2 );
    assert( vi.sum() == 6 );
    assert( vi.median() == 2 );
    int sum = 0;
    vi.forEach( [&]( int x ) { sum += x; } );
    assert( sum == 6 );
    assert( (Vector< int >{ 3, 1, 2 }.sort() == vi) );
    assert( (Vector< int >{ 3, 2, 1 }.reverse() == vi) );
    assert( vi.reverse().reverse() == vi );
    assert( (Vector< int >{ 1, 1, 2, 1, 2, 3, 1, 2, 3 }.unique()) == vi );
    assert( (Vector< Vector< int > >{ { 1, 2 }, { 3 } }.flatten< int >()) == vi );
    assert( vu.accumulate( 0, []( int l, Unit ) { return l + 1; } ) == 3 );
}
