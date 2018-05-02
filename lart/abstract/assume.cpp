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

#include <lart/abstract/metadata.h>
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

    Function* assume_placeholder( Instruction *cond ) {
        auto i1 = Type::getInt1Ty( cond->getContext() );
        auto fty = llvm::FunctionType::get( i1, { i1, i1 }, false );
        return get_or_insert_function( get_module( cond ), fty, "lart.assume.placeholder" );
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

            auto edge_bb = from->getTerminator()->getSuccessor( i );

            auto to_i1 = cast< Instruction >( ass.cond );

            llvm::IRBuilder<> irb( &edge_bb->front() );
            irb.CreateCall( assume_placeholder( to_i1 ), { ass.cond, ass.val } );
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
