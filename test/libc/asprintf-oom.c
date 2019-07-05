/* TAGS: c */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main( void )
{
    char *buf = NULL;
    int r = asprintf( &buf, "test" );
    assert( r == 4 ); /* ERROR */
    if ( r ) free( buf );
    return 0;
}
