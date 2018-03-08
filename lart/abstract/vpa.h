// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <lart/abstract/value.h>
#include <lart/abstract/util.h>
#include <lart/abstract/fieldtrie.h>
namespace lart {
namespace abstract {

struct VPA {
    void run( llvm::Module & m );
private:
    void preprocess( llvm::Function * );
    void propagate_root( llvm::Value* );

    using Task = std::function< void() >;
    std::deque< Task > tasks;

    std::set< llvm::Value * > seen_vals;
    std::set< llvm::Function * > seen_funs;
};


} // namespace abstract
} // namespace lart
