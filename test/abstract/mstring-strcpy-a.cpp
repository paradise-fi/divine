/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char * dest = __mstring_val( "aaaaaa\0", 7 );
    char * src = __mstring_val( "bbb\0", 4 );

    strcpy( dest, src );
    assert( strlen( dest ) == 3 );

    dest[3] = 'a';
    assert( strlen( dest ) == 6 );
}
