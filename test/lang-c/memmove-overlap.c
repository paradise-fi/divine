/* TAGS: min c */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main()
{
    char *mem = malloc(4);
    if ( mem )
    {
        mem[0] = 4;
        memmove( mem + 1, mem, 2 );
        assert( mem[0] == 4 );
        assert( mem[1] == 4 );
        free( mem );
    }
    return 0;
}
