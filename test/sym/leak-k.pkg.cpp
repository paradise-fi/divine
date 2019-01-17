/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return VERIFY_OPTS: --leakcheck return
// V: v.state VERIFY_OPTS: --leakcheck state
// V: v.exit VERIFY_OPTS: --leakcheck exit
#include <rst/domains.h>

#include <array>

#include <assert.h>
#include <stdlib.h>

void foo( int * addr ) {
    auto val = __sym_val_i32();
    *addr = val;
}

int main() {
    int buff;
    int * addr = &buff;
    int val = __sym_val_i32();
    *addr = val;
    *addr = 10;

    foo( addr );

    return 0;
}
