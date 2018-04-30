/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int x = 7;

int get() { return x; }

int main() {
    assert( x == 7 );

    int val = __sym_val_i32();
    x = x + val;
    assert( val + 7 == x );
}
