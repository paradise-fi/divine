/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int main() {
    int array[ 4 ] = { 0 };
    _SYM int x;
    array[ 2 ] = x;
    assert( array[ 2 ] == x );
}
