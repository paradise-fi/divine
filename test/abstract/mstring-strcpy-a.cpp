/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[7] = "aaaaaa";
    char * dest = __lamp_lift_str( stra );
    char strb[4] = "bbb";
    char * src = __lamp_lift_str( strb );

    strcpy( dest, src );
    assert( strlen( dest ) == 3 );

    dest[3] = 'a';
    assert( strlen( dest ) == 6 );
}
