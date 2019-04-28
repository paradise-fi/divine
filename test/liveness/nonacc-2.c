/* TAGS: c min */
/* VERIFY_OPTS: --liveness */
/* CC_OPTS: -Os */ // avoid duplicated states

#include <dios.h>
#include <sys/divm.h>
#include <stdbool.h>

int next( int state ) {
    switch ( state ) {
        case -1:
            return 0;
        case 0:
            return __vm_choose( 2 ); /* 0, 1 */
        case 1:
            __vm_ctl_flag( 0, _VM_CF_Accepting );
            return 2;
        case 2:
            return 3;
        case 3:
            return 2;

    }
    return 0;
}

int main() {
    int state = -1, oldstate;

    while ( true ) {
        oldstate = 0;
        __dios_reschedule();
        oldstate = state;
        state = next( state );
        __dios_trace_f( "state: %d -> %d", oldstate, state );
    }
}
