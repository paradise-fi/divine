/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>
#include <limits>

void * nondet() {
    return reinterpret_cast< void * >( __lamp_any_i64() );
}

int main() {
    auto x = reinterpret_cast< uint64_t >( nondet() );
}

