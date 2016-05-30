#ifndef __DIOS_H__
#define __DIOS_H__

#include <divine.h>

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

enum _Sys_Fault
{
	_Sys_F_Threading = _VM_F_Last,
	_Sys_F_DiOSAssert,
	_Sys_F_MissingFunction,
	_Sys_F_Last
};

#define dios_assert( x ) do { \
		if ( !(x) ) { \
			__vm_trace( "DIOS_assert");\
			__vm_fault( (_VM_Fault) _Sys_F_DiOSAssert );\
		} \
	} while (0)

/*
 * Sheduler main routine routine
 */
void *__sys_sched( int st_size, void *_state ) NOTHROW;

/*
 * Fault handler
 */
enum _VM_FaultAction __sys_fault( enum _VM_Fault what ) NOTHROW;

typedef unsigned _Sys_ThreadId;
typedef struct _VM_FunctionInfo * _Sys_FunPtr;

/*
 * Start a new thread and obtain its identifier. Thread starts executing routine
 * [void (*routine) (void *)] with arg. Cleanup [void (*cleanup)(int)] handler,
 * which is called when __sys_kill_thread is executed. User-specified kill
 * reason can be specified.
 */
_Sys_ThreadId __sys_start_thread( _Sys_FunPtr routine, void *arg,
	_Sys_FunPtr cleanup ) NOTHROW;

/*
 * Get caller _Sys_ThreadId
 */
_Sys_ThreadId __sys_get_thread_id() NOTHROW;

/*
 * Kill thread with given id and reason. Reason is an arbitratry user-defined
 * value.
 */
void __sys_kill_thread( _Sys_ThreadId id, int reason ) NOTHROW;

/*
 * Get function pointer based on function name
 */
_Sys_FunPtr __sys_get_fun_ptr( const char* name ) NOTHROW;

CPP_END

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW


#ifdef __cplusplus

namespace dios {

template< bool fenced >
struct _InterruptMask {

    _InterruptMask() {
        _owns = !__vm_mask( 1 );
        _masked = true;
        if ( fenced && _owns )
            __sync_synchronize();
    }

    ~_InterruptMask() {
        release();
    }

#if __cplusplus >= 201103L
    _InterruptMask( const _InterruptMask & ) = delete;
    // ownership transfer
    _InterruptMask( _InterruptMask &&o ) : _owns( o._owns ), _masked( o._masked ) {
        o._owns = false;
    }
#else
  private:
      _InterruptMask( const _InterruptMask & ) { };
      _InterruptMask &operator=( const _InterruptMask & ) { return *this; }
  public:
#endif

    // if mask isowned by this object release it
    void release() {
        if ( _owns ) {
            _owns = false;
            _masked = false;
            if ( fenced )
                __sync_synchronize();
            __vm_mask( 0 );
        }
    }

    // acquire mask if not masked already
    void acquire() {
        if ( !_masked ) {
            _owns = !__vm_mask( 1 );
            _masked = true;
            if ( fenced && _owns )
                __sync_synchronize();
        }
    }

    // break mask, only if this owns it!
    void breakMask() {
        if ( _owns )
            _unsafeBreakMask();
    }

    bool owned() const { return _owns; }

  private:
    // this should not be inlined, so it does not produce register change
    // in subsequent calls in cycle
    void _unsafeBreakMask() __attribute__((__noinline__)) {
        if ( fenced )
            __sync_synchronize();
        __vm_mask( 0 );
        __vm_mask( 1 );
        if ( fenced )
            __sync_synchronize();
    }

    bool _owns;
    bool _masked;
};

using InterruptMask = _InterruptMask< false >;
using FencedInterruptMask = _InterruptMask< true >;

} // namespace dios

#endif

#endif // __DIOS_H__
