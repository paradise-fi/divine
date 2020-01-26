/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char str[8] = "aabbbcc";
    char * a = __lamp_lift_arr( str, sizeof( str ) );
    a[ 4 ] = '\0';
    assert( strlen( a ) == 4 );
}
