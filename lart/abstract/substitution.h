// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/common.h>
#include <unordered_map>

namespace lart {
namespace abstract {

struct Substitution {
    void run( llvm::Module& );
    llvm::Value* process( llvm::CallInst *i, Values &args );

    void init_domains( llvm::Module& );
    void add_domain( std::shared_ptr< Common > dom );
private:
    using Abstraction = std::shared_ptr< Common >;
    std::unordered_map< Domain, Abstraction > domains;
};


} // namespace abstract
} // namespace lart

