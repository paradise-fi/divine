/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[8] = "abc\0abc";
    const char * a = __mstring_from_string( stra );
    char strb[8] = "abc\0cba";
    const char * b = __mstring_from_string( strb );
    auto cmp = strcmp( a, b );
    assert( cmp == 0 );
}
