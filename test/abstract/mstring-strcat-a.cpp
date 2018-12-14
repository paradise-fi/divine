/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char * expected = __mstring_val( "aaabbb", 7 );

    char arr[7] = "aaa\0";
    char * a = __mstring_val( arr, 7 );
    char * b = __mstring_val( "bbb\0", 4 );

    strcat(a, b);

    assert( strcmp(a, expected) == 0 );
}
