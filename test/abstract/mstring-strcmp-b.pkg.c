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
    char stra[5] = "aabb";
    const char * a = __lamp_lift_arr( stra, sizeof( stra ) );
    char strb[7] = "ccc\0bb";
    const char * b = __lamp_lift_arr( strb, sizeof( strb ) );
    int cmp = strcmp( a, b );
    assert( cmp == 0 ); /* ERROR */
}
