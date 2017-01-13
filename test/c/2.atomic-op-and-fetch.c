#include <assert.h>
int main()
{
    int val = 1;
    assert( __sync_add_and_fetch( &val, 1 ) == 2 );
    assert( val == 2 );
    assert( __sync_sub_and_fetch( &val, 1 ) == 1 );
    assert( val == 1 );
    assert( __sync_or_and_fetch( &val, 2 ) == 3 );
    assert( val == 3 );
    assert( __sync_and_and_fetch( &val, 2 ) == 2 );
    assert( val == 2 );
    return 0;
}
