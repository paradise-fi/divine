/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buff[7] = "abbabb";
    char *str = __mstring_val( buff, 7 );
    char target = 'a';

    char buffe1[7] = "abbabb";
    const char * expected1 = __mstring_val( buffe1, 7 );
    str = strchr( str, target );
    assert( strcmp( str, expected1 ) == 0 );

    str++;

    char buffe2[4] = "abb";
    const char * expected2 = __mstring_val( buffe2, 4 );
    str = strchr( str, target );
    assert( strcmp( str, expected2 ) == 0 );
}
