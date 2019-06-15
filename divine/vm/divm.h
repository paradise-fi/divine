// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* The values of constants defined in this file are used in the VM
 * implementation itself -- hence the funny include guards. */

#if !defined(__DIVM_H_CONST__) && ( !defined(__divm__) || defined(__divm_const__) )
#define __DIVM_H_CONST__
#include <stdint.h>

/* Constants related to pointer layout and object types. The _VM_PM_* are
 * simply masks for the 2 parts of a pointer (object identifier and an offset).
 * The _VM_PB_* are number of bits. */

enum { _VM_PM_Off  = 0x00000000FFFFFFFFull,
       _VM_PM_Obj  = 0xFFFFFFFF00000000ull };
enum { _VM_PB_Full = 64,
       _VM_PB_Obj = 32,
       _VM_PB_Off  = _VM_PB_Full - _VM_PB_Obj };

/* The _VM_PT_* classify objects (and hence pointers to those objects) into a
 * number of classes. Global pointers are addresses of global variables, code
 * pointers are addresses of functions and labels, alloca pointers are
 * addresses of local variables, heap pointers are for heap allocations (this
 * includes both `malloc` memory and function frames and similar VM-allocated
 * objects), marked objects are like heap, but are treated specially when
 * states are compared in the model checker (see `divine/mc/hasher.hpp`).
 * Finally, weak objects don't enter state comparison at all -- any two weak
 * objects are considered equal. */

enum _VM_PointerType { _VM_PT_Global, _VM_PT_Code, _VM_PT_Alloca, _VM_PT_Heap,
                       _VM_PT_Marked, _VM_PT_Weak };

/* The VM keeps certain metadata about all allocated memory: whether a
 * particular address stores an object ID, whether a particular byte is defined
 * (initialized) or whether it is tainted. Additionally, the userspace can
 * attach metadata to memory locations and the VM will keep track of it. The
 * interface to access and possibly modify the metadata is via `__vm_peek` and
 * `__vm_poke`, which both take the metadata layer to operate on as one of its
 * parameters. The available layers are listed in the _VM_MemLayer enum below.
 * There may be more than one user-defined metadata layer, in which case they
 * are numbered sequentially starting from _VM_ML_User. */

enum _VM_MemLayer { _VM_ML_Pointers, _VM_ML_Definedness, _VM_ML_Taints, _VM_ML_User };

/* The object identifier part of the pointer is a 32 bit number, but each
 * object type has a range of values from which the VM allocates object
 * identifiers for the given object types. The specific ranges are described
 * here. The _VM_PL* constants are the upper limit of each range (but the
 * ranges are not guaranteed to be in any particular order and may be
 * rearranged in a later version); a more convenient way to obtain the limits
 * is via the __vm_pointer_limits array below. */

enum { _VM_PL_Global = 0x00080000,
       _VM_PL_Code   = 0x00100000,
       _VM_PL_Alloca = 0x10000000,
       _VM_PL_Heap   = 0xF0000000,
       _VM_PL_Marked = 0xF7000000 };

/* Each entry of __vm_pointer_limits describes the lower and upper bound of
 * object identifier values that correspond to the given object type. The
 * interval is closed from left but open at the end (like C++ iterators). */

struct _VM_PointerLimits { uint32_t low, high; };
static const struct _VM_PointerLimits __vm_pointer_limits[] =
{
    [_VM_PT_Global] = { 0,             _VM_PL_Global },
    [_VM_PT_Code]   = { _VM_PL_Global, _VM_PL_Code   },
    [_VM_PT_Alloca] = { _VM_PL_Code,   _VM_PL_Alloca },
    [_VM_PT_Heap]   = { _VM_PL_Alloca, _VM_PL_Heap   },
    [_VM_PT_Marked] = { _VM_PL_Heap,   _VM_PL_Marked },
    [_VM_PT_Weak]   = { _VM_PL_Marked, 0xFFFFFFFF    }
};

/* Obtain the type of object associated to a given object identifier (an
 * efficient implementation depends on the range ordering, hence it's part of
 * this file and not part of dios). */

