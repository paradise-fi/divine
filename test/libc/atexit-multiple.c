#include <stdlib.h>
#include <assert.h>

int count = 0;

void inc() { count++; }
void check() { assert( count == 37 ); }

int main()
{
    atexit( check );
    for ( int i = 0; i < 37; ++i )
        atexit( inc );
    exit( 0 );
}
