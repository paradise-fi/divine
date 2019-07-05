/* TAGS: c */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main( void )
{
    char *buf = NULL;
    int r = asprintf( &buf, "test %d %03ld", 0, 42l );
    if ( r != 0 ) // !oom
    {
        assert( buf != NULL );
        assert( strcmp( "test 0 042", buf ) == 0 );
        free( buf );
    }

    return 0;
}
