/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct S {
    S() : x(0), y(0) {}

    int64_t x, y;
};

int main() {
    S a, b;
    a.y = __sym_val_i32();
    b = a; // TODO peeking from mem copied value
    assert( a.x == b.x && a.y == b.y );
}
