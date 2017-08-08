// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/value.h>
#include <lart/abstract/types/common.h>

#include <unordered_set>

namespace lart {
namespace abstract {


struct FunctionNode;

using FunctionNodePtr = std::shared_ptr< FunctionNode >;
using FunctionNodes = std::vector< FunctionNodePtr >;


struct Walker {
    using Preprocessor = std::function< void( llvm::Function * ) >;

    Walker( llvm::Module & m, Preprocessor p )
        : preprocess( p ) { init( m ); }

    std::vector< FunctionNodePtr > postorder() const;

private:
    void init( llvm::Module & m );

    // returns functions with packed annotated values
    std::vector< FunctionNodePtr > annotations( llvm::Module & m );

    // returns dependent functions
    std::vector< FunctionNodePtr > process( FunctionNodePtr & processed );

    // returns true if propagation crearted new entry
    bool propagateThroughCalls( FunctionNodePtr & processed );

    // returns dependent function nodes
    std::vector< FunctionNodePtr > dependencies( const FunctionNodePtr & processed );

    // returns calls of 'processed' from other nodes
    std::vector< FunctionNodePtr > callsOf( const FunctionNodePtr & processed );

    std::vector< FunctionNodePtr > functions;
    std::unordered_set< FunctionNodePtr > seen;
    Preprocessor preprocess;
};


struct FunctionNode {
    FunctionNode() = default;
    FunctionNode( const FunctionNode & ) = default;
    FunctionNode( FunctionNode && ) = default;

    FunctionNode( llvm::Function * fn,
                  const std::unordered_set< AbstractValue > & origins,
                  FunctionNodes succs = {} )
        : function( fn ), origins( origins ), succs( succs )
    {}

    FunctionNode &operator=( FunctionNode && other ) = default;

    std::vector< AbstractValue > postorder() const;

    llvm::Function * function;
    std::unordered_set< AbstractValue > origins;
    FunctionNodes succs;
};

inline bool operator==( const FunctionNode& l, const FunctionNode& r ) {
    return l.function == r.function
        && l.origins  == r.origins;
}
} // namespace abstract
} // namespace lart
