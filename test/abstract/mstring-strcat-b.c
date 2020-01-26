/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void my_strcat( char * dest, const char * src )
{
    while (*dest) dest++;
    while ((*dest++ = *src++));
}

int main() {
    char buffe[7] = "aaabbb";
    char * expected = __lamp_lift_arr( buffe, 7 );

    char buffa[7] = "aaa";
    char * a = __lamp_lift_arr( buffa, 7 );
    char buffb[4] = "bbb";
    char * b = __lamp_lift_arr( buffb, 4 );

    my_strcat( a, b );

    assert( strcmp( a, expected ) == 0 );
}
