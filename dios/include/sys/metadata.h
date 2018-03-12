#ifndef _SYS_METADATA_H
#define _SYS_METADATA_H

#include <sys/divm.h>
#include <dios.h>
#include <stdint.h>
#include <sys/cdefs.h>

#define _ROOT __attribute__((__annotate__("divine.link.always")))

__BEGIN_DECLS

typedef struct
{
    /* LLVM instruction opcode (use divine/opcodes.h, enum OpCode to decode) */
    int opcode;
    /* suboperation for cmp, armw, and ivoke (in case of invoke it is the
     * offset of landing block from the beginning of the function (that is,
     * instruction index of landing block)) */
    int subop;
    /* offset of return value in frame, do not use directly, use
     * __md_get_register_info instead */
    int val_offset;
    /* width of return value in frame, do not use directly, use
     * __md_get_register_info instead */
    int val_width;
} _MD_InstInfo;

typedef struct
{
    char *name;
    void (*entry_point)( void );
    int frame_size;
    int arg_count;
    int is_variadic;
    char *type; /* similar to Itanium ABI mangle, first return type, then
                *  arguments, all pointers are under p, all integers except for
                *  char are set as unsigned, all chars are 'c' (char) */
    int inst_table_size;
    _MD_InstInfo *inst_table;
    void (*ehPersonality)( void );
    void *ehLSDA; /* language-specific exception handling tables */
    int is_nounwind;
    int is_trap;
} _MD_Function;

typedef struct
{
    void *start;
    int width;
} _MD_RegInfo;

typedef struct
{
    char *name;
    void *address;
    long size;
    int is_constant;
} _MD_Global;

/* Query function metadata by function name, this is the mangled name in case
 * of C++ or other languages which employ name mangling */
// const _MD_Function *__md_get_function_meta( const char *name ) NOTHROW _ROOT;

/* Query function metadata program counter value */
const _MD_Function *__md_get_pc_meta( _VM_CodePointer pc ) __nothrow _ROOT;

/* Given function frame, program counter and metadata extracts pointer to
 * register corresponding to instruction with given PC from the frame.
 *
 * If the PC is out of range, or any argument is invalid return { nullptr, 0 },
 * while for void values (registers of instructions which do no return) returns
 * { ptr-somewhere-to-the-frame, 0 }.
 *
 * Be aware that registers can be reused/overlapping if they do not interfere
 * or are guaranteed to have same value any time they are both defined in a
 * valid run.
 * Examples of safe use:
 * * writing landingpad's register before jumping to the instruction after the
 *   landingpad
 * * reading register of value of call/invoke
 * */
_MD_RegInfo __md_get_register_info( struct _VM_Frame *frame, _VM_CodePointer pc,
                                    const _MD_Function *funMeta ) __nothrow _ROOT;

const _MD_Global *__md_get_global_meta( const char *name ) __nothrow _ROOT;

__END_DECLS

#undef _ROOT

#endif
