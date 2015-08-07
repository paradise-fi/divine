#if GEN_LLVM
#include <llvm/Support/Threading.h>
#endif

#include <brick-assert.h>

namespace divine {
namespace llvm {

bool initMultithreaded() {
#if GEN_LLVM
    return ::llvm::llvm_is_multithreaded();
#else
    ASSERT_UNREACHABLE( "LLVM not available" );
#endif
}

}
}

