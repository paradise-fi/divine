/* TAGS: min c */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main()
{
    int a = 1234567;
    int *pa = &a;
    char mem[ 128 ];
    for ( int i = 0; i < 8; ++i )
        memcpy( mem + 9 * i, &pa, 8 );

    for ( int i = 0; i < 8; ++ i )
    {
        int *p = *( ( int ** ) (mem + 9 * i) );
        assert( p == pa );
        assert( *p == a );
    }
}

