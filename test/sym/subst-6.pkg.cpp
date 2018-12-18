/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <assert.h>
#include <dios.h>

int main() {
    int x = __sym_val_i32();
    x %= 8;
    while ( true ) {
        x = (x + 1) % 8;
        __vm_trace( _VM_T_Text, "x" );
    }
}
