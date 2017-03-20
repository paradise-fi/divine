// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/abstraction.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/walker.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/lowerselect.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <lart/abstract/types.h>

namespace lart {
namespace abstract {

namespace {

bool isLifted( const std::vector< llvm::Value * > & deps ) {
    for ( auto & dep : deps )
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( dep ) )
            if ( intrinsic::isLift( call ) ) return true;
    return false;
}

} // anonymous namespace

llvm::PreservedAnalyses Abstraction::run( llvm::Module & m ) {
    auto preprocess = [] ( llvm::Function * fn ) {
        auto lowerSwitchInsts = llvm::createLowerSwitchPass();
        lowerSwitchInsts->runOnFunction( *fn );

        // FIXME lower only abstract selects
        LowerSelect lowerSelectInsts;
        lowerSelectInsts.runOnFunction( *fn );

        llvm::UnifyFunctionExitNodes ufen;
        ufen.runOnFunction( *fn );
    };

    using Preprocess = decltype( preprocess );
    AbstractWalker< Preprocess > walker{ preprocess, m };

    llvm::ValueToValueMapTy vmap;

    for ( const auto & node : walker.postorder() ) {
        // 1. if signature changes create a new function declaration
        auto changed = llvm::cast< llvm::Function >( builder.process( node ) );

        // 2. prcess function abstract entries
        auto postorder = node->postorder();
        for ( auto & a : lart::util::reverse( postorder ) ) {
            // FIXME let builder take Annotation structure
            builder.process( a.value );
        }

        // 3. clean function
        // FIXME let builder take annotation structure
        std::vector< llvm::Value * > deps;
        for ( auto & a : postorder )
            deps.emplace_back( a.value );
        builder.clean( deps );

        // 4. copy function to declaration and handle function uses
        auto fn = node->function;
        if ( changed != fn ) {
            auto destIt = changed->arg_begin();
            for ( const auto &arg : fn->args() )
                if ( vmap.count( &arg ) == 0 ) {
                    destIt->setName( arg.getName() );
                    vmap[&arg] = &*destIt++;
                }
            // FIXME CloneDebugInfoMeatadata
            llvm::SmallVector< llvm::ReturnInst *, 8 > returns;
            llvm::CloneFunctionInto( changed, fn, vmap, false, returns, "", nullptr );
            // Remove lifts of arguments
            for ( auto & arg : changed->args() ) {
                auto lifts = query::query( arg.users() )
                            .map( query::llvmdyncast< llvm::CallInst > )
                            .filter( query::notnull )
                            .filter( [&]( llvm::CallInst * call ) {
                                 return intrinsic::isLift( call );
                             } ).freeze();
                for ( auto & lift : lifts ) {
                    lift->replaceAllUsesWith( &arg );
                    lift->eraseFromParent();
                }
            }
        }

        // 5. add function to unused functions ?
        // TODO
    }

    // TODO clean unused functions

    return llvm::PreservedAnalyses::none();
}

} // abstract
} // lart

