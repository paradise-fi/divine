/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char str[8] = "aabb\0cc";
    char * a = __mstring_val( str, 8 );
    a[ 4 ] = 'b';
    assert( strlen( a ) == 7 );
}
