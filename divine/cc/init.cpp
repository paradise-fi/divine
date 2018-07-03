#include <llvm/Support/TargetSelect.h>
#include <mutex> // call_once

namespace divine {
namespace cc {

static std::once_flag initTargetsFlags;

void initTargets() {
    std::call_once( initTargetsFlags, [] {
            llvm::InitializeAllTargetInfos();
            llvm::InitializeAllTargets();
            llvm::InitializeAllTargetMCs();
            llvm::InitializeAllAsmPrinters();
            llvm::InitializeAllAsmParsers();
        } );
}

} // cc
} // divine
