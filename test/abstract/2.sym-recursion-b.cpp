/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int zero( int a ) {
    if ( a % 2 == 0 )
        return 42;
    else
        return zero( a - 1 );
}

int main() {
    __sym int a;
    int s = zero( a );
    assert( s == 42 ); /* ERROR */
    return 0;
}
