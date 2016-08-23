// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <cstdarg>
#include <new>
#include <divine/dios.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#define NOTHROW noexcept
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#endif

EXTERN_C

// Syscodes
enum _DiOS_SC {
    _SC_INACTIVE = 0,
    _SC_START_THREAD,
    _SC_GET_THREAD_ID,
    _SC_KILL_THREAD,
    _SC_DUMMY,

    _SC_LAST
};

// Mapping of syscodes to implementations
extern void ( *_DiOS_SysCalls[_SC_LAST] ) ( void* retval, va_list vl );

void __sc_dummy( void* retval, va_list vl );


CPP_END

namespace __dios {

struct Syscall {
    Syscall() noexcept : _syscode( _SC_INACTIVE ) { }

    void call(int syscode, void* ret, va_list& args) noexcept {
        _syscode = static_cast< _DiOS_SC >( syscode );
        _ret = ret;
        va_copy( _args, args );
        __dios_syscall_trap();
        va_end( _args );
    }

    bool handle() noexcept {
        if ( _syscode != _SC_INACTIVE ) {
            ( *( _DiOS_SysCalls[ _syscode ] ) )( _ret, _args );
            _syscode = _SC_INACTIVE;
            return true;
        }
        return false;
    }

    static Syscall& get() noexcept {
        if ( !instance ) {
            instance = static_cast< Syscall * > ( __vm_make_object( sizeof( Syscall ) ) );
            new ( instance ) Syscall();
        }
        return *instance;
    }

private:
    _DiOS_SC _syscode;
    void *_ret;
    va_list _args;

    static Syscall *instance;
};

} // namespace __dios

#endif // __DIOS_SYSCALL_H__
