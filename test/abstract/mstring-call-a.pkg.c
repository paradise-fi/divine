/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

char *concat_bbb( char *str )
{
    char buffb[] = "bbb";
    char *bbb = __lamp_lift_str( buffb );
    strcat( str, bbb );
    return str;
}

int main()
{
    char buffe[] = "aaabbb";
    char * expected = __lamp_lift_str( buffe );

    char buffa[7] = "aaa";
    char * aaa = __lamp_lift_arr( buffa, sizeof( buffa ) );

    concat_bbb( aaa );
    assert( strcmp( aaa, expected ) == 0 );
}