static inline int __vm_pointer_type( uint32_t objid )
{
    if ( objid < _VM_PL_Global )
        return _VM_PT_Global;
    if ( objid < _VM_PL_Code )
        return _VM_PT_Code;
    if ( objid < _VM_PL_Alloca )
        return _VM_PT_Alloca;
    if ( objid < _VM_PL_Heap )
        return _VM_PT_Heap;
    if ( objid < _VM_PL_Marked )
        return _VM_PT_Marked;
    if ( 1 )
        return _VM_PT_Weak;
}

#endif

/* Definitions of data structures and hypercalls follow. The __divm__ macro is
 * defined when DiVM itself is being built, while __divine__ is defined when
 * `divine cc` or `divcc` is compiling code to be executed in DiVM. */

#if !defined(__DIVM_H__) && !defined(__divm_const__)
#define __DIVM_H__

#ifdef __divm__ /* #included in divine/vm */
#include <divine/vm/pointer.hpp>
#endif

#include <stdint.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define EXTERN_END }
#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif
#else
#define EXTERN_C
#define EXTERN_END
#define NOTHROW __attribute__((__nothrow__))
#endif

#ifndef _VMUTIL_INLINE
#define _VMUTIL_INLINE static inline __attribute__((always_inline))
#define _VMUTIL_INLINE_UNDEF
#endif

#define NATIVE_VISIBLE

#ifndef __divm__
typedef void (*_VM_CodePointer)( void );
typedef int (*_VM_Personality)( void *, void *, void * ); // ?
typedef void *_VM_Pointer;
#else
typedef divine::vm::CodePointer _VM_CodePointer;
typedef divine::vm::CodePointer _VM_Personality;
typedef divine::vm::GenericPointer _VM_Pointer;
#endif

/* Instruction and operand encoding. The VM currently uses a Harvard
 * architecture -- instructions are not visible to the executing program. This
 * might change in the future. */

struct _VM_Instruction
{
    uint32_t is_instruction:1; /* always 1 */
    uint32_t opcode:8;
    uint32_t subcode:14;
    uint32_t valuecount:6;
    uint32_t extcount:2;
    uint32_t immediate:1; /* second argument is an immediate constant */
#if __cplusplus >= 201103L
    explicit _VM_Instruction( int opc = 0, int subc = 0, int valc = 0, int extc = 0 )
        : is_instruction( 1 ), opcode( opc ), subcode( subc ),
          valuecount( valc ), extcount( extc ), immediate( 0 ) {}
#endif
};

struct _VM_Operand
{
    uint32_t is_instruction:1; /* always 0 */
    enum Type { I1, I8, I16, I32, I64, I128, IX,
                F32, F64, F80, Ptr, PtrC, PtrA, Agg, Void, Other } type:4;
    /* NB. The numeric value of Location has to agree with the
           corresponding register's index in the _VM_ControlRegister enum */
    enum Location { Const, Global, Local, Invalid } location:3;
    uint32_t offset:24;
#if __cplusplus >= 201103L
    _VM_Operand() : is_instruction( 0 ), type( Void ), location( Invalid ), offset( 0 ) {}
#endif
};

struct _VM_OperandExt
{
    uint32_t is_instruction:1; /* always 0 */
    uint32_t width:24;
    uint32_t unused:7;
#if __cplusplus >= 201103L
    _VM_OperandExt() : is_instruction( 0 ), width( 0 ), unused( 0 ) {}
#endif
};

/* Representation of a function. Not (yet) exposed, same as above. */

struct _VM_Function
{
    /* where the code for the function lives, relative to start of the heap
       object holding the RR */
    uint32_t code_offset, code_size, frame_size;
    uint32_t argcount:31;
    uint32_t vararg:1;
    struct _VM_Operand result;
    struct _VM_OperandExt result_ext;
    /* arguments are encoded as OpArg instructions at the start of the first
     * basic block of the function */
    _VM_Personality personality;
    _VM_Pointer lsda;
};

/* Encoding of types (operands, constants, ...) in the form they are used in
 * the VM. Mainly useful for computing GEP (GetElementPtr) offsets. Not exposed
 * either. */

struct _VM_Type
{
    enum { Array, Struct, Scalar } type:2;
    unsigned items:30;
    unsigned size;
};

