/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "aabbbcc";
    char * a = __mstring_from_string( str );
    a[ 1 ] = 'b';

    assert( a[1] == 'b' );
}
