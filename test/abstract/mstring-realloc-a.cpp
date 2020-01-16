/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char buff1[5] = "aabb";
    char * str = __mstring_from_string( buff1 );
    char * extended = (char *)realloc( str, 10 * sizeof( char ) );

    char buff2[5] = "aabb";
    char * expected = __mstring_from_string( buff2 );

    int  i = 0;
    while ( expected[ i ] != '\0' ) {
        assert( extended[ i ] == expected[ i ] );
        ++i;
    }

    assert( extended[ i ] == '\0' );
}
