/* TAGS: min c */
#include <assert.h>
int main()
{
    int val = 1;
    assert( __sync_fetch_and_add( &val, 1 ) == 1 );
    assert( val == 2 );
    assert( __sync_fetch_and_sub( &val, 1 ) == 2 );
    assert( val == 1 );
    assert( __sync_fetch_and_or( &val, 2 ) == 1 );
    assert( val == 3 );
    assert( __sync_fetch_and_and( &val, 2 ) == 3 );
    assert( val == 2 );
    return 0;
}
