// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct AddAssumes {
	void run( llvm::Module &m );

    void process( llvm::BranchInst *i );
};

} // namespace abstract
} // namespace lart
