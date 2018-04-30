/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

struct S { int val; };
struct T { int val; };
struct U { T t; };
struct V { S s; U u; };

int main() {
    V a, b;
    int x = __sym_val_i32();
    a.s.val = x;
    b.u.t.val = a.s.val;
    assert( a.s.val == b.u.t.val );
}
