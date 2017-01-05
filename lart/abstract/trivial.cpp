// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/trivial.h>

namespace lart {
namespace abstract {

void Trivial::process( llvm::CallInst *, std::map< llvm::Value *, llvm::Value * > & ) {}

bool Trivial::is( llvm::Type * ) {
    //TODO
    return true;
}

llvm::Type * Trivial::abstract( llvm::Type * ) { return nullptr; }

} // namespace abstract
} // namespace lart
