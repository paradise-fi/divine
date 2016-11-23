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

#include <stddef.h>

enum _DiOS_Fault
{
    _DiOS_F_Threading = _VM_F_Last,
    _DiOS_F_Assert,
    _DiOS_F_Config,
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

typedef void * _DiOS_ThreadId;
typedef void * _DiOS_ProcId;

enum _DiOS_TLS_Offsets {
    /* offset of errno from beginning of TLS */
    _DiOS_TLS_ErrnoOffset = 0,
    /* amount of TLS which is reserved for DiOS */
    _DiOS_TLS_Reserved = sizeof( int ),
};

/*
 * Start a new thread and obtain its identifier. Thread starts executing routine
 * with arg.
 * - tls_size is the total size of TLS, _DiOS_TLS_Reserved must be included in this,
 *   if tls_size is less then _DiOS_TLS_Reserved at least _DiOS_TLS_Reserved is allocated
 * - the resulting _DiOS_ThreadId points to the beginning of TLS. Userspace is
 *   allowed to use it from offset _DiOS_TLS_Reserved
 */
_DiOS_ThreadId __dios_start_thread( void ( *routine )( void * ), void *arg, int tls_size ) NOTHROW;

/*
 * Get caller thread id
 *
 * - the resulting _DiOS_ThreadId points to the beginning of TLS. Userspace is
 *   allowed to use it from offset _DiOS_TLS_Reserved
 */
_DiOS_ThreadId __dios_get_thread_id() NOTHROW;

/*
 * get pointer to errno, which is in dios-managed thread-local data (accessible
 * to userspace, but independent of pthreading library)
 */
int *__dios_get_errno() NOTHROW;

/*
 * Kill thread with given id.
 */
void __dios_kill_thread( _DiOS_ThreadId id ) NOTHROW;

/*
 * Kill process with given id. If NULL is passed, all processes are killed.
 */
void __dios_kill_process( _DiOS_ProcId id ) NOTHROW;


_DiOS_ThreadId *__dios_get_process_threads() NOTHROW;

/*
 * Return number of claimed hardware concurrency units, specified in DiOS boot
 * parameters.
 */
int __dios_hardware_concurrency() NOTHROW;

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
void __dios_fault( int f, const char *msg, ... ) NOTHROW __attribute__(( __noinline__ ));


void __dios_trace( int indent, const char *fmt, ... ) NOTHROW;
void __dios_trace_t( const char *str ) NOTHROW;
void __dios_trace_f( const char *fmt, ... ) NOTHROW;
void __dios_trace_i( int indent_level, const char *fmt, ... ) NOTHROW;

// unwind and free frames on stack 'stack' from 'from' to 'to' so that 'to'
// the frame which originally returned to 'from' now returns to 'to'
// * 'stack' can be nullptr if unwinding on local stack
// * 'from' can be nullptr if everything from the caller of __dios_unwind should be unwound
// * 'to' can be nullptr if all frames from 'from' below should be destroyed
//
// i.e. __dios_unwind( nullptr, nullptr, nullptr ) destroys complete stack
// except for the caller of __dios_unwind, which will have 'parent' set to
// nullptr
void __dios_unwind( struct _VM_Frame *stack, struct _VM_Frame *from, struct _VM_Frame *to ) NOTHROW __attribute__((__noinline__));

// transfer control to given frame and program counter, if restoreMaskTo is -1
// it does not change mask
void __dios_jump( struct _VM_Frame *to, void (*pc)( void ), int restoreMaskTo ) NOTHROW __attribute__((__noinline__));

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


#ifdef __cplusplus
#if defined( __divine__ ) || defined( DIVINE_NATIVE_RUNTIME )

#include <cstdint>
#include <dios/stdlibwrap.hpp>

namespace divine {
    namespace fs {
        struct VFS;
    }
}

namespace __dios {

template < class T, class... Args >
T *new_object( Args... args ) {
    T* obj = static_cast< T * >( __vm_obj_make( sizeof( T ) ?: 1 ) );
    new (obj) T( args... );
    return obj;
}

template < class T >
void delete_object( T *obj ) {
    obj->~T();
    __vm_obj_free( obj );
}

using SysOpts = dvector< std::pair< dstring, dstring > >;

struct Scheduler;
struct Syscall;
struct Fault;
using VFS = divine::fs::VFS;

struct MachineParams {
    int hardwareConcurrency;

    void initialize( const SysOpts& opts );
    void traceParams( int indent );
};

struct Context {
    Scheduler *scheduler;
    Syscall *syscall;
    Fault *fault;
    VFS *vfs;
    void *globals;
    MachineParams machineParams;

    Context();
    void finalize();
};

template< bool fenced >
struct _InterruptMask {

    _InterruptMask()
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
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, uintptr_t( _VM_CF_Mask ),
                      _orig_state ? uintptr_t( _VM_CF_Mask ) : 0ull );
    }

  private:
    struct Without {
        Without( _InterruptMask &self ) : self( self ) {
            if ( !self._orig_state ) {
                if ( fenced )
                    __sync_synchronize();
                __vm_control( _VM_CA_Bit, _VM_CR_Flags, uintptr_t( _VM_CF_Mask ), 0ull );
            }
        }

        // acquire mask if not masked already
        ~Without() {
            if ( !self._orig_state ) {
                if ( fenced )
                    __sync_synchronize();
                __vm_control( _VM_CA_Bit, _VM_CR_Flags,
                              uintptr_t( _VM_CF_Mask ), uintptr_t( _VM_CF_Mask ) );
            }
        }

        _InterruptMask &self;
    };
  public:

#if __cplusplus >= 201103L
    _InterruptMask( const _InterruptMask & ) = delete;
    _InterruptMask( _InterruptMask && ) = delete;
#else
  private:
    _InterruptMask( const _InterruptMask & );
    _InterruptMask &operator=( const _InterruptMask & );
  public:
#endif

#if __cplusplus >= 201402L
    // break mask (if it is held by this guard), call given function and then
    // return mask to original state
    template< typename Fun >
    auto without( Fun &&f, bool mustBreakMask = false ) {
        __dios_assert_v( !mustBreakMask || !_orig_state,
                         "Interrupt does not own interrupt mask, but it is required to by caller of InterruptMask::without" );
        Without _( *this );
        return f();
    }
#endif

    // beware, this is dangerous
    void _setOrigState( bool state ) { _orig_state = state; }
    bool _origState() const { return _orig_state; }

  private:

    bool _orig_state;
};

using InterruptMask = _InterruptMask< false >;
using FencedInterruptMask = _InterruptMask< true >;


struct SetFaultTemporarily {

    SetFaultTemporarily( int fault, int cfg ) NOTHROW :
        _fault( fault ), _orig( __dios_configure_fault( fault, cfg ) )
    { }

    ~SetFaultTemporarily() NOTHROW {
        __dios_configure_fault( _fault, _orig );
    }

  private:
    int _fault;
    int _orig;
};

} // namespace __dios

#endif // __divine__ || DIVINE_NATIVE_RUNTIME

#endif // __cplusplus

#endif // __DIOS_H__

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
