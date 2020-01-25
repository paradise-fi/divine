/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char buff1[9] = "aa\0bbbcc";
    char * str = __lamp_lift_arr( buff1, sizeof( buff1 ) );

    char * shifted = str + 3;
    shifted[ 1 ] = 'c';

    char buff2[6] = "bcbcc";
    char * expected = __lamp_lift_str( buff2 );

    int i = 0;
    while ( expected[ i ] != '\0' )
    {
        assert( shifted[ i ] == expected[ i ] );
        ++i;
    }
}
