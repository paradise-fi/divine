/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym int array[ 4 ];
    if ( array[ 0 ] < array[ 3 ] ) {
        assert( array[ 3 ] - array[ 0 ] > 0 );
    } else {
        assert( array[ 0 ] - array[ 3 ] > 0 );
    }
}
