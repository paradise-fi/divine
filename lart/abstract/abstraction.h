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
    Abstraction( PassData & data ) : data( data ) {}

    void run( llvm::Module & );

    llvm::Function * process( const FunctionNodePtr & node );
    void clone( const FunctionNodePtr & node );

    void clean( Values & deps );

    PassData & data;
    FunctionMap< llvm::Function *, Types > fns;
};


} // namespace abstract
} // namespace lart
