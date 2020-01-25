/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char str[8] = "aabbbcc";
    char * a = __lamp_lift_arr( str, sizeof( str ) );
    a[ 4 ] = '\0';
    assert( strlen( a ) == 4 );
}
