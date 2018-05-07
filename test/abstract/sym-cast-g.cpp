/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int nondet() {
    return static_cast< int >( __sym_val_i32() );
}

int main() {
    int x = nondet();
    if ( short( x ) < 0 )
        return 0;
    short &y = *reinterpret_cast< short * >( &x );
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}

