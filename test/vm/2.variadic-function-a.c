#include <stdarg.h>
#include <assert.h>

int sum( int count, ... ) {
    int acc = 0;
    va_list args;
    va_start( args, count );
    for ( int i = 0; i != count; i++ ) {
        int x = va_arg( args, int );
        acc += x;
    }
    return acc;
}

int main() {
    assert( sum( 0 ) == 0 );
    assert( sum( 1, 1 ) == 1 );
    assert( sum( 2, 1, 2 ) == 3 );
    assert( sum( 3, 1, 2, 3 ) == 6 );
}