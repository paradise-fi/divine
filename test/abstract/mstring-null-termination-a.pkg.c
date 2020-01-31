/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char buffa[4] = {};
    char * a = __lamp_lift_arr( buffa, sizeof( buffa ) );
    char buffb[4] = {};
    char * b = __lamp_lift_arr( buffb, sizeof( buffb ) );
    char buffc[7] = {};
    char * c = __lamp_lift_arr( buffc, sizeof( buffc ) );

    char buff1[4] = "012";
    char * src1 = __lamp_lift_arr( buff1, sizeof( buff1 ) );
    strcpy( a, src1 );

    char buff2[4] = "012";
    char * src2 = __lamp_lift_arr( buff2, sizeof( buff2 ) );
    strcpy( b, src2 );

    strcpy( c, a );
    strcat( c, b );

    char expected[7] = "012012";
    char * exp = __lamp_lift_arr( expected, sizeof( expected ) );

    assert( strcmp( c, exp ) == 0 );

    return 0;
}
