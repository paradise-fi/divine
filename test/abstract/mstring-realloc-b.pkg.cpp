/* TAGS: mstring todo */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    char buff1[5] = "aabb";
    char * str = __lamp_lift_str( buff1 );
    char * extended = (char *)realloc( str, 3 * sizeof( char ) );

    char buff2[4] = "aab";
    char * expected = __lamp_lift_str( buff2 );

    int  i = 0;
    while ( expected[ i ] != '\0' )
    {
        assert( extended[ i ] == expected[ i ] );
        ++i;
    }
}
