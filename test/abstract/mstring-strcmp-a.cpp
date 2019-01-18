/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[8] = "abc\0abc";
    const char * a = __mstring_val( stra, 8 );
    char strb[8] = "abc\0cba";
    const char * b = __mstring_val( strb, 8 );
    assert( strcmp( a, b ) == 0 );
}
