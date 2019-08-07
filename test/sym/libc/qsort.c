/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <stdlib.h>
#include <assert.h>

int le( const void *a, const void *b )
{
    return ( *(int*)a - *(int*)b );
}

int main()
{
    int arr[6];
    for( unsigned i = 0; i < 6; ++i )
        arr[i] = __sym_val_i32();

    qsort( arr, 6, sizeof( int ), le );

    for( unsigned i = 0; i < 5; ++i )
        assert( arr[i] <= arr[i+1] );
}
