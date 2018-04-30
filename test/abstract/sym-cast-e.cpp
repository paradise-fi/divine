/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int main() {
    union {
        int x;
        short y;
    };

    y = static_cast< int >( __sym_val_i32() );

    if ( short( x ) < 0 )
        return 0;
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}
