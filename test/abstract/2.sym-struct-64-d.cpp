/* VERIFY_OPTS: --symbolic */

#include <cstdint>
#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

struct S { int64_t val; };
struct T { int64_t val; };
struct U { T t; };
struct V { S s; U u; };

int main() {
    V a, b;
    __sym int x;
    a.s.val = x;
    b.u.t.val = a.s.val;
    assert( a.s.val == b.u.t.val );
}
