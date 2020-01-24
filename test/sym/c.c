/* TAGS: sym min c */
/* VERIFY_OPTS: --symbolic */
#include <sys/lamp.h>

#include <assert.h>

int main() {
    int i = __lamp_any_i32();
    assert( i != 0 ); /* ERROR */
}
