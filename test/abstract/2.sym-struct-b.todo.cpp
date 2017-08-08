/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

struct S {
    S() : x(0), y(0) {}

    int x, y;
};

int main() {
    S a, b;
    __sym int y;
    a.y = y;
    b = a;
    assert( a.x == b.x && a.y == b.y );
}
