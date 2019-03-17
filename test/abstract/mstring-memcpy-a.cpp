/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char buff1[5] = "aabb";
    char * src = __mstring_val( buff1, 5 );

    char buff2[5] = {};
    char * dst = __mstring_val( buff2, 5 );

    memcpy( dst, src, 5 );

    char buff3[5] = "aabb";
    char * expected = __mstring_val( buff3, 5 );

    int  i = 0;
    while ( expected[ i ] != '\0' ) {
        assert( dst[ i ] == expected[ i ] );
        ++i;
    }
    assert( dst[ i ] == '\0' );
}
