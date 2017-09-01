/* VERIFY_OPTS: --symbolic */

#include <cstdint>
#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))


struct T {
    int64_t val;
    int padding;
};

struct S {
    int64_t val;
    int padding;
};

int main() {
    S s; T t;
    __sym int val;
    s.val = val;
    t.val = s.val;
    assert( s.val == t.val );
}
