/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char * a = __mstring_val( "abc\0abc", 7 );
    const char * b = __mstring_val( "abc\0cba", 7 );
    assert( strcmp( a, b ) == 0 );
}
