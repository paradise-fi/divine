/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return VERIFY_OPTS: --leakcheck return
// V: v.state VERIFY_OPTS: --leakcheck state
// V: v.exit VERIFY_OPTS: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct Widget {
    int a = 0, b;
};

int main() {
    Widget w1, w2;
    w1.b = __sym_val_i32();

    memcpy( &w2, &w1, sizeof( Widget ) );
    assert( w1.b == w2.b );

    return 0;
}
