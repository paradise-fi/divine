/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

struct S {
    S() : x(0), y(0) {}

    int x, y;
};

int main() {
    S a, b;
    _SYM int y;
    a.y = y;
    b = a;
    assert( a.x == b.x && a.y == b.y );
}