struct _VM_TypeItem
{
    unsigned offset;
    int type_id;
};

union _VM_TypeTable
{
    struct _VM_Type type;
    struct _VM_TypeItem item;
};

/* The header of a function activation frame. The stack in DiVM is a linked
 * list of such frames. The 'pc' holds the current instruction pointer (updted
 * at the discretion of the VM, but at least when a call or an invoke
 * instruction is executed, creating a new frame). The 'parent' pointer is a
 * pointer to the frame of the caller. The remainder of the frame contains
 * function arguments and values of local LLVM registers. */

struct _VM_Frame
{
    _VM_CodePointer pc;
    struct _VM_Frame *parent;
};

/* Fault classes generated by the VM. To be interpreted by the fault handler.
 * The OS may define further classes for its own use. The VM does not read or
 * otherwise recognize values of this type, it only emits them. */

enum _VM_Fault
{
    _VM_F_NoFault,
    _VM_F_Assert,
    _VM_F_Integer, /* division by zero */
    _VM_F_Float,   /* division by zero */
    _VM_F_Memory,
    _VM_F_Leak,
    _VM_F_Control,
    _VM_F_Locking,
    _VM_F_Hypercall,
    _VM_F_Access, /* access violation */
    _VM_F_NotImplemented,
    _VM_F_Last
};

/* XXX. Trace types. See also __vm_trace below. */

enum _VM_Trace
{
    _VM_T_Text,        /* ( const char *text ) */
    _VM_T_Fault,       /* ( const char *text ) */
    _VM_T_SchedChoice, /* ( void *info ) */
    _VM_T_SchedInfo,   /* ( int pid, int tid ) */
    _VM_T_TaskID,      /* ( void *id ) */
    _VM_T_StateType,   /* ( void *state ) */
    _VM_T_Info,        /* ( const char *yaml ) */
    _VM_T_Assume,      /* ( weak void *path_condition ) */
    _VM_T_LeakCheck,   /* () */
    _VM_T_TypeAlias,   /* ( void *, const char * ): create a type alias */
    _VM_T_DebugPersist /* ( void **, weak void * ) */
};

/* XXX. Flags stored in _VM_CR_Flags, set/cleared by __vm_ctl_flag(). */

static const uint64_t _VM_CF_None        = 0;
static const uint64_t _VM_CF_IgnoreLoop  = 0b1;
static const uint64_t _VM_CF_IgnoreCrit  = 0b10;
static const uint64_t _VM_CF_Accepting   = 0b100;
static const uint64_t _VM_CF_Error       = 0b1000;
static const uint64_t _VM_CF_Cancel      = 0b10000;
static const uint64_t _VM_CF_KernelMode  = 0b100000;
static const uint64_t _VM_CF_DebugMode   = 0b1000000;
static const uint64_t _VM_CF_AutoSuspend = 0b10000000;
static const uint64_t _VM_CF_KeepFrame   = 0b100000000;
static const uint64_t _VM_CF_Booting     = 0b1000000000;
static const uint64_t _VM_CF_Stop        = 0b10000000000;

/* Control flags are split up into 4 blocks of 16 bits, each reserved for a
 * different area of use. The first block is reserved for VM use (see above),
 * the second block is for operating system use, third one for bitcode
 * transformations that require runtime support (such as relaxed memory) and
 * the final block is reserved for use by model checking algorithms. */

static const uint64_t _VM_CFB_VM        = 1ull <<  0;
static const uint64_t _VM_CFB_OS        = 1ull << 16;
static const uint64_t _VM_CFB_Transform = 1ull << 32;
static const uint64_t _VM_CFB_Alg       = 1ull << 48;

/* The VM maintains a number of control registers (out of which, 4 are
 * available for OS use) The __vm_ctl_* family of hypercalls can be used to get
 * and set register values (as applicable). Bitwise changes to the _VM_CR_Flags
 * register are also possible, using the __vm_ctl_flag call. */

enum _VM_ControlRegister
{
    _VM_CR_Constants,        /* read-only,  pointer */
    _VM_CR_Globals,          /* read-write, pointer */
    _VM_CR_Frame,            /* read-write, pointer */

