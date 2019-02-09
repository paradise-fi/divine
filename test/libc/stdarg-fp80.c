/* TAGS: todo */

#include <stdarg.h>
#include <assert.h>

int test( int x, ... )
{
    va_list ap;
    va_start( ap, x );
    long double fp80 = va_arg( ap, long double );
    assert( fp80 == 3.14 );
    va_end( ap );
}

int main()
{
    test( 1, 3.14l );
}
