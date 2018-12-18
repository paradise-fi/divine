/* TAGS: sym min c */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <assert.h>

int main() {
    int i = __sym_val_i32();
    assert( i != 0 ); /* ERROR */
}
