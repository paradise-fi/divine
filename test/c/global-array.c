#include <assert.h>

int array[2];

int main()
 {
    array[1] = 3;
    assert( array[1] == 3 );
    return 0;
}
