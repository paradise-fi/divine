/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int zero( int a ) {
    if ( a == 0 )
        return 42;
    else
        return zero( a - 1 );
}

int main() {
    __sym int a;
    int s = zero( a );
    assert( s == 42 );
    return 0;
}
