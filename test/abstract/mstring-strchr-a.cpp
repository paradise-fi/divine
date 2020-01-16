/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[5] = "aabb";
    char *str = __mstring_from_string( buff );
    char target = 'b';

    char buffe[3] = "bb";
    const char * expected = __mstring_from_string( buffe );
    const char * res = strchr( str, target );
    assert( strcmp( res, expected ) == 0 );
}
