#include <assert.h>
const volatile float x = 1.3;

int main()
{
    assert( x < 1.31 );
    assert( x > 1.29 );
    assert ( x - 1 < 0.31 );
    assert ( x - 1 > 0.29 );
    assert( 2 * x < 2.61 );
    assert( 2 * x > 2.59 );
    return 0;
}
