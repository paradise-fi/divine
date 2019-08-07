/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buffa[16] = {};
    char * a = __mstring_val( buffa, 16 );
    char buffb[16] = {};
    char * b = __mstring_val( buffb, 16 );
    char buffc[31] = {};
    char * c = __mstring_val( buffc, 31 );

    char buff1[16] = "0123456789abcde";
    char * src1 = __mstring_val( buff1, 16 );
    strcpy( a, src1 );

    char buff2[16] = "0123456789abcde";
    char * src2 = __mstring_val( buff2, 16 );
    strcpy( b, src2 );

    strcpy( c, a );
    strcat( c, b );

    char expected[31] = "0123456789abcde0123456789abcde";
    char * exp = __mstring_val( expected, 31 );

    assert( strcmp( c, exp ) == 0 );

    return 0;
}
