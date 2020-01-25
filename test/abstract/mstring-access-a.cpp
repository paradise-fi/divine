/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char str[] = "aabbbcc";
    char * a = __lamp_lift_str( str );
    a[ 1 ] = 'b';

    assert( a[1] == 'b' );
}
