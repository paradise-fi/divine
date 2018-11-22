/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char * a = __mstring_val( "aabb\0", 5 );
    const char * b = __mstring_val( "ccc\0bb", 6 );
    assert( strcmp( a, b ) == 0 ); /* ERROR */
}
