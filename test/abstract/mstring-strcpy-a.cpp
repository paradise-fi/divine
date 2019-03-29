/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[7] = "aaaaaa";
    char * dest = __mstring_val( stra, 7 );
    char strb[4] = "bbb";
    char * src = __mstring_val( strb, 4 );

    strcpy( dest, src );
    assert( strlen( dest ) == 3 );

    dest[3] = 'a';
    assert( strlen( dest ) == 6 );
}
