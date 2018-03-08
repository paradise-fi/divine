// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/vpa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/abstract/metadata.h>

namespace lart {
namespace abstract {

namespace {

bool valid_root_metadata( const MDValue &mdv ) {
    auto ty = mdv.value()->getType();
    return ( ty->isIntegerTy() ) ||
           ( ty->isPointerTy() && ty->getPointerElementType()->isIntegerTy() );
}

} // anonymous namespace

void VPA::preprocess( llvm::Function * fn ) {
    if ( !seen_funs.count( fn ) ) {
        auto lsi = std::unique_ptr< llvm::FunctionPass >( llvm::createLowerSwitchPass() );
        lsi.get()->runOnFunction( *fn );

        LowerSelect().runOnFunction( *fn );
        llvm::UnifyFunctionExitNodes().runOnFunction( *fn );

        seen_funs.insert( fn );
    }
}

void VPA::propagate_root( llvm::Value* ) {
    // propagates from value down
}

void VPA::run( llvm::Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) ) {
        if ( !valid_root_metadata( mdv ) )
            throw std::runtime_error( "Only annotation of integer values is allowed" );

        auto v = mdv.value();

        preprocess( getFunction( v ) );
        tasks.push_back( [&]{ propagate_root( v ); } );
    }

    while ( !tasks.empty() ) {
        tasks.front()();
        tasks.pop_front();
    }
}

} // namespace abstract
} // namespace lart
