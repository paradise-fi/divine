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
#include <brick-assert>

#include <lart/abstract/assume.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/types/common.h>
#include <lart/support/query.h>
#include <lart/analysis/edge.h>

namespace lart {
namespace abstract {

namespace {
    struct Assumption {
        Assumption( llvm::Value * cond, llvm::Constant * val ) : cond( cond ), val( val ) {}

        llvm::Value * cond;
        llvm::Constant * val;
    };

    using BB = llvm::BasicBlock;
    using BBEdge = analysis::BBEdge;
    struct AssumeEdge : BBEdge {
        AssumeEdge( BB * from, BB * to ) : BBEdge( from, to ) {}

        void assume( Assumption assume ) {
            unsigned i = succ_idx( from, to );
            llvm::SplitEdge( from, to );
            auto edgeBB = from->getTerminator()->getSuccessor( i );
            auto rty = VoidType( from->getContext() );
            auto fty = llvm::FunctionType::get( rty,
                                              { assume.cond->getType(), assume.val->getType()},
                                                false );
            std::string tag = "lart.tristate.assume";
            auto call = from->getModule()->getOrInsertFunction( tag, fty );

            llvm::IRBuilder<> irb( &edgeBB->front() );
            irb.CreateCall( call, { assume.cond, assume.val } );
        }

        unsigned succ_idx( BB * from, BB * to ) {
            auto term = from->getTerminator();
            for ( unsigned i = 0; i < term->getNumSuccessors(); ++i ) {
                if ( term->getSuccessor( i ) == to )
                    return i;
            }
            UNREACHABLE( "BasicBlock 'to' is not successor of BasicBlock 'from'." );
        }
    };

    llvm::Value * getTristate( llvm::CallInst * lower ) {
        ASSERT( lower->getCalledFunction()->getName().startswith( "lart.tristate.lower" ) );
        return lower->getOperand( 0 );
    }
}

    void AddAssumes::run( llvm::Module & m ) {
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
    }

    void AddAssumes::process( llvm::Instruction * inst ) {
        auto br = llvm::dyn_cast< llvm::BranchInst >( inst );
        ASSERT( br && "Cannot assume about non-branch instruction.");

        auto lower = llvm::dyn_cast< llvm::CallInst >( br->getCondition() );
        if ( lower && isLower( lower ) ) {
            auto tristate = getTristate( lower );

            AssumeEdge trueBr = { br->getParent(), br->getSuccessor( 0 ) };
            trueBr.assume( { tristate, llvm::ConstantInt::getTrue( inst->getContext() ) } );
            AssumeEdge falseBr = { br->getParent(), br->getSuccessor( 1 ) };
            falseBr.assume( { tristate, llvm::ConstantInt::getFalse( inst->getContext() ) } );
        }
    }

} /* namespace abstract */
} /* namespace lart */
