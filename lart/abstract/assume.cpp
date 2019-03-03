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
#include <lart/abstract/util.h>
#include <lart/analysis/edge.h>

namespace lart {
namespace abstract {

using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

namespace {
    struct Assumption {
        Assumption( Value *cond, Constant *val )
            : cond( cond ), val( val )
        {}

        Value *cond;
        Constant *val;
    };

    Function* assume_placeholder( Instruction *cond ) {
        auto i1 = Type::getInt1Ty( cond->getContext() );
        auto fty = llvm::FunctionType::get( i1, { i1, i1 }, false );
        return get_or_insert_function( get_module( cond ), fty, "lart.assume.placeholder" );
    }

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
            auto to_i1 = cast< Instruction >( ass.cond );

            llvm::IRBuilder<> irb( &*edgebb->getFirstInsertionPt() );
            auto ph = irb.CreateCall( assume_placeholder( to_i1 ), { ass.cond, ass.val } );
            add_abstract_metadata( ph, Domain::get( to_i1 ) );

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
        for ( auto ph : placeholders( m ) )
            for ( auto u : ph->users() )
                if ( auto br = dyn_cast< BranchInst >( u ) )
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
