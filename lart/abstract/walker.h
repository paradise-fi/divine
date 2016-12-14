// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>

#include <lart/abstract/annotation.h>

namespace lart {
namespace abstract {

struct AbstractWalker {

    AbstractWalker() {}
    AbstractWalker( llvm::Module & m );

    std::vector< llvm::Function * > functions();

    std::vector< llvm::Value * > entries( llvm::Function * );

    std::vector< llvm::Value * > dependencies( llvm::Value * );

private:
    std::vector< Annotation > annotations;
    llvm::Module * _m = nullptr;
};

} // namespace abstract
} // namespace lart
