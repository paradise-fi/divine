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

#include <lart/abstract/operation.h>
#include <lart/abstract/assume.h>
#include <lart/abstract/util.h>
#include <lart/analysis/edge.h>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {
    struct Assumption {
        Assumption( Value *cond, Constant *val )
            : cond( cond ), val( val )
        {}

        Value *cond;
        Constant *val;
    };

    void replace_phis_incoming_bbs( BasicBlock *bb, BasicBlock *oldbb, BasicBlock *newbb ) {
        for ( auto& inst : *bb ) {
            if ( auto phi = dyn_cast< PHINode >( &inst ) ) {
                int bbidx = phi->getBasicBlockIndex( oldbb );
                if ( bbidx >= 0 )
                    phi->setIncomingBlock( bbidx, newbb );
            }
        }
    }

    using BB = llvm::BasicBlock;
    using BBEdge = analysis::BBEdge;
    struct AssumeEdge : BBEdge {
        AssumeEdge( BB *from, BB *to )
            : BBEdge( from, to )
        {}

        void assume( Assumption ass ) {
            unsigned i = succ_idx( from, to );
            SplitEdge( from, to );

            auto edgebb = from->getTerminator()->getSuccessor( i );
            to = edgebb->getSingleSuccessor();
            auto cond = llvm::cast< llvm::Instruction >( ass.cond );

            llvm::IRBuilder<> irb( &*edgebb->getFirstInsertionPt() );

            OperationBuilder builder;

            auto op = builder.construct< Operation::Type::Assume >( cond, irb );

            auto abs = cond->getModule()->getFunction( "__lart_abstract_assume" );
            auto tag = meta::tag::operation::index;
            op.inst->setMetadata( tag, abs->getMetadata( tag ) );

            // Correct phis after edge splitting
            replace_phis_incoming_bbs( to, from, edgebb );
        }

        unsigned succ_idx( BB * from, BB * to ) {
            auto term = from->getTerminator();
            for ( unsigned i = 0; i < term->getNumSuccessors(); ++i ) {
                if ( term->getSuccessor( i ) == to )
                    return i;
            }
            UNREACHABLE( "BasicBlock 'to' is not a successor of BasicBlock 'from'." );
        }
    };
}

    void AddAssumes::run( Module & m ) {
        for ( const auto & ph : operations< Operation::Type::ToBool >( m ) )
            for ( auto * u : ph.inst->users() )
                if ( auto * br = llvm::dyn_cast< llvm::BranchInst >( u ) )
                    process( br );
    }

    void AddAssumes::process( BranchInst *br ) {
        auto to_i1 = cast< CallInst >( br->getCondition() );

        auto &ctx = br->getContext();

        AssumeEdge true_br = { br->getParent(), br->getSuccessor( 0 ) };
        true_br.assume( { to_i1, ConstantInt::getTrue( ctx ) } );

        AssumeEdge false_br = { br->getParent(), br->getSuccessor( 1 ) };
        false_br.assume( { to_i1, ConstantInt::getFalse( ctx ) } );
    }

} /* namespace abstract */
} /* namespace lart */
