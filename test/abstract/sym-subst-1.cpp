/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>

int main() {
    int x = __sym_val_i32();
    int y = __sym_val_i32();
    assert( x > y ); /* ERROR */
}
