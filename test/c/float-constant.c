#include <assert.h>
const volatile float x = 0;

int main()
{
    assert( x == 0 );
    return 0;
}
