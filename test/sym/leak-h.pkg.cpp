/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>
#include <stdlib.h>

void set( int * addr ) {
    *addr = __sym_val_i32();
}

int main() {
    int * a = static_cast< int * >( malloc(sizeof(int)) );
    set( a );
    free( a );
    return 0;
}
