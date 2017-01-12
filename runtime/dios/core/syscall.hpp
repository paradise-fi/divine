// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <dios.h>
#ifndef _DiOS_SYS_RETRY
#define _DiOS_SYS_RETRY 221
#endif

namespace __dios {

#define SYSCALL(n,...) _SC_ ## n,
enum _DiOS_SC {
    _SC_INACTIVE = 0,

    #include <dios/core/syscall.def>

    _SC_LAST
};
#undef SYSCALL

enum class SchedCommand : uint8_t { RESCHEDULE, CONTINUE };

// Mapping of syscodes to implementations
extern void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( Context& ctx, int *err, void* retval, va_list vl );
// True if corresponding syscall requires thread rescheduling
extern const SchedCommand _DiOS_SysCallsSched[ _SC_LAST ];

struct Syscall {
    Syscall() noexcept : _syscode( _SC_INACTIVE ) {}

    static void trap(int syscode, int* err, void* ret, va_list& args) noexcept
    {
        Syscall inst;
        inst._syscode = static_cast< _DiOS_SC >( syscode );
        inst._ret = ret;
        inst._err = err;
        va_copy( inst._args, args );
        __vm_control( _VM_CA_Set, _VM_CR_User1, &inst );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask );
        va_end( inst._args );
    }

    SchedCommand handle( Context *ctx ) noexcept {
        if ( _syscode != _SC_INACTIVE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( *ctx, _err ,_ret , _args );
            if ( *_err == _DiOS_SYS_RETRY ) {
                _syscode = _SC_INACTIVE;
                return SchedCommand::RESCHEDULE;;
            }
            auto cmd = _DiOS_SysCallsSched[ _syscode ];
            _syscode = _SC_INACTIVE;
            // Either CONTINUE or RESCHEDULE
            return cmd;
        }
        return SchedCommand::RESCHEDULE;
    }

private:
    _DiOS_SC _syscode;
    int *_err;
    void *_ret;
    va_list _args;
};

} // namespace __dios

namespace __sc {

    void uname( __dios::Context& c, int *err, void* retval, va_list vl );

} // namespace __sc


#endif // __DIOS_SYSCALL_H__
