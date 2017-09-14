/* VERIFY_OPTS: --symbolic */
#include <cstdint>
#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

struct S {
    int64_t x;
};

int main() {
    __sym int x;
    S s;
    s.x = x;
    assert( s.x == x );
}
