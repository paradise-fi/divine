// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/zero.h>

namespace lart {
namespace abstract {

llvm::Value * Zero::process( llvm::CallInst *, Values & ) {
    NOT_IMPLEMENTED();
}

} // namespace abstract
} // namespace lart
