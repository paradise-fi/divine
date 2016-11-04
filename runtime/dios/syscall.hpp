// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <dios.h>
#ifndef RECALL
#define RECALL 221
#endif

namespace __dios {

#define SYSCALL(n,...) _SC_ ## n,
enum _DiOS_SC {
    _SC_INACTIVE = 0,

    #include <dios/syscall.def>

    _SC_LAST
};

// Mapping of syscodes to implementations
extern void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( Context& ctx, int *err, void* retval, va_list vl );

struct Syscall {
    Syscall() noexcept : _syscode( _SC_INACTIVE ) {
        _inst = this;
    }

    static void trap(int syscode, int* err, void* ret, va_list& args) noexcept
    {
        _inst->_syscode = static_cast< _DiOS_SC >( syscode );
        _inst->_ret = ret;
        _inst->_err = err;
        va_copy( _inst->_args, args );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
        va_end( _inst->_args );
    }

    bool handle( Context *ctx ) noexcept {
        if ( _syscode != _SC_INACTIVE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( *ctx, _err ,_ret , _args );
            _syscode = _SC_INACTIVE;
            if (*_err == RECALL ) {
                return false;
            }
            // If syscall returns, scheduler has to continue current thread
            // It it does not return, scheduler can continue with an arbitrary thread
            return _ret;
        }
        return false;
    }

private:
    _DiOS_SC _syscode;
    int *_err;
    void *_ret;
    va_list _args;

    static Syscall *_inst;
};

} // namespace __dios

namespace __sc {

    void uname( __dios::Context& c, int *err, void* retval, va_list vl );

} // namespace __sc


#endif // __DIOS_SYSCALL_H__
