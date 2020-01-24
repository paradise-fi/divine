/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return TAGS: todo
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit TAGS: todo
#include <sys/lamp.h>

#include <array>

#include <assert.h>
#include <stdlib.h>

void foo( int * addr ) {
    auto val = __lamp_any_i32();
    *addr = val;
}

int main() {
    int buff;
    int * addr = &buff;
    int val = __lamp_any_i32();
    *addr = val;
    *addr = 10;

    foo( addr );

    return 0;
}
