#ifndef __DIVINE_H__
#define __DIVINE_H__

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

#ifdef DIVINE_NATIVE_RUNTIME
#define NATIVE_VISIBLE __attribute__ ((visibility("default")))
#else
#define NATIVE_VISIBLE
#endif

struct _VM_Frame
{
    void (*pc)(void);
    struct _VM_Frame *parent;
};

enum _VM_Fault
{
    _VM_F_Assert,
    _VM_F_Arithmetic, /* division by zero */
    _VM_F_Memory,
    _VM_F_Control,
    _VM_F_Locking,
    _VM_F_Hypercall,
    _VM_F_NotImplemented,
    _VM_F_Last
};

enum _VM_FaultAction
{
    _VM_FA_Resume, _VM_FA_Abort
};

struct _VM_Env {
    const char *key;
    const char *value;
    int size;
};

#if defined( __divine__ ) || defined( DIVINE_NATIVE_RUNTIME )

EXTERN_C

void *__sys_init( struct _VM_Env *env );

/*
 * Set up the scheduler. After __sys_init returns, the VM will repeatedly call
 * the scheduler (every time with a blank frame). In the first invocation, the
 * return value of __sys_init is passed to the scheduler (along with its size),
 * while in each subsequent call, it gets the return value of its previous
 * invocation (again, along with its size). The scheduler can use __vm_jump to
 * invoke suspended threads of execution. Upon interrupt, the address of the
 * interrupted frame is optionally stored by the VM in a location set by a call
 * to __vm_set_ifl and the control is returned to the scheduler's frame
 * (i.e. if the scheduler invokes __vm_jump and the jumped-to function is then
 * interrupted, control returns to the instruction right after the __vm_jump).
 */
void __vm_set_sched( void *(*f)( int, void * ) ) NOTHROW NATIVE_VISIBLE;
void __vm_set_fault( enum _VM_FaultAction (*f)( enum _VM_Fault ) ) NOTHROW NATIVE_VISIBLE;

/*
 * When execution is interrupted, the VM will store the address of the
 * interrupted frame into *where (unless where is NULL). This address is reset
 * to NULL upon every new invocation of the scheduler.
 */
void __vm_set_ifl( struct _VM_Frame **where ) NOTHROW NATIVE_VISIBLE;

/*
 * Non-deterministic choice: when __vm_choose is encountered, the "universe" of
 * the program splits into n copies. Each copy of the "universe" will see a
 * different return value from __vm_choose, starting from 0.
 *
 * When more than one parameter is given, the choice becomes probabilistic: for
 * N choices, you need to provide N additional integers, giving probability
 * weight of a given alternative (numbering from 0). E.g.  `__vm_choose( 2, 1,
 * 9 )` will return 0 with probability 1/10 and 1 with probability 9/10.
 */
int __vm_choose( int n, ... ) NOTHROW NATIVE_VISIBLE;

/*
 * Transfer control to a given frame. The program counter of the current frame
 * is incremented *before* __vm_jump transfers control, so if the target frame
 * uses __vm_jump to go back to the first one, it'll continue execution right
 * after the call which caused the first __vm_jump. If `forgetful` is non-zero,
 * the jump will also atomically clear the mask flag (see `__vm_mask`) and
 * forget any visible effects that may have happened within the current atomic
 * section.
 */
void __vm_jump( struct _VM_Frame *dest, int forgetful ) NOTHROW NATIVE_VISIBLE;

/*
 * Cause a fault. The first argument is passed on to the fault handler in the
 * program. All remaining arguments are ignored, but can be examined by the
 * fault handler, since it will obtain a pointer to the call instruction which
 * invoked __vm_fault as its 'fault location' parameter.
 */
void __vm_fault( enum _VM_Fault t, ... ) NOTHROW NATIVE_VISIBLE;

/*
 * Setting the mask to 1 disables interrupts, i.e. the program executes
 * atomically until the mask is reset to 0. Returns the previous value of
 * mask.
 */
int __vm_mask( int ) NOTHROW NATIVE_VISIBLE;

/*
 * When interrupts are masked (cf. __vm_mask) but an interrupt is raised, a
 * flag is maintained by the VM and the call that clears the mask will cause an
 * interrupt if the flag is set. The __vm_interrupt hypercall can be used to
 * manipulate this flag. If interrupts are not masked, calling __vm_interrupt( 1 )
 * will cause an immediate interrupt. The previous value of the flag is returned.
 */
int __vm_interrupt( int ) NOTHROW NATIVE_VISIBLE;
void __vm_cfl_interrupt( void ) NOTHROW NATIVE_VISIBLE;
void __vm_mem_interrupt( void ) NOTHROW NATIVE_VISIBLE;

void __vm_trace( const char * ) NOTHROW NATIVE_VISIBLE;

/*
 * Create and destroy heap objects.
 */
void *__vm_make_object( int size ) NOTHROW NATIVE_VISIBLE;
void  __vm_free_object( void *ptr ) NOTHROW NATIVE_VISIBLE;

/*
 * Efficient, atomic memory copy. Should have the same effect as a loop-based
 * copy done under a mask, but much faster (especially much faster than
 * byte-based copying, due to much simpler pointer tracking code paths).
 */
void  __vm_memcpy( void *, void *, int ) NOTHROW;

/*
 * Variable argument handling. Calling __vm_query_varargs gives you a pointer
 * to a monolithic block of memory that contains all the varargs, successively
 * assigned higher addresses (going from left to right in the argument list).
 */
void *__vm_query_varargs( void ) NOTHROW NATIVE_VISIBLE;

/*
 * Get the address of the currently executing frame.
 */
void *__vm_query_frame( void ) NOTHROW NATIVE_VISIBLE;
int __vm_query_object_size( void * ) NOTHROW NATIVE_VISIBLE;

CPP_END

#endif // __divine__

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
#undef NATIVE_VISIBLE

#endif // __DIVINE_H__
