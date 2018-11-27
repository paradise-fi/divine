/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char * str = __mstring_val( "string", 7 );
    int len = strlen( str );
    assert( len == 6 );
}
