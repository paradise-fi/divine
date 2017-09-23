/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cstdint>
#include <cassert>

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
    _SYM int val;
    s.val = val;
    t.val = s.val;
    assert( s.val == t.val );
}
