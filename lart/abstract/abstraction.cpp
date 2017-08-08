// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/abstraction.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/intrinsic.h>
#include <lart/support/lowerselect.h>

namespace lart {
namespace abstract {

void Abstraction::run( llvm::Module & m ) {
    auto preprocess = [] ( llvm::Function * fn ) {
        auto lowerSwitchInsts = std::unique_ptr< llvm::FunctionPass >(
                                llvm::createLowerSwitchPass() );
        lowerSwitchInsts.get()->runOnFunction( *fn );

        // FIXME lower only abstract selects
        LowerSelect lowerSelectInsts;
        lowerSelectInsts.runOnFunction( *fn );

        llvm::UnifyFunctionExitNodes ufen;
        ufen.runOnFunction( *fn );
    };

    std::vector< llvm::Function * > remove;
    auto functions = Walker( m, preprocess ).postorder();

    // create function declarations
    std::unordered_map< FunctionNodePtr, llvm::Function * > declarations;

    for ( const auto & node : functions )
        declarations[ node ] =  builder.process( node );
    for ( const auto & node : functions ) {
        // 1. if signature changes create a new function declaration

        // if proccessed function is called with abstract argument create clone of it
        // to preserve original function for potential call without abstract argument
        // TODO what about function with abstract argument and abstract annotation?
        bool called = query::query( node->origins ).any( [] ( const auto & n ) {
            return llvm::isa< llvm::Argument >( n.value() );
        } );

        if ( called )
            builder.clone( node );

        // 2. process function abstract entries
        auto postorder = node->postorder();
        for ( auto & val : lart::util::reverse( postorder ) )
            builder.process( val );

        // 3. clean function
        // FIXME let builder take annotation structure
        std::vector< llvm::Value * > deps;
        for ( auto & val : postorder )
            deps.emplace_back( val.value() );
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
                if ( isAbstract( arg.getType() ) ) {
                    auto lifts = query::query( arg.users() )
                        .map( query::llvmdyncast< llvm::CallInst > )
                        .filter( query::notnull )
                        .filter( [&]( llvm::CallInst * call ) {
                             return isLift( call );
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
}

} // abstract
} // lart

