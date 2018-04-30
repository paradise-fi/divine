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
#include <lart/abstract/taint.h>
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

    using BB = llvm::BasicBlock;
    using BBEdge = analysis::BBEdge;
    struct AssumeEdge : BBEdge {
        AssumeEdge( BB *from, BB *to )
            : BBEdge( from, to )
        {}

        Function* assume_fn( Module *m, Type *concrete, Type *abstract, Domain dom ) {
            auto i1 = Type::getInt1Ty( m->getContext() );
            auto fty = FunctionType::get( abstract, { i1, concrete, i1, abstract, i1, i1 }, false );
            std::string tag = "lart.gen." + DomainTable[ dom ] + ".assume";

            return cast< Function >( m->getOrInsertFunction( tag, fty ) );
        }

        void assume( Assumption ass ) {
            unsigned i = succ_idx( from, to );
            SplitEdge( from, to );

            auto edge_bb = from->getTerminator()->getSuccessor( i );

            auto taint = cast< Instruction >( ass.cond );
            auto dom = MDValue( get_dual( taint ) ).domain();

            auto m = get_module( ass.cond );
            auto abstract = ass.cond;
            auto concrete = get_dual( cast< Instruction >( abstract ) );

            Values args = { assume_fn( from->getModule(), concrete->getType(), abstract->getType(), dom ),
                            abstract,  // fallback value
                            concrete,
                            abstract,  // assumed condition
                            ass.val }; // result of assumed condition

            auto fn = get_taint_fn( m, abstract->getType(), types_of( args ) );

            llvm::IRBuilder<> irb( &edge_bb->front() );
            auto tass = irb.CreateCall( fn, args );

            auto dual = cast< Instruction >( get_dual( taint ) );
            tass->setMetadata( "lart.domains", dual->getMetadata( "lart.domains" ) );
            // TODO use assumed result
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
        for ( auto t : taints( m ) )
            for ( auto u : t->users() )
                if ( auto br = dyn_cast< BranchInst >( u ) )
                    process( br );
    }

    void AddAssumes::process( BranchInst *br ) {
        auto to_i1 = cast< CallInst >( br->getCondition() );
        auto cond = get_dual( cast< Instruction >( to_i1->getOperand( 1 ) ) );

        auto &ctx = br->getContext();
        AssumeEdge true_br = { br->getParent(), br->getSuccessor( 0 ) };
        true_br.assume( { cond, ConstantInt::getTrue( ctx ) } );

        AssumeEdge false_br = { br->getParent(), br->getSuccessor( 1 ) };
        false_br.assume( { cond, ConstantInt::getFalse( ctx ) } );
    }

} /* namespace abstract */
} /* namespace lart */
