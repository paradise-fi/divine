/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char str[8] = "aabbbcc";
    char * a = __mstring_val( str, 8 );
    a[ 1 ] = 'b';

    assert( a[1] == 'b' );
}
