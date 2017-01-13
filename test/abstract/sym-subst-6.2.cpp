/* VERIFY_OPTS: --symbolic */

#include <assert.h>
#include <dios.h>

#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym long x;
    x %= 8;
    while ( true ) {
        x = (x + 1) % 8;
        __vm_trace( _VM_T_Text, "x" );
    }
}
