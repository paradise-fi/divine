/* TAGS: min c tso divine-internal */
/* VERIFY_OPTS: --relaxed-memory tso:2 */

/* this is a regression test demonstrating error in an early version of lazy
 * x86-TSO in DIVINE */

#include <pthread.h>
#include <sys/divm.h>
#include <sys/lart.h>

#define REACH( X ) assert( !(X) )

volatile int x, y, z;
volatile int bypass_sig;

void *t1( void *_ ) {
    x = 1;
    y = 1;
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _LART_CF_RelaxedMemRuntime );
    bypass_sig = 1;
    while ( bypass_sig != 2 ) { }
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _VM_CF_None );
    __sync_synchronize();
    return NULL;
}


int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );

    /* wait for the other thread to put everything to SB */
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _LART_CF_RelaxedMemRuntime );
    while ( bypass_sig != 1 ) { }
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _VM_CF_None );

    x = 2;
    y = 2;
    y = 3; /* forces flush of x = 2 while x = 1 is still in SB of t1 */

    /* let the other thread flush its SB */
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _LART_CF_RelaxedMemRuntime );
    bypass_sig = 2;
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _LART_CF_RelaxedMemRuntime, _VM_CF_None );
    pthread_join( t1t, NULL );

    REACH( y == 3 && x == 2 ); /* ERROR */
}

