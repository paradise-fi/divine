/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <cstdlib>
#include <cassert>

int main() {
    int *a = static_cast< int* >( malloc( sizeof(int) ) );
    int v = __sym_val_i32();
    *a = v;
    assert( v == *a );
    free( a );
}


