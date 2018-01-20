/* TAGS: min c */
#include <assert.h>

void fail( int *i ) { *i = 0; } /* ERROR */
int main()
{
    fail( 0 );
    return 0;
}
