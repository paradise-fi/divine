#include <stdio.h>

int factorial( int n ) {
    if ( n == 0 )
        return 1;
    return n * factorial( n - 1 );
}

int main() {
    int i = 0;
    while ( 1 ) {
        i = i + 3;
        i = factorial( i ) + 1;
        i = i % 16;
        trace("hello there %d", i);
        assert(i != 1);
    }
    return 0;
}
