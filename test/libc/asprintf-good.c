/* TAGS: c */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int main( void )
{
    char *buf = NULL;
    int r = asprintf( &buf, "test" );
    if ( r != 0 ) // !oom
    {
        assert( r == 4 );
        assert( buf != NULL );
        assert( strncmp( "test", buf, 4 ) == 0 );
        free( buf );
    }

    return 0;
}
