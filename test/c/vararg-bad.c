#include <assert.h>
#include <stdarg.h>

void vararg(int i, ...)
{
    assert( i < 5 );
    va_list ap;
    va_start( ap, i );
    for ( int j = 0; j < i; ++j )
        assert( va_arg( ap, int ) == j ); /* ERROR */
}

int main()
{
    vararg( 0 );
    vararg( 1, 0 );
    vararg( 2, 0, 2 );
    return 0;
}
