/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

struct S {
    int x;
};

int main() {
    int x = __sym_val_i32();
    S s;
    s.x = x;
    assert( s.x != 0 ); /* ERROR */
}
