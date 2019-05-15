DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS
#include <lart/support/meta.h>

namespace lart::divine
{

struct DropEmptyDecls {
    static PassMeta meta();
    void run( llvm::Module &m );
};

} // namespace lart::divine
