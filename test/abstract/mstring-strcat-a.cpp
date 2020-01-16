/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buffe[7] = "aaabbb";
    char * expected = __mstring_from_string( buffe );

    char buffa[7] = "aaa";
    char * a = __mstring_from_string( buffa );
    char buffb[7] = "bbb";
    char * b = __mstring_from_string( buffb );

    strcat(a, b);

    assert( strcmp( a, expected ) == 0 );
}
