// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>

#include <iostream>

namespace lart {
namespace abstract {

struct MDBuilder {
    MDBuilder( llvm::LLVMContext &ctx )
        : ctx( ctx )
    {}

    llvm::MDNode* domain_node( Domain dom );
private:
    llvm::LLVMContext &ctx;
};


struct CreateAbstractMetadata {
    static void run( llvm::Module &m );
};

struct MDValue {
    MDValue( llvm::Metadata * md )
        : _md( llvm::cast< llvm::ValueAsMetadata >( md ) )
    {}

    std::string name() const;
    std::vector< Domain > domains() const;
    Domain domain() const; // requires that value has single domain
private:
    llvm::ValueAsMetadata  *_md;
};

void dump_abstract_metadata( const llvm::Module &m );

} // namespace abstract
} // namespace lart
