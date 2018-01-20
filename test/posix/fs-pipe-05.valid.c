/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char buf[ 1100 ] = {};
    int pfd[ 2 ];
    assert( pipe( pfd ) == 0 );

    assert( write( pfd[ 1 ], buf, 1100 ) == 1024 );
    write( pfd[ 1 ], buf, 1 );
    assert( 0 );
    return 0;
}
