/* TAGS: mstring min sym todo */
/* VERIFY_OPTS: --symbolic --lamp symstring -o nofail:malloc */

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void my_strcpy( char * dest, const char * src )
{
    while ( ( *dest++ = *src++ ) );
}

int main()
{
    char buff[7] = "string";
    const char * src = __lamp_lift_arr( buff, 7 );
    char res[7] = "";
    char * dest = __lamp_lift_arr( res, 7 );

    my_strcpy( dest, src );
    assert( strcmp( src, dest ) == 0 );
}
