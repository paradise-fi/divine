/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void check( char * str, const char * expected ) {
}

int main() {
    char buff1[9] = "aa\0bbbcc";
    char * str = __mstring_val( buff1, 9 );

    char * shifted = str + 3;
    shifted[ 1 ] = 'c';

    char buff2[6] = "bcbcc";
    char * expected = __mstring_val( buff2, 6 );

    int i = 0;
    while ( expected[ i ] != '\0' ) {
        assert( shifted[ i ] == expected[ i ] );
        ++i;
    }
}
