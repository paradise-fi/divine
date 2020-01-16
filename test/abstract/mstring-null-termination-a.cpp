/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    char buffa[16] = {};
    char * a = __mstring_from_string( buffa );
    char buffb[16] = {};
    char * b = __mstring_from_string( buffb );
    char buffc[31] = {};
    char * c = __mstring_from_string( buffc );

    char buff1[16] = "0123456789abcde";
    char * src1 = __mstring_from_string( buff1 );
    strcpy( a, src1 );

    char buff2[16] = "0123456789abcde";
    char * src2 = __mstring_from_string( buff2 );
    strcpy( b, src2 );

    strcpy( c, a );
    strcat( c, b );

    char expected[31] = "0123456789abcde0123456789abcde";
    char * exp = __mstring_from_string( expected );

    assert( strcmp( c, exp ) == 0 );

    return 0;
}
