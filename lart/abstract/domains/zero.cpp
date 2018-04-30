// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>

namespace lart {
namespace abstract {

llvm::Value * Zero::process( llvm::Instruction *, Values & ) {
    NOT_IMPLEMENTED();
}

llvm::Value* Zero::lift( llvm::Value * ) {
    NOT_IMPLEMENTED();
}

llvm::Type* Zero::type( llvm::Module *, llvm::Type * ) const {
    NOT_IMPLEMENTED();
}

} // namespace abstract
} // namespace lart
