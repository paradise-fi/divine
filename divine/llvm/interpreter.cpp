#include <divine/llvm/execution.tcc>
#include <divine/llvm/interpreter.tcc>

namespace divine {
namespace llvm {

#ifdef TCC_INSTANCE
@template struct Interpreter< machine::NoHeapMeta >;
@template struct Interpreter< machine::HeapIDs >;
#endif

}
}
