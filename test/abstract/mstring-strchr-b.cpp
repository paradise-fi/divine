/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

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
