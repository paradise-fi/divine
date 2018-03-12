/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct S {
    int64_t x;
};

int main() {
    _SYM int x;
    S s;
    s.x = x;
    assert( s.x == x );
}
