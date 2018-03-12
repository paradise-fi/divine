/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>
#include <dios.h>

int main() {
    _SYM long x;
    x %= 8;
    while ( true ) {
        x = (x + 1) % 8;
        __vm_trace( _VM_T_Text, "x" );
    }
}
