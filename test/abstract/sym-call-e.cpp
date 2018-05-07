/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int* init_and_pass( int* val ) {
    *val = __sym_val_i32();
    return val;
}

int process( int* addr ) {
    auto ret = init_and_pass( addr );
    return *ret;
}

int main() {
    int i;
    int j = process( &i );
    assert( i == j );
}



