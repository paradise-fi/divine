#include <assert.h>

int main() {
    char array[20];
    int x;

    int **ptr = &array[4];
    *ptr = &x;
    **ptr = 42;
    assert( x == 42 );

    int y;
    int **ptr2 = &array[12];
    *ptr2 = &y;
    **ptr2 = 2;
    assert( y == 2 );
    assert( x == 42 );

    assert( *ptr == &x );
    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );
    assert( **ptr == 42 );

    *ptr = 0;

    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );

    *ptr = &x;

    assert( *ptr == &x );
    assert( *ptr2 == &y );
    assert( **ptr2 == 2 );
    assert( **ptr == 42 );

    *ptr2 = 0;

    assert( *ptr == &x );
    assert( **ptr == 42 );
}
