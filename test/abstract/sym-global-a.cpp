/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int x;

int get() { return x; }

int main() {
    int val = __sym_val_i32();
    x = val;

    int y = get();
    assert( x == y );
}
