// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/builder.h>
#include <lart/abstract/data.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Abstraction {
    using FunctionNodeDecl = std::pair< llvm::Function *, RootsSet >;
    struct FunctionNode : FunctionNodeDecl {
        using FunctionNodeDecl::FunctionNodeDecl;

        AbstractValues reached( const Fields & fields ) const;

        llvm::Function * function() const { return first; }
        const RootsSet& roots() const { return second; }
        RootsSet& roots() { return second; }
    };

    Abstraction( PassData & data ) : data( data ) {}

    void run( llvm::Module & );

    llvm::Function * process( const FunctionNode & node, const Fields & );
    FunctionNode clone( const FunctionNode & node, Fields & );

    PassData & data;

    using Signature = std::vector< size_t >;
    FunctionMap< llvm::Function *, Signature > fns;
};

} // namespace abstract
} // namespace lart