    _VM_CR_User1,
    _VM_CR_User2,
    _VM_CR_User3,
    _VM_CR_User4, /* TODO remove */

    _VM_CR_Flags,            /* read-write, _VM_Flags */
    _VM_CR_ObjIdShuffle,

    _VM_CR_PersistLast,

    _VM_CR_PC = _VM_CR_PersistLast, /* read-write, code pointer */
    _VM_CR_Scheduler,        /* write-once, function pointer */
    _VM_CR_State,            /* write-once, pointer */
    _VM_CR_FaultHandler,     /* write-once, function pointer */

    _VM_CR_Last
};

static const int _VM_CR_PtrCount = 3;
static const int _VM_CR_UserCount = 4;

/* The data structure which holds the initial environment passed to __boot. The
 * keys are null-terminated ASCII strings, while the values might contain
 * binary data, hence their size is specified in the structure. The environment
 * is passed as an array of _VM_Env, terminated by an item with a null key. */

struct _VM_Env
{
    const char *key;
    const char *value;
    int size;
};

enum _VM_MemAccessType { _VM_MAT_Load = 0x1, _VM_MAT_Store = 0x2, _VM_MAT_Both = 0x3 };

enum _VM_SC_ValueType
{
    _VM_SC_Int32 = 0, _VM_SC_Int64 = 1, _VM_SC_Mem = 2,
    _VM_SC_In = 0x100, _VM_SC_Out = 0x200
};

#if defined( __divine__ ) || defined( DIVINE_NATIVE_RUNTIME )

EXTERN_C

/* The main task of the __boot procedure is to set up the scheduler. After
 * __boot returns, the VM will repeatedly call the scheduler (every time with a
 * blank frame). The scheduler may use __vm_ctl_set to invoke suspended threads
 * of execution. After a __vm_suspend(), the scheduler is started again.
 *
 * Additionally, __boot can (and probably should) set up a fault handler, using
 * `__vm_cfl_set( _VM_CR_FaultHandler, &handler_function )`. See also below. */

void __boot( const struct _VM_Env *env )
    __attribute__((__annotate__("brick.llvm.prune.root")));

/* The type of the fault handler. The 'type' parameter exists to make it easier
 * for the handler to quickly decide whether to continue execution and whether
 * to raise a user-reported error. In case the handler decides to continue
 * execution, it should transfer control to the cont_frame at the cont_pc
 * location: __vm_ctl_set( _VM_CR_Frame, cont_frame, cont_pc ) */

typedef void (*__vm_fault_t)( enum _VM_Fault type,
                              struct _VM_Frame *cont_frame,
                              void (*cont_pc)(void) )
    __attribute__((__noreturn__));

/* Read and write the control registers of the VM. See the definition of
 * _VM_ControlRegister above for details on the registers themselves. The
 * __vm_ctl_flag call clears and sets individual flags in the _VM_CR_Flags
 * register and returns the value of the flags register as it was before the
 * changes were performed. Depending on the particular register, __vm_ctl_set
 * might have side-effects (specifically, setting _VM_CR_Frame transfers
 * control to the specified frame; in this case, a third argument might be
 * given to __vm_ctl_set, which specifies the value of the program counter to
 * jump to within the function associated with the target frame). */

void     __vm_ctl_set( enum _VM_ControlRegister reg, void *val, ... ) NOTHROW;
void    *__vm_ctl_get( enum _VM_ControlRegister reg ) NOTHROW;
uint64_t __vm_ctl_flag( uint64_t clear, uint64_t set ) NOTHROW;

/* Conditionally call a function on criteria tracked by the VM related to state
 * space generation. An effect of both functions is to make a note of the call.
 * In case of `__vm_test_crit`, if a conflicting (critical) memory operation
 * was already tested on this edge, the condition is satisfied and the function
 * 'yes' is called. In the case of `__vm_test_loop`, if the same call is
 * performed a second time within a single state space edge and with the same
 * call stack and the same 'counter' argument, the function is called. In both
 * cases, the function is expected to cause a reschedule and call
 * `__vm_suspend` to restart the scheduler. */

void __vm_test_crit( void *addr, int size, enum _VM_MemAccessType access,
                     void (*yes)( void ) ) NOTHROW;
