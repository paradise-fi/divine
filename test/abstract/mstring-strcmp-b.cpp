/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[5] = "aabb";
    const char * a = __lamp_lift_str( stra );
    char strb[7] = "ccc\0bb";
    const char * b = __lamp_lift_str( strb );
    auto cmp = strcmp( a, b );
    assert( cmp == 0 ); /* ERROR */
}
