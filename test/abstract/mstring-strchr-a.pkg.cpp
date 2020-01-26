/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[5] = "aabb";
    char *str = __lamp_lift_str( buff );
    char target = 'b';

    char buffe[3] = "bb";
    const char * expected = __lamp_lift_str( buffe );
    const char * res = strchr( str, target );
    assert( strcmp( res, expected ) == 0 );
}
