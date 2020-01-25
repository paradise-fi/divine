/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char buff[7] = "string";
    const char * str = __lamp_lift_str( buff );
    int len = strlen( str );
    assert( len == 6 );
}
