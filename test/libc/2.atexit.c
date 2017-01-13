#include <stdlib.h>
#include <assert.h>

void diediedie() { assert( 0 ); } /* ERROR */

int main()
{
    atexit( diediedie );
    exit( 0 );
    return 0;
}
