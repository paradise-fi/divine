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
        auto lowerSwitchInsts = std::unique_ptr< llvm::FunctionPass >(
                                llvm::createLowerSwitchPass() );
        lowerSwitchInsts->runOnFunction( *fn );

        // FIXME lower only abstract selects
        LowerSelect lowerSelectInsts;
        lowerSelectInsts.runOnFunction( *fn );

        llvm::UnifyFunctionExitNodes ufen;
        ufen.runOnFunction( *fn );
    };

    using Preprocess = decltype( preprocess );
    AbstractWalker< Preprocess > walker{ preprocess, m };

    std::vector< llvm::Function * > remove;

    auto functions = walker.postorder();

    // create function declarations
    Map< FunctionNodePtr, llvm::Function * > declarations;

    for ( const auto & node : functions )
        declarations[ node ] = llvm::cast< llvm::Function >( builder.process( node ) );
    for ( const auto & node : functions ) {
        // 1. if signature changes create a new function declaration

        // if proccessed function is called with abstract argument create clone of it
        // to preserve original function for potential call without abstract argument
        // TODO what about function with abstract argument and abstract annotation?
        bool called = query::query( node->entries ).any( [] ( const ValueNode & n ) {
            return llvm::isa< llvm::Argument >( n.value );
        } );

        if ( called )
            builder.clone( node );

        // 2. process function abstract entries
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
        auto & changed = declarations[ node ];

        if ( changed != fn ) {
            remove.push_back( fn );
            llvm::ValueToValueMapTy vmap;
            auto destIt = changed->arg_begin();
            for ( const auto &arg : fn->args() )
                if ( vmap.count( &arg ) == 0 ) {
                    destIt->setName( arg.getName() );
                    vmap[&arg] = &*destIt++;
                }
            // FIXME CloneDebugInfoMeatadata
            llvm::SmallVector< llvm::ReturnInst *, 8 > returns;
            llvm::CloneFunctionInto( changed, fn, vmap, false, returns, "", nullptr );
            // Remove lifts of abstract arguments
            for ( auto & arg : changed->args() ) {
                if ( types::isAbstract( arg.getType() ) ) {
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
        }
    }

    for ( auto & fn : lart::util::reverse( remove ) )
       fn->eraseFromParent();
    return llvm::PreservedAnalyses::none();
}

} // abstract
} // lart

