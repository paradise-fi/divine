/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <string.h>

int main() {
    char buf[ 8 ] = {};
    int pfd[ 2 ];
    assert( pipe( pfd ) == 0 );
    read( pfd[ 0 ], buf, 7 );
    assert( 0 );
    return 0;
}
