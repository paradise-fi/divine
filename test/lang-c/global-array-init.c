/* TAGS: min c */
#include <assert.h>

int array[2] = { 3, 5 };

int main()
{
    assert( array[0] == 3 );
    array[0] = 2;
    assert( array[1] == 5 );
    assert( array[0] == 2 );
    return 0;
}
