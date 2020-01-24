/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <sys/lamp.h>
#include <assert.h>

int value;

int get(int * addr) {
    return *addr;
}

int main(void) {
    value = __lamp_any_i32();

    int loaded = get(&value);
    assert( loaded == value );
    return 0;
}

