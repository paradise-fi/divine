/* TAGS: min c */
#include <assert.h>
int main()
{
    int val = 0, expect = 0, new = 1;
    assert( __sync_val_compare_and_swap( &val, expect, new ) == 0 );
    assert( val == 1 );
    assert( expect == 0 );
    new = 2;
    assert( __sync_val_compare_and_swap( &val, expect, new ) == 1 );
    assert( val == 1 );
    return 0;
}
