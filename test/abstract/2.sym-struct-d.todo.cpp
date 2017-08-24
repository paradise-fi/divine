/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

struct S { int val; };
struct T { int val; };
struct U { T t; };
struct V { S s; U u; };

int main() {
    V a, b;
    __sym int x;
    a.s.val = x;
    b.u.t.val = a.s.val;
    assert( a.s.val == b.u.t.val );
}
