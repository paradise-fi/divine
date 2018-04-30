/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int main() {
    int x = static_cast< int >( __sym_val_i32() );
    if ( x < 0 )
        return 0;
    short y = x;
    ++y;
    assert( x + 1 == y ); /* ERROR */
}
