/* TAGS: min c */
#include <assert.h>
#include <stdarg.h>
void vararg(int i, ...) {
    assert( i < 5 );
    va_list ap;
    va_start( ap, i );
    for ( int j = 0; j < i; ++j )
        assert( va_arg( ap, int ) == j );
    va_end( ap );
}

int main() {
    vararg( 0 );
    vararg( 1, 0 );
    vararg( 2, 0, 1 );
    vararg( 3, 0, 1, 2 );
    vararg( 4, 0, 1, 2, 3 );
    return 0;
}
