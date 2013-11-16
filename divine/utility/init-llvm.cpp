#ifdef O_LLVM
#include <llvm/Support/Threading.h>
#endif

#include <wibble/test.h>

namespace divine {
namespace llvm {

bool initMultithreaded() {
#ifdef O_LLVM
    return ::llvm::llvm_is_multithreaded() || ::llvm::llvm_start_multithreaded();
#else
    assert_unreachable( "LLVM not available" );
#endif
}

}
}

