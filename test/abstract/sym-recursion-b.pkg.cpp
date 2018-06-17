/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>
#include <limits>
#include <sys/vmutil.h>

int zero( int a ) {
    if ( a % 2 == 0 )
        return 42;
    else
        return zero( a - 1 );
}

int main() {
    int a = __sym_val_i32();
    assert( zero( a ) == 42 );
    return 0;
}
