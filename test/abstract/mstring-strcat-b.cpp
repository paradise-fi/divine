/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

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
    char * expected = __mstring_val( buffe, 7 );

    char buffa[7] = "aaa";
    char * a = __mstring_val( buffa, 7 );
    char buffb[4] = "bbb";
    char * b = __mstring_val( buffb, 4 );

    my_strcat( a, b );

    assert( strcmp( a, expected ) == 0 );
}
