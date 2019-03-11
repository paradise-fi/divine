/* TAGS: mstring min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void my_strcpy( char * dest, const char * src ) {
    while((*dest++ = *src++));
}

int main() {
    char buff[7] = "string";
    const char * src = __mstring_val( buff, 7 );
    char res[7] = "";
    char * dest = __mstring_val( res, 7 );

    my_strcpy( dest, src );

    assert( strcmp( src, dest ) == 0 );
}
