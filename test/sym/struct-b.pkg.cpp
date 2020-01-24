/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min todo
// V: v.O1 CC_OPT: -O1 TAGS: todo
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>

struct S {
    S() : x(0), y(0) {}

    int x, y;
};

int main() {
    S a, b;
    int y = __lamp_any_i32();
    a.y = y;
    b = a; // TODO peeking from mem copied value
    assert( a.x == b.x && a.y == b.y );
}
