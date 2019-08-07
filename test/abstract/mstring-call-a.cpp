/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

char * concat_bbb( char * str ) {
    char buffb[7] = "bbb";
    char * bbb = __mstring_val( buffb, 4 );
    strcat( str, bbb );
    return str;
}

int main() {
    char buffe[7] = "aaabbb";
    char * expected = __mstring_val( buffe, 7 );

    char buffa[7] = "aaa";
    char * aaa = __mstring_val( buffa, 7 );

    concat_bbb( aaa );
    assert( strcmp( aaa, expected ) == 0 );
}
