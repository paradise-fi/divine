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
    char buffa[8] = {};
    char * a = __lamp_lift_arr( buffa, sizeof( buffa ) );
    char buffb[8] = {};
    char * b = __lamp_lift_arr( buffb, sizeof( buffb ) );
    char buffc[15] = {};
    char * c = __lamp_lift_arr( buffc, sizeof( buffc ) );

    char buff1[8] = "0123456";
    char * src1 = __lamp_lift_arr( buff1, sizeof( buff1 ) );
    strcpy( a, src1 );

    char buff2[8] = "0123456";
    char * src2 = __lamp_lift_arr( buff2, sizeof( buff2 ) );
    strcpy( b, src2 );

    strcpy( c, a );
    strcat( c, b );

    char expected[15] = "01234560123456";
    char * exp = __lamp_lift_arr( expected, sizeof( expected ) );

    assert( strcmp( c, exp ) == 0 );

    return 0;
}
