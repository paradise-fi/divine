/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <rst/domains.h>
#include <assert.h>

int value;

int get(int * addr) {
    return *addr;
}

int main(void) {
    value = __sym_val_i32();

    int loaded = get(&value);
    assert( loaded == value );
    return 0;
}

