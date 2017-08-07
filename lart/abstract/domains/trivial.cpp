// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/trivial.h>

namespace lart {
namespace abstract {

llvm::Value * Trivial::process( llvm::CallInst *, std::vector< llvm::Value * > & ) {
    return nullptr;
}

bool Trivial::is( llvm::Type * ) {
    //TODO
    return true;
}

llvm::Type * Trivial::abstract( llvm::Type * ) { return nullptr; }

} // namespace abstract
} // namespace lart
