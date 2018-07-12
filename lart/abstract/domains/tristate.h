// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct Tristate final : Common {
    llvm::Value* process( llvm::Instruction *i, Values &args ) override;
    Domain domain() const override { return Domain::Tristate(); }
    llvm::Value* lift( llvm::Value *v ) override;
    llvm::Type* type( llvm::Module *m, llvm::Type *type ) const override;
    llvm::Value* default_value( llvm::Type *type ) const override;
};

} // namespace abstract
} // namespace lart
