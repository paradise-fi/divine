/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>
unsigned sum( unsigned a, unsigned b );

unsigned sum_impl( unsigned a, unsigned b ) {
    return sum( a - 1, b + 1 );
}

unsigned sum( unsigned a , unsigned b ) {
    if ( b == 0 )
        return a;
    if ( a == 0 )
        return b;
    return sum_impl( a, b );
}


int main() {
    _SYM int b;
    int s = sum( 4, b );
    assert( s == 4 + b );
    return 0;
}
