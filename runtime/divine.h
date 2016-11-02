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
    _VM_F_NoFault,
    _VM_F_Assert,
    _VM_F_Arithmetic, /* division by zero */
    _VM_F_Memory,
    _VM_F_Control,
    _VM_F_Locking,
    _VM_F_Hypercall,
    _VM_F_NotImplemented,
    _VM_F_Last
};

enum _VM_Trace
{
    _VM_T_Text,        /* takes one const char * parameter */
    _VM_T_SchedChoice, /* takes one void * parameter */
    _VM_T_SchedInfo,   /* takes two integer parameters */
    _VM_T_StateType,   /* takes one pointer parameter */
    _VM_T_Info         /* takes one const char * parameter - a line of YAML */
};

enum _VM_ControlAction
{
    _VM_CA_Set, /* set the register to next_argument */
    _VM_CA_Get, /* return the current value */
    _VM_CA_Bit  /* set or clear bits in a register */
};

/*
 * Setting the _VM_IF_Mask bit in the _VM_CR_Int register (see below) to 1 will
 * disallow interrupts; that is, the program will execute atomically until the
 * register is reset to 0.
 *
 * When interrupts are masked but an interrupt is raised, a the
 * _VM_IF_Interrupted bit is set in this register by the VM and the call that
 * clears the mask (but not the _VM_IF_Interrupted bit) will cause an
 * interrupt.
 *
 * When the scheduler returns, the value of the _VM_CF_Accepting and
 * _VM_CF_Error bits indicate whether the edge in the state space should be
 * treated as erroneous or accepting. The _VM_CF_Cancel flag indicates, on the
 * other hand, that it is impossible to proceed (eg. there are no runnable
 * processes).
 */
enum _VM_ControlFlags
{
    _VM_CF_None        = 0,
    _VM_CF_Mask        = 0b000001,
    _VM_CF_Interrupted = 0b000010,
    _VM_CF_Accepting   = 0b000100,
    _VM_CF_Error       = 0b001000,
    _VM_CF_Cancel      = 0b010000,
    _VM_CF_KernelMode  = 0b100000,
    _VM_CF__64bit__    = 1ull << 33
};

/*
 * The VM maintains 8 control registers and 2 registers available for OS use
 * (one integer and one pointer). The __vm_control hypercall can be used to get
 * and set register values (as applicable). Bitwise changes to the _VM_CR_Flags
 * register are also possible, using the _VM_CA_Bit action, providing a mask
 * (which bits should be affected) and a the values to set those bits to.
 */
enum _VM_ControlRegister
{
    _VM_CR_Constants,        /* read-only,  pointer */
    _VM_CR_Globals,          /* read-write, pointer */
    _VM_CR_Frame,            /* read-write, pointer */
    _VM_CR_PC,               /* read-write, code pointer */

    _VM_CR_Scheduler,        /* write-once, function pointer */
    _VM_CR_State,            /* write-once, pointer */
    _VM_CR_IntFrame,         /* read-only,  pointer */
    _VM_CR_Flags,            /* read-write, _VM_Flags */

    _VM_CR_FaultHandler,     /* write-once, function pointer */
    _VM_CR_User1,
    _VM_CR_User2,

    _VM_CR_Last
};

/* jumping out of the scheduler:
   __vm_control( _VM_CA_Set, _VM_CR_Frame, destination,
                 _VM_CA_Set, _VM_CR_Int, 0 );
   jumping in the unwinder:
   __vm_control( _VM_CA_Set, _VM_CR_Frame, destination,
                 _VM_CA_Set, _VM_CR_PC, dest_pc );
   masking interrupts:
   int oldint = __vm_control( _VM_CA_Get, _VM_CR_Int,
                              _VM_CA_Bit, _VM_CR_Int, _VM_IF_Mask, _VM_IF_Mask );
   ...
   __vm_control( _VM_CA_Bit, _VM_CR_Int, _VM_IF_Mask, oldint );
*/

struct _VM_Env
{
    const char *key;
    const char *value;
    int size;
};

