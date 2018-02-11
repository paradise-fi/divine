#include <abstract/domains.h>
#include <sys/fault.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

#define _SVC_NONDET(t,n) t __VERIFIER_nondet_ ## n () { _SYM t x; return x; }

extern "C"
{

    _SVC_NONDET(bool, bool)
    _SVC_NONDET(char, char)
    _SVC_NONDET(int, int)
    _SVC_NONDET(short, short)
    _SVC_NONDET(long, long)
    _SVC_NONDET(unsigned int, uint)
    _SVC_NONDET(unsigned long, ulong)
    _SVC_NONDET(unsigned, unsigned)

    void __VERIFIER_error()
    {
        __dios_fault( _DiOS_F_Assert, "verifier error called" );
    }

    void __VERIFIER_assume( int expression )
    {
        if ( !expression )
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Interrupted | _VM_CF_Cancel | _VM_CF_Mask,
                          _VM_CF_Interrupted | _VM_CF_Cancel );
    }

    void __VERIFIER_atomic_begin()
    {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
    }

    void __VERIFIER_atomic_end()
    {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_None );
    }

    void __VERIFIER_assert( int cond )
    {
        assert( cond );
    }

}

#pragma clang diagnostic pop
