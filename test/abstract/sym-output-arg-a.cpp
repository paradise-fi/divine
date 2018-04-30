/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

void init( int * i ) {
    int v = __sym_val_i32();
    *i = v % 10;
}

int main() {
    int i;
    init( &i );
    assert( i < 10 );
}
