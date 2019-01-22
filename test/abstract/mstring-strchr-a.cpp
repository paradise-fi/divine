/* TAGS: mstring min todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[5] = "aabb";
    char *str = __mstring_val( buff, 5 );
    char target = 'b';

    char buffe[3] = "bb";
    const char * expected = __mstring_val( buffe, 3 );
    const char * res = strchr( str, target );
    assert( strcmp( res, expected ) == 0 );
}
