/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[7] = "aaaaaa";
    char * dest = __mstring_from_string( stra );
    char strb[4] = "bbb";
    char * src = __mstring_from_string( strb );

    strcpy( dest, src );
    assert( strlen( dest ) == 3 );

    dest[3] = 'a';
    assert( strlen( dest ) == 6 );
}
