/* TAGS: c++ */
#include <cassert>

void bar() {
    throw 5;
}

void foo( int n ) {
    bar();
    int vla[n];
}

int main()
{
    try { foo( 10 ); }
    catch( int x ) {
        assert( x == 5 );
        return 0;
    }
    assert( 0 );
    return 0;
}
