/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int y = 0;

int main() {
    uint64_t x = __sym_val_i64();
    assert( x != y ); /* ERROR */
}
