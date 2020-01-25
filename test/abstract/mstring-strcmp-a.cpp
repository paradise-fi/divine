/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char stra[8] = "abc\0abc";
    const char * a = __lamp_lift_arr( stra, sizeof( stra ) );
    char strb[8] = "abc\0cba";
    const char * b = __lamp_lift_arr( strb, sizeof( strb ) );
    auto cmp = strcmp( a, b );
    assert( cmp == 0 );
}
