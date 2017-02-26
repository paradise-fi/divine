/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int sum( int a, int b ) {
    if ( a == 0 )
        return b;
    else
        return sum( a - 1, b + 1 );
}

int main() {
    __sym int a;
    __sym int b;
    int s = sum( a, b );
    assert( s == a + b );
    return 0;
}
