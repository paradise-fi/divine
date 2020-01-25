/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

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
