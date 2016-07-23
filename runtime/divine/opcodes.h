#ifndef __DIVINE_OPCODES_H__
#define __DIVINE_OPCODES_H__
// Instruction.def are created by cmake as a copy of llvm/include/llvm/IR/Instruction.def
namespace OpCode {
enum OpCode {
#define HANDLE_INST(op,n,i)   n = op,
#include <divine/Instruction.def>
};
}
#endif // __DIVINE_OPCODES_H__
