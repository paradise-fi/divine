/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[5] = "aabb";
    const char * a = __mstring_val( stra, 5 );
    char strb[7] = "ccc\0bb";
    const char * b = __mstring_val( strb, 7 );
    assert( strcmp( a, b ) == 0 ); /* ERROR */
}
