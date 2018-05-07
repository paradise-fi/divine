/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

// test output argument
void init( int *i, int v ) { *i = v; }

int main() {
    int val = __sym_val_i32();
    int i;

    init( &i, val );

    assert( i == val );
}
