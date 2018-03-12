/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int sum( int a, int b ) {
    if ( a == 0 )
        return b;
    else
        return sum( a - 1, b + 1 );
}

int main() {
    _SYM int b;
    int s = sum( 10, b );
    assert( s == 10 + b );
    return 0;
}
