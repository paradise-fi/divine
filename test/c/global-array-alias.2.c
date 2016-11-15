#include <assert.h>

int a[] = {105,106,107,108,109,110};
int (*b)[6] = &a;

int main()
{
    int (*c)[6] = &a;
    assert( (*b)[0] == 105 );
    assert( (*c)[0] == 105 );
    assert( (*b)[1] == 106 );
    assert( (*c)[1] == 106 );

    (*b)[0] = 3;
    (*c)[1] = 4;
    a[2] = 1;

    assert( a[0] == 3 );
    assert( (*b)[0] == 3 );
    assert( (*c)[0] == 3 );
    assert( a[1] == 4 );
    assert( (*b)[1] == 4 );
    assert( (*c)[1] == 4 );
    assert( a[2] == 1 );
    assert( (*b)[2] == 1 );
    assert( (*c)[2] == 1 );
    return 0;
}
