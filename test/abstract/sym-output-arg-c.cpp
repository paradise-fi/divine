/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

void init_impl( int * i, int v ) {
    *i = v % 10;
}

void init( int * i ) {
    int v = __sym_val_i32();
    init_impl( i, v );
}

int main() {
    int i;
    init( &i );
    assert( i < 10 );
}
