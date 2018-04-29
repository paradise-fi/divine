#include <rst/lart.h>
#include <sys/divm.h>

extern "C" {
    uintptr_t __lart_unstash() {
        return reinterpret_cast< uintptr_t >( __vm_ctl_get( _VM_CR_User5 ) );
    }

    void __lart_stash( uintptr_t val ) {
        __vm_ctl_set( _VM_CR_User5, reinterpret_cast< void* >( val ) );
    }
}
