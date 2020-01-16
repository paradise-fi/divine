/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char buff1[5] = "aabb";
    char * src = __mstring_from_string( buff1 );

    char buff2[5] = {};
    char * dst = __mstring_from_string( buff2 );

    memcpy( dst, src, 5 );

    char buff3[5] = "aabb";
    char * expected = __mstring_from_string( buff3 );

    int  i = 0;
    while ( expected[ i ] != '\0' ) {
        assert( dst[ i ] == expected[ i ] );
        ++i;
    }
    assert( dst[ i ] == '\0' );
}
