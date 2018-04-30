/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct S {
    int64_t x;
};

int main() {
    int x = __sym_val_i32();
    S s;
    s.x = x;
    assert( s.x == x );
}
