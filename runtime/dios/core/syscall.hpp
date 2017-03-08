// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <dios.h>
#include <errno.h>
#include <sys/syscall.h>

namespace __dios {

#define SYSCALL(num,name) name = num,
enum _VM_SC {
    #include <dios/core/systable.def>
};
#undef SYSCALL

enum class SchedCommand : uint8_t { RESCHEDULE, CONTINUE };

using SC_Handler = void (*)( Context& ctx, int *err, void* retval, va_list vl );

// Mapping of syscodes to implementations
extern const SC_Handler * _DiOS_SysCalls;
extern const SC_Handler _DiOS_SysCalls_Virt[ SYS_MAXSYSCALL ];
extern const SC_Handler _DiOS_SysCalls_Passthru[ SYS_MAXSYSCALL ];

// True if corresponding syscall requires thread rescheduling
extern const SchedCommand _DiOS_SysCallsSched[ SYS_MAXSYSCALL ];

struct Syscall : _DiOS_Syscall
{
    Syscall() noexcept { _syscode = SYS_NONE; }

    SchedCommand handle( Context *ctx ) noexcept
    {
        if ( _syscode != SYS_NONE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( *ctx, _err ,_ret , _args );
            if ( *_err == EAGAIN2 )
            {
                _syscode = SYS_NONE;
                return SchedCommand::RESCHEDULE;;
            }
            auto cmd = _DiOS_SysCallsSched[ _syscode ];
            _syscode = SYS_NONE;
            // Either CONTINUE or RESCHEDULE
            return cmd;
        }
        return SchedCommand::RESCHEDULE;
    }
};

} // namespace __dios

namespace __sc {

    void uname( __dios::Context& c, int *err, void* retval, va_list vl );

} // namespace __sc


#endif // __DIOS_SYSCALL_H__
