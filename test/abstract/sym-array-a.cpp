/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int main() {
    _SYM int array[ 4 ];
    if ( array[ 0 ] < array[ 3 ] ) {
        assert( array[ 3 ] - array[ 0 ] > 0 );
    } else {
        assert( array[ 0 ] - array[ 3 ] > 0 );
    }
}
