/* TAGS: c */
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
            return 1;
        case 1:
            return __vm_choose( 2 ) ? 2 : (__vm_ctl_flag( 0, _VM_CF_Accepting ), 10);
        case 2:
            return __vm_choose( 2 ) ? 11 : 3;
        case 3:
            return __vm_choose( 2 )
                      ? 4
                      : (__vm_choose( 2 ) ? 5
                                          : (__vm_ctl_flag( 0, _VM_CF_Accepting ), 12));
        case 4:
            return 5;
        case 5:
            return 6;
        case 6:
            return 1;
        case 12:
            return 13 + __vm_choose( 2 );
        case 13:
            return 14;
        default:
            return state;
    }
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
