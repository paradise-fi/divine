/* TAGS: c */
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int call_vasprintf( char **out, const char *fmt, ... )
{
    int rc;
    va_list ap;
    va_start( ap, fmt );
    return vasprintf( out, fmt, ap );
    va_end( ap );
    return rc;
}

int main( void )
{
    char *buf = NULL;
    int r = call_vasprintf( &buf, "test" );
    assert( r == 4 ); /* ERROR */
    if ( r ) free( buf );
    return 0;
}
