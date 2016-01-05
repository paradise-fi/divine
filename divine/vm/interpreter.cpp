#include <divine/llvm/execution.tcc>
#include <divine/llvm/interpreter.tcc>

namespace divine {
namespace llvm {

#ifdef TCC_INSTANCE
@template struct Interpreter< machine::NoHeapMeta, graph::NoLabel >;
@template struct Interpreter< machine::NoHeapMeta, graph::ControlLabel >;
@template struct Interpreter< machine::NoHeapMeta, graph::Probability >;
@template struct Interpreter< machine::HeapIDs, graph::NoLabel >;
#endif

}
}
