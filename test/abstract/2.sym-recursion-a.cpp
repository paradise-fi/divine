/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int sum( int a, int b ) {
    if ( a == 0 )
        return b;
    else
        return sum( a - 1, b + 1 );
}

int main() {
    __sym int b;
    int s = sum( 10, b );
    assert( s == 10 + b );
    return 0;
}
