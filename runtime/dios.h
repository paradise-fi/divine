// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_H__
#define __DIOS_H__

#include <divine.h>
#include <divine/metadata.h>

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
    _DiOS_F_MissingFunction,
    _DiOS_F_MainReturnValue,
    _DiOS_F_Last
};

#define __dios_assert_v( x, msg ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d: %s", \
                __FILE__, __LINE__, msg ); \
            __vm_fault( (_VM_Fault) _DiOS_F_Assert ); \
        } \
    } while (0)

#define __dios_assert( x ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d", \
                __FILE__, __LINE__ ); \
            __vm_fault( (_VM_Fault) _DiOS_F_Assert ); \
        } \
    } while (0)

typedef unsigned _DiOS_ThreadId;
typedef const _MD_Function * _DiOS_FunPtr;

/*
 * Start a new thread and obtain its identifier. Thread starts executing routine
 * [void (*routine) (void *)] with arg. Cleanup [void (*cleanup)(int)] handler
 * is called when __dios_kill_thread is executed. User-specified kill reason
 * can be specified.
 */
_DiOS_ThreadId __dios_start_thread( _DiOS_FunPtr routine, void *arg,
    _DiOS_FunPtr cleanup ) NOTHROW;

/*
 * Get caller thread id
 */
_DiOS_ThreadId __dios_get_thread_id() NOTHROW;

/*
 * Kill thread with given id and reason. Reason is an arbitratry user-defined
 * value.
 */
void __dios_kill_thread( _DiOS_ThreadId id, int reason ) NOTHROW;

/*
 * Get function pointer based on function name
 */
_DiOS_FunPtr __dios_get_fun_ptr( const char* name ) NOTHROW;

/*
 * Jump into DiOS kernel and then return back. Has no effect
 */
void __dios_dummy() NOTHROW;

/*
 * Interrupt execution and go to scheduler
 */
void __dios_syscall_trap() NOTHROW;

/*
 * Issue DiOS syscall with given args. Return value is stored in ret.
 */
void __dios_syscall(int syscode, void* ret, ...);

void __dios_trace( int indent, const char *fmt, ... ) NOTHROW;
void __dios_trace_t( const char *str ) NOTHROW;
void __dios_trace_f( const char *fmt, ... ) NOTHROW;

_Noreturn void __dios_unwind( struct _VM_Frame *to, void (*pc)( void ) ) NOTHROW;

CPP_END
#undef EXTERN_C
#undef CPP_END
#undef NOTHROW


#ifdef __cplusplus

namespace __dios {

template < class T, class... Args >
T *new_object( Args... args ) {
    T* obj = static_cast< T * >( __vm_make_object( sizeof( T ) ) );
    new (obj) T( args... );
    return obj;
}

struct Scheduler;
struct Syscall;

struct Context {
    Scheduler *scheduler;
    Syscall *syscall;

    static Context *_ctx;

    Context();
    static Context *get() { return _ctx; }
};

template< bool fenced >
struct _InterruptMask {

    _InterruptMask() :  _owns( true ) {
        _orig_state = __vm_mask( 1 );
        if ( fenced )
            __sync_synchronize();
    }

    ~_InterruptMask() {
        if ( _owns )
            __vm_mask( _orig_state );
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

    void release() {
        assert( _owns );
        if ( fenced )
            __sync_synchronize();
        __vm_mask( 0 );
    }

    // acquire mask if not masked already
    void acquire() {
        assert( _owns );
        if ( fenced )
            __sync_synchronize();
        __vm_mask( 1 );
    }

    bool owned() const { return _owns; }

  private:
    bool _owns;
    int _orig_state;
};

using InterruptMask = _InterruptMask< false >;
using FencedInterruptMask = _InterruptMask< true >;

} // namespace __dios

#endif // __cplusplus

#endif // __DIOS_H__
