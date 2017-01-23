/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    int array[ 4 ] = { 0 };
    __sym int x;
    array[ 2 ] = x;
    assert( array[ 2 ] == x );
}
