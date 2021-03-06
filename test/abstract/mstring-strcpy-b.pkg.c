/* TAGS: mstring */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void my_strcpy( char * dest, const char * src )
{
    int i = 0;

    while ( src[i] != '\0' )
    {
        dest[i] = src[i];
        ++i;
    }

    dest[i] = '\0';
}

int main()
{
    char buff1[7] = "string";
    const char * src = __lamp_lift_str( buff1 );
    char buff2[7] = "";
    char *dst = __lamp_lift_arr( buff2, sizeof( buff2 ) );

    my_strcpy( dst, src );
    assert( strcmp( src,dst ) == 0 );
}