void __vm_test_loop( int counter, void (*yes)( void ) ) NOTHROW;

/* Non-deterministic choice: when __vm_choose is encountered, the 'universe' of
 * the program splits into n copies (n is the first argument of __vm_choose).
 * In each such 'universe', the program will see a different return value from
 * __vm_choose, starting from 0. The remaining arguments specify the
 * probability of each outcome, but no model checking algorithm can currently
 * use those values. */

int __vm_choose( int n, ... ) NOTHROW;

/* Provides meta-information about the executing code. The 'type' parameter
 * distinguishes between multiple categories of said meta-information. How the
 * trace info is used or presented to the user depends on the particular
 * frontend: text traces may be printed out as the program executes, or they
 * may show up as arrow labels in visual state space representation. The
 * scheduling trace data can be used for annotating counterexamples, for
 * guiding heuristic searches (minimising the number of context switches, for
 * example) or in an interactive simulator/debugger for constructing a
 * high-level view of thread/process structure of the program. The details of
 * individual trace types are documented at the _VM_Trace enum. */

void __vm_trace( enum _VM_Trace type, ... ) NOTHROW;

/*
 * Create and destroy heap objects.
 */
void *__vm_obj_make( int size, int type ) NOTHROW;
void  __vm_obj_resize( void *ptr, int size ) NOTHROW;
void  __vm_obj_free( void *ptr ) NOTHROW;
int   __vm_obj_size( const void * ) NOTHROW;
void *__vm_obj_clone( const void *root, const void **block ) NOTHROW;

/* Read and write additional metadata, indexed by an address and a key. The
 * metadata is only valid as long as the corresponding address is. There are a
 * few reserved keys that allow access to metadata automatically tracked by the
 * VM (see _VM_MemLayer above). User-specified metadata (i.e. attached to
 * _VM_ML_User and higher-valued keys) does not propagate automatically. */

uint32_t __vm_peek( void *addr, int key ) NOTHROW;
void     __vm_poke( void *addr, int key, uint32_t value ) NOTHROW;

/* Pass a syscall through the VM to the host system. The parameters must
 * describe the parameters of the actual operating system on the outside,
 * corresponding to the syscall with a given id.
 *
 * For example:
 *
 *   const char file[] = "filename.c";
 *   int fileno;
 *   int errno = __vm_syscall( SYS_open, _VM_SC_Int32, &fileno,
 *                                       _VM_SC_Mem | _VM_SC_In,
 *                                       sizeof( filename ), filename );
 *
 *   long rl_ret;
 *   char buffer[100];
 *   errno = __vm_syscall( SYS_readlink, _VM_SC_Int64, &rl_ret,
 *                                       _VM_SC_Mem | _VM_SC_In,
 *                                       sizeof( filename ), filename,
 *                                       _VM_SC_Mem | _VM_SC_Out,
 *                                       100, buffer,
 *                                       _VM_SC_Int64 | _VM_SC_In,
 *                                       100 );
 *   int close_err;
 *   errno = __vm_syscall( SYS_close, _VM_SC_Int32, &close_err,
 *                                    _VM_SC_Int32 | _VM_SC_In, fileno ); */

int __vm_syscall( int id, int retval_type, ... ) NOTHROW;

/* Suspend execution of the VM. This will cause the scheduler to be called again. */

_VMUTIL_INLINE void __vm_suspend( void ) NOTHROW
{
    __vm_ctl_set( _VM_CR_Frame, 0 );
}

/* Abandon the current execution. Everything since the last invocation of the
 * scheduler is thrown away and not considered part of the state space of the
 * program. Can be used to implement assume() and similar constructs. */

_VMUTIL_INLINE void __vm_cancel( void ) NOTHROW
{
    __vm_ctl_flag( 0, _VM_CF_Cancel );
    __vm_ctl_set( _VM_CR_Frame, 0 );
}

EXTERN_END

#endif // __divine__

#undef EXTERN_C
#undef EXTERN_END
#undef NOTHROW
#undef NATIVE_VISIBLE

#ifdef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE_UNDEF
#undef _VMUTIL_INLINE
#endif

#endif // __DIVINE_H__
