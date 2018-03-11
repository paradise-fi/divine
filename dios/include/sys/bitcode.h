#ifndef __DIVINE_BITCODE_H__
#define __DIVINE_BITCODE_H__

#include <sys/divm.h>

#ifdef __cplusplus
// Instruction.def are created by cmake as a copy of llvm/include/llvm/IR/Instruction.def
namespace OpCode {
enum OpCode {
#define HANDLE_INST(op,n,i)   n = op,
#include <divine/Instruction.def>
    OpLLVMEnd,
    OpHypercall, OpBB, OpArgs
};
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DiOS_FunAttrs {
    _VM_CodePointer personality;
    int is_nounwind;
    void *lsda;
} _DiOS_FunAttrs;

_VM_CodePointer __dios_get_function_entry( _VM_CodePointer pc );

_VM_CodePointer __dios_get_landing_block( _VM_CodePointer pc );

// find and instruction with the given opcode which is closest to given
// instruction in its block
// if no instruction is found, nullptr is returned
// this can be also used to search for block header
// direction can be either 1 to search forward, or -1 to search backward
_VM_CodePointer __dios_find_instruction_in_block( _VM_CodePointer pc_, int direction, unsigned opcode );

_DiOS_FunAttrs __dios_get_func_exception_attrs( _VM_CodePointer pc );

unsigned __dios_get_opcode( _VM_CodePointer pc );

#ifdef __cplusplus
} // extern "C"
#endif
#endif // __DIVINE_OPCODES_H__

