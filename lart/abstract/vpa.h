// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <deque>
#include <set>
#include <unordered_set>

#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

struct VPA {
    void run( llvm::Module& );
private:
    void preprocess( llvm::Function* );
    void propagate_value( llvm::Value*, Domain );
    void propagate_back( llvm::Argument*, Domain );

    void step_out( llvm::Function*, Domain );

    using Task = std::function< void() >;
    std::deque< Task > tasks;

    std::set< std::pair< llvm::Value*, Domain > > seen_vals;
    std::set< std::pair< llvm::Value*, Domain > > entry_args;
    std::unordered_set< llvm::Function* > seen_funs;
};


} // namespace abstract
} // namespace lart
