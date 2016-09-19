#include <string.h>

int array[2];

int main()
{
    memset( array, 1, 2 * sizeof( int ) );
    return 0;
}
