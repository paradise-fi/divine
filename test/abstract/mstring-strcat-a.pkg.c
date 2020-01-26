/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buffe[7] = "aaabbb";
    char * expected = __lamp_lift_str( buffe );

    char buffa[7] = "aaa";
    char * a = __lamp_lift_arr( buffa, sizeof( buffa ) );
    char buffb[7] = "bbb";
    char * b = __lamp_lift_arr( buffb, sizeof( buffb ) );

    strcat(a, b);

    assert( strcmp( a, expected ) == 0 );
}
