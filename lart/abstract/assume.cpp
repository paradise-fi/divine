// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

#include <llvm/Support/Casting.h>

#include <llvm/Transforms/Utils/BasicBlockUtils.h>
DIVINE_UNRELAX_WARNINGS
#include <lart/abstract/assume.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/types.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

namespace {
    struct Assumption {
        Assumption( llvm::Value * cond, llvm::Constant * val ) : cond( cond ), val( val ) {}

        llvm::Value * cond;
        llvm::Constant * val;
    };

    template< typename Node >
    struct Edge {
        Edge( Node from, Node to ) : from( from ), to( to ) {}

        Node from;
        Node to;

        void assume( Assumption assume ) {
            llvm::SplitEdge( from, to );
            auto rty = llvm::Type::getVoidTy( from->getContext() );
            auto fty = llvm::FunctionType::get( rty,
                                              { assume.cond->getType(), assume.val->getType()},
                                                false );
            std::string tag = "lart.tristate.assume";
            auto call = from->getModule()->getOrInsertFunction( tag, fty );

            llvm::IRBuilder<> irb( &to->front() );
            irb.CreateCall( call, { assume.cond, assume.val } );
        }
    };

    using BBEdge = Edge< llvm::BasicBlock * >;

    llvm::Value * getTristate( llvm::CallInst * lower ) {
        ASSERT( lower->getCalledFunction()->getName().startswith( "lart.tristate.lower" ) );
        return lower->getOperand( 0 );
    }
}

    llvm::PreservedAnalyses AddAssumes::run( llvm::Module & m ) {
        auto branches = query::query( m )
					   .flatten().flatten()
					   .map( query::refToPtr )
					   .map( query::llvmdyncast< llvm::BranchInst > )
					   .filter( query::notnull )
					   .filter( []( llvm::BranchInst * br ) {
							return br->isConditional();
						} )
						.freeze();

		for ( auto & br : branches )
			process( br );

        return llvm::PreservedAnalyses::none();
    }

    void AddAssumes::process( llvm::Instruction * inst ) {
        auto br = llvm::dyn_cast< llvm::BranchInst >( inst );
        ASSERT( br && "Cannot assume about non-branch instruction.");

        auto lower = llvm::dyn_cast< llvm::CallInst >( br->getCondition() );
        if ( lower && intrinsic::isLower( lower ) ) {
            auto tristate = getTristate( lower );

            BBEdge trueBr = { br->getParent(), br->getSuccessor( 0 ) };
            trueBr.assume( { tristate, types::Tristate::True() } );
            BBEdge falseBr = { br->getParent(), br->getSuccessor( 1 ) };
            falseBr.assume( { tristate, types::Tristate::False() } );
        }
    }


} /* namespace abstract */
} /* namespace lart */