enum _VM_MemAccessType { _VM_MAT_Load = 0x1, _VM_MAT_Store = 0x2, _VM_MAT_Both = 0x3 };

#if defined( __divine__ ) || defined( DIVINE_NATIVE_RUNTIME )

EXTERN_C

/*
 * The main task of the __boot procedure is to set up the scheduler. After
 * __boot returns, the VM will repeatedly call the scheduler (every time with a
 * blank frame). The scheduler may use __vm_control to invoke suspended threads
 * of execution. Upon interrupt, the control is returned to the scheduler's
 * frame (i.e. if the scheduler invokes __vm_control to transfer to the user
 * program and the jumped-to function is then interrupted, control returns to
 * the instruction right after the call to __vm_control).
 *
 * Additionally, __boot can (and probably should) set up a fault handler, using
 * __vm_control( _VM_CR_FaultHandler | _VM_CR_Set, &handler_function ). See
 * also below.
 */

void __boot( const struct _VM_Env *env )
    __attribute__((__annotate__("brick.llvm.prune.root")));

/*
 * The type of the fault handler. The 'type' parameter exists ot make it easier
 * for the handler to quickly decide whether to continue execution and whether
 * to raise an user-reported error. In case the handler decides to continue
 * execution, it should transfer control to the cont_frame at the cont_pc
 * location:
 * __vm_control( _VM_CA_Set, _VM_CR_Frame, cont_frame,
 *               _VM_CA_Set, _VM_CR_PC, cont_pc );
 */
typedef void (*__vm_fault_t)( enum _VM_Fault type,
                              struct _VM_Frame *cont_frame,
                              void (*cont_pc)(void), ... )
    __attribute__((__noreturn__));

typedef void (*__vm_sched_t)( void *state );

/*
 * Access the control registers of the VM. See the definition of
 * _VM_ControlRegister above for details on the registers themselves. It is
 * possible to pass multiple requests at once, making it posible to control
 * transfer (by manipulating _VM_CR_PC and/or _VM_CR_Frame) and set other
 * register values atomically (eg. clear the interrupt flag or unmask
 * interrupts, or to also transfer control within the target frame).
 */
void *__vm_control( enum _VM_ControlAction, ... ) NOTHROW NATIVE_VISIBLE;

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
 * Cause a fault. The first argument is passed on to the fault handler in the
 * program. All remaining arguments are ignored, but can be examined by the
 * fault handler, since it will be able to obtain a pointer to the call
 * instruction which invoked __vm_fault by reading the program counter of its
 * parent frame.
 */
void __vm_fault( enum _VM_Fault t, ... ) NOTHROW NATIVE_VISIBLE;

void __vm_interrupt_mem( void ) NOTHROW NATIVE_VISIBLE;
void __vm_interrupt_cfl( void *, _VM_MemAccessType ) NOTHROW NATIVE_VISIBLE;

/*
 * Provides meta-information about the executing code. The 'type' parameter
 * distinguishes between multiple categories of said meta-information. How the
 * trace info is used or presented to the user depends on the particular
 * frontend: text traces may be printed out as the program executes, or they
 * may show up as arrow labels in visual state space representation. The
 * scheduling trace data can be used for annotating counterexamples, for
 * guiding heuristic searches (minimising the number of context switches, for
 * example) or in an interactive simulator/debugger for constructing a
 * high-level view of thread/process structure of the program.
 */
void __vm_trace( enum _VM_Trace type, ... ) NOTHROW NATIVE_VISIBLE;

/*
 * Create and destroy heap objects.
 */
void *__vm_obj_make( int size ) NOTHROW NATIVE_VISIBLE;
void  __vm_obj_resize( void *ptr, int size ) NOTHROW NATIVE_VISIBLE;
void  __vm_obj_free( void *ptr ) NOTHROW NATIVE_VISIBLE;
int   __vm_obj_size( void * ) NOTHROW NATIVE_VISIBLE;

CPP_END

#endif // __divine__

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
#undef NATIVE_VISIBLE

#endif // __DIVINE_H__
