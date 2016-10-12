// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_H__
#define __DIOS_H__

#include <divine.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#endif

EXTERN_C

enum _DiOS_Fault
{
    _DiOS_F_Threading = _VM_F_Last,
    _DiOS_F_Assert,
    _DiOS_F_Last
};

enum _DiOS_SimFail
{
    _DiOS_SF_Malloc = _DiOS_F_Last,
    _DiOS_SF_Last
};

enum _DiOS_FaultConfig
{
    _DiOS_FC_EInvalidCfg = -3,
    _DiOS_FC_EInvalidFault = -2,
    _DiOS_FC_ELocked = -1,
    _DiOS_FC_Ignore = 0,
    _DiOS_FC_Report,
    _DiOS_FC_Abort,
    _DiOS_FC_NoFail,
    _DiOS_FC_SimFail,
};

typedef int _DiOS_ThreadId;

/*
 * Start a new thread and obtain its identifier. Thread starts executing routine
 * with arg.
 */
_DiOS_ThreadId __dios_start_thread( void ( *routine )( void * ), void *arg ) NOTHROW;

/*
 * Get caller thread id
 */
_DiOS_ThreadId __dios_get_thread_id() NOTHROW;

/*
 * Kill thread with given id and reason. Reason is an arbitratry user-defined
 * value.
 */
void __dios_kill_thread( _DiOS_ThreadId id ) NOTHROW;

/*
 * Jump into DiOS kernel and then return back. Has no effect
 */
void __dios_dummy() NOTHROW;

/*
 * Issue DiOS syscall with given args. Return value is stored in ret.
 */
void __dios_syscall(int syscode, void* ret, ...) NOTHROW;

/*
 * Configures given fault or symfail and returns original value. Possible
 * configurations are _DiOS_FC_{Ignore, Report, Abort, NoFail, SimFail}.
 *
 * Return original configuration or error.
 * Possible errors:
 *   - _DiOS_FC_ELocked if user forced cofiguration,
 *   - _DiOS_FC_EInvalidFault if invalid fault number was specified
 *   - _DiOS_FC_EInvalidCfg if simfail configuration was passed for fault or
 *     fault confiugation was passed for simfail.
 */
int __dios_configure_fault( int fault, int cfg ) NOTHROW;

/*
 * Return fault configuration. Possible configurations are
 * _DiOS_FC_{Ignore, Report, Abort, NoFail, SimFail}.
 *
 * Can return _DiOS_FC_EInvalidFault if invalid fault was specified.
 */
int __dios_get_fault_config( int fault ) NOTHROW;

/*
 * Cause program fault by calling fault handler. Remaining arguments are
 * ignored, but can be examined by the fault handler, since it is able to obtain
 * a pointer to the call instruction which invoked __vm_fault by reading the
 * program counter of its parent frame.
 */
void __dios_fault( enum _VM_Fault f, const char *msg, ... ) NOTHROW __attribute__(( __noinline__ ));


void __dios_trace( int indent, const char *fmt, ... ) NOTHROW;
void __dios_trace_t( const char *str ) NOTHROW;
void __dios_trace_f( const char *fmt, ... ) NOTHROW;

_Noreturn void __dios_unwind( struct _VM_Frame *to, void (*pc)( void ) ) NOTHROW;

#define __dios_assert_v( x, msg ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d: %s", \
                __FILE__, __LINE__, msg ); \
            __dios_fault( (_VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

#define __dios_assert( x ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d", \
                __FILE__, __LINE__ ); \
            __dios_fault( (_VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

CPP_END
#undef EXTERN_C
#undef CPP_END
#undef NOTHROW


#ifdef __cplusplus
#if defined( __divine__ ) || defined( DIVINE_NATIVE_RUNTIME )

#include <cstdint>

namespace __dios {

template < class T, class... Args >
T *new_object( Args... args ) {
    T* obj = static_cast< T * >( __vm_obj_make( sizeof( T ) ) );
    new (obj) T( args... );
    return obj;
}

struct Scheduler;
struct Syscall;
struct Fault;

struct Context {
    Scheduler *scheduler;
    Syscall *syscall;
    Fault *fault;
    void *globals;

    Context();
};

template< bool fenced >
struct _InterruptMask {

    _InterruptMask()
        : _owns( true )
    {
        _orig_state = uintptr_t( __vm_control( _VM_CA_Get, _VM_CR_Flags,
                                               _VM_CA_Bit, _VM_CR_Flags,
                                               uintptr_t( _VM_CF_Mask ),
                                               uintptr_t( _VM_CF_Mask ) ) )
                      & _VM_CF_Mask;
        if ( fenced )
            __sync_synchronize();
    }

    ~_InterruptMask() {
        if ( _owns )
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, uintptr_t( _VM_CF_Mask ),
                          _orig_state ? uintptr_t( _VM_CF_Mask ) : 0ull );
    }

#if __cplusplus >= 201103L
    _InterruptMask( const _InterruptMask & ) = delete;
    // ownership transfer
    _InterruptMask( _InterruptMask &&o ) : _owns(o._owns), _orig_state( o._orig_state ) {
        o._owns = false;
    }
#else
  private:
      _InterruptMask( const _InterruptMask & ) { };
      _InterruptMask &operator=( const _InterruptMask & ) { return *this; }
  public:
#endif

    void release()
    {
        if ( fenced )
            __sync_synchronize();
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, uintptr_t( _VM_CF_Mask ), 0ull );
    }

    // acquire mask if not masked already
    void acquire()
    {
        if ( fenced )
            __sync_synchronize();
        __vm_control( _VM_CA_Bit, _VM_CR_Flags,
                      uintptr_t( _VM_CF_Mask ), uintptr_t( _VM_CF_Mask ) );
    }

  private:
    bool _owns;
    bool _orig_state;
};

using InterruptMask = _InterruptMask< false >;
using FencedInterruptMask = _InterruptMask< true >;

} // namespace __dios

#endif // __divine__ || DIVINE_NATIVE_RUNTIME

#endif // __cplusplus

#endif // __DIOS_H__
