/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    char buff1[5] = "aabb";
    char * src = __lamp_lift_str( buff1 );

    char buff2[5] = {};
    char * dst = __lamp_lift_arr( buff2, sizeof( buff2 ) );

    memcpy( dst, src, 5 );

    char buff3[5] = "aabb";
    char * expected = __lamp_lift_str( buff3 );

    int  i = 0;
    while ( expected[ i ] != '\0' )
    {
        assert( dst[ i ] == expected[ i ] );
        ++i;
    }
    assert( dst[ i ] == '\0' );
}
