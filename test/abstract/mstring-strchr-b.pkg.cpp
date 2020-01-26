/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[7] = "abbabb";
    char *str = __lamp_lift_str( buff );
    char target = 'a';

    char buffe1[7] = "abbabb";
    const char * expected1 = __lamp_lift_str( buffe1 );
    str = strchr( str, target );
    assert( strcmp( str, expected1 ) == 0 );

    str++;

    char buffe2[4] = "abb";
    const char * expected2 = __lamp_lift_str( buffe2 );
    str = strchr( str, target );
    assert( strcmp( str, expected2 ) == 0 );
}
