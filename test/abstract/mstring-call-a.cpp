/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

char * concat_bbb( char * str ) {
    char buffb[] = "bbb";
    char * bbb = __mstring_from_string( buffb );
    strcat( str, bbb );
    return str;
}

int main() {
    char buffe[] = "aaabbb";
    char * expected = __mstring_from_string( buffe );

    char buffa[7] = "aaa";
    char * aaa = __mstring_from_string( buffa );

    concat_bbb( aaa );
    assert( strcmp( aaa, expected ) == 0 );
}
