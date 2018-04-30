/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <cstdint>
#include <cassert>

int main() {
    uint64_t x = __sym_val_i64();
    uint64_t y = 0;
    assert( x != y ); /* ERROR */
}
