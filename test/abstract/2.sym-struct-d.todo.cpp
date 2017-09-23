/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

struct S { int val; };
struct T { int val; };
struct U { T t; };
struct V { S s; U u; };

int main() {
    V a, b;
    _SYM int x;
    a.s.val = x;
    b.u.t.val = a.s.val;
    assert( a.s.val == b.u.t.val );
}
