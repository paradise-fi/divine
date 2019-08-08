/* TAGS: star min */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <assert.h>

int main() {
    int v = __unit_val_i32();
    assert( v ); /* ERROR */
}
