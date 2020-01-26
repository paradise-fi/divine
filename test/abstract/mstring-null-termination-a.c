/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buffa[16] = {};
    char * a = __lamp_lift_arr( buffa, sizeof( buffa ) );
    char buffb[16] = {};
    char * b = __lamp_lift_arr( buffb, sizeof( buffa ) );
    char buffc[31] = {};
    char * c = __lamp_lift_arr( buffc, sizeof( buffa ) );

    char buff1[16] = "0123456789abcde";
    char * src1 = __lamp_lift_arr( buff1, sizeof( buff1 ) );
    strcpy( a, src1 );

    char buff2[16] = "0123456789abcde";
    char * src2 = __lamp_lift_arr( buff2, sizeof( buff2 ) );
    strcpy( b, src2 );

    strcpy( c, a );
    strcat( c, b );

    char expected[31] = "0123456789abcde0123456789abcde";
    char * exp = __lamp_lift_arr( expected, sizeof( expected ) );

    assert( strcmp( c, exp ) == 0 );

    return 0;
}
