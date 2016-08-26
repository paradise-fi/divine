// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <dios.h>

namespace __dios {

// Syscodes
enum _DiOS_SC {
    _SC_INACTIVE = 0,
    _SC_START_THREAD,
    _SC_GET_THREAD_ID,
    _SC_KILL_THREAD,
    _SC_DUMMY,

    _SC_CONFIGURE_FAULT,
    _SC_GET_FAULT_CONFIG,

    _SC_LAST
};

// Mapping of syscodes to implementations
extern void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( Context& ctx, void* retval, va_list vl );

struct Syscall {
    Syscall() noexcept : _syscode( _SC_INACTIVE ) {
        _inst = this;
    }

    static void call(int syscode, void* ret, va_list& args) noexcept {
        _inst->_syscode = static_cast< _DiOS_SC >( syscode );
        _inst->_ret = ret;
        va_copy( _inst->_args, args );
        __dios_syscall_trap();
        va_end( _inst->_args );
    }

    bool handle( Context *ctx ) noexcept {
        if ( _syscode != _SC_INACTIVE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( *ctx, _ret, _args );
            _syscode = _SC_INACTIVE;
            return true;
        }
        return false;
    }

private:
    _DiOS_SC _syscode;
    void *_ret;
    va_list _args;

    static Syscall *_inst;
};

} // namespace __dios

namespace __sc {

    void dummy( __dios::Context& c, void* retval, va_list vl );

} // namespace __sc


#endif // __DIOS_SYSCALL_H__
