// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <lart/support/util.h>
#include <lart/support/meta.h>
#include <lart/support/pass.h>
#include <lart/support/query.h>
#include <lart/analysis/escape.h>

#include <brick-types.h>

#include <vector>
#include <unordered_set>
#include <utility>

#include <iostream>

namespace lart {
namespace reduction {

/*MD
# Const Conditional Jump Elimination
*/
struct ConstConditionalJumpElimination : lart::Pass {

    static PassMeta meta() {
        return passMeta< ConstConditionalJumpElimination >(
                "ConstConditionalJumpElimination",
                "Replace constant conditional jumps with unconditional branches." );
    }

    llvm::BasicBlock *constJump( llvm::BasicBlock &bb ) {
        if ( auto br = llvm::dyn_cast< llvm::BranchInst >( bb.getTerminator() ) ) {
            if ( br->isConditional() ) {
                if ( auto cond = llvm::dyn_cast< llvm::ConstantInt >( br->getCondition() ) ) {
                    ++changed;
                    ASSERT_EQ( br->getNumSuccessors(), 2u );
                    auto unlinked = br->getSuccessor( !cond->isZero() );
                    unlinked->removePredecessor( &bb );
                    llvm::ReplaceInstWithInst( br, llvm::BranchInst::Create( br->getSuccessor( cond->isZero() ) ) );
                    return llvm::pred_begin( unlinked ) == llvm::pred_end( unlinked ) ? unlinked : nullptr;
                }
            }
        }
        return nullptr;
    }

    std::vector< llvm::BasicBlock * > removeUnused( llvm::BasicBlock *bb ) {
        std::vector< llvm::BasicBlock * > succs;
        auto term = bb->getTerminator();
        ASSERT( term );
        int cnt = term->getNumSuccessors();
        for ( int i = 0; i < cnt; ++i )
            succs.push_back( term->getSuccessor( i ) );
        llvm::DeleteDeadBlock( bb );

        std::vector< llvm::BasicBlock * > unreachable;
        for ( auto succ : succs ) {
            if ( llvm::pred_begin( succ ) == llvm::pred_end( succ ) && succ != bb )
                unreachable.push_back( succ );
        }
        ++removed;
        return unreachable;
    }

    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        for ( auto &fn : m ) {
            std::unordered_set< llvm::BasicBlock * > unreachable;

            for ( auto &bb : fn )
                if ( auto unr = constJump( bb ) )
                    unreachable.insert( unr );
            while ( !unreachable.empty() ) {
                auto todo = *unreachable.begin();
                unreachable.erase( todo );
                auto unr = removeUnused( todo );
                unreachable.insert( unr.begin(), unr.end() );
            }
        }
        std::cerr << "INFO: changed " << changed << " branches, removed " << removed << " basic blocks" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

  private:
    long changed = 0;
    long removed = 0;
};

/*MD
# Merge Basic Blocks
*/
struct MergeBasicBlocks : lart::Pass {

    static PassMeta meta() {
        return passMeta< MergeBasicBlocks >( "MergeBasicBlocks" );
    }

    void mergeBB( llvm::BasicBlock *bb ) {
        std::unordered_set< llvm::BasicBlock * > seen;
        mergeBB( 0, bb, seen );
    }

    void mergeBB( int parsuccs, llvm::BasicBlock *bb, std::unordered_set< llvm::BasicBlock * > &seen ) {
        if ( seen.count( bb ) )
            return;

        seen.insert( bb );
        auto term = bb->getTerminator();
        int cnt = term->getNumSuccessors();
        for ( int i = 0; i < cnt; ++i )
            mergeBB( cnt, term->getSuccessor( i ), seen );
        if ( parsuccs == 1 )
            // this checks if remove is safe
            if ( llvm::MergeBlockIntoPredecessor( bb ) )
                ++merged;
    }

    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        for ( auto &f : m )
            if ( !f.empty() )
                mergeBB( &f.getEntryBlock() );
        std::cerr << "INFO: merged " << merged << " basic blocks" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

    long merged = 0;
};

/*MD

# Const Alloca Elimination

Clang often generates allocas even for variables which are not changed ever,
those can be eliminated without changing properties of parallel program.

The alloca can be eliminated if:
1.  it does not escape function's scope
2.  is only accessed by load and store instruction
3.  there is only one store
4.  the store dominates all loads

Not that calls to llvm.dbg.declare do not count as uses.
*/
struct ConstAllocaElimination : lart::Pass {

    static auto meta() {
        return passMeta< ConstAllocaElimination >( "ConstAllocaElimination" );
    }

    void processFunction( llvm::Function &fn, analysis::EscapeAnalysis::Analysis &&esc ) {
        std::vector< std::pair< llvm::AllocaInst *, llvm::StoreInst * > > vars;
        auto dt = brick::types::lazy( [&]() { return llvm::DominatorTreeAnalysis().run( fn ); } );

        for ( auto &inst: query::query( fn ).flatten() ) {
            llvmcase( inst, [&]( llvm::AllocaInst *alloca ) {
                ++allAllocas;
                if ( esc.escapes( alloca ).empty() ) {
                    auto users = query::query( alloca->users() ).filter( query::isnot< llvm::DbgDeclareInst > );
                    llvm::StoreInst *store = users.map( query::llvmdyncast< llvm::StoreInst > )
                            .filter( query::notnull )
                            .singleton()
                            .getOr( nullptr );
                    if ( store // single store
                            && users.filter( query::isnot< llvm::StoreInst > )
                                    .all( query::is< llvm::LoadInst > ) // + only loads and dbg info
                            && users.map( query::llvmdyncast< llvm::LoadInst > ) // store dominates all loads
                                    .filter( query::notnull )
                                    .all( [&]( auto l ) { return dt->dominates( store, l ); } )
                       )
                        vars.emplace_back( alloca, store );
                }
            } );
        }

        for ( auto var : vars ) {
            auto val = var.second->getValueOperand();
            for ( auto user : var.first->users() )
                llvmcase( user,
                    [val]( llvm::LoadInst *load ) {
                        load->replaceAllUsesWith( val );
                        load->eraseFromParent();
                    },
                    []( llvm::DbgDeclareInst *dbg ) {
                        dbg->eraseFromParent(); // TODO: fixme: copy debuginfo somehow
                    },
                    [var]( llvm::StoreInst *store ) {
                        ASSERT_EQ( store, var.second );
                        store->eraseFromParent();
                    },
                    []( llvm::Value *val ) {
                        val->dump();
                        ASSERT_UNREACHABLE( "unhandled case" );
                    } );
            var.first->eraseFromParent();
            ++deletedAllocas;
        }
    }

    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        analysis::EscapeAnalysis escape( m );

        for ( auto &fn : m )
            processFunction( fn, escape.analyze( fn ) );

        std::cout << "INFO: removed " << deletedAllocas << " out of " << allAllocas
                  << " (" << double( 100 * deletedAllocas ) / allAllocas
                  << "%) allocas" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

    long allAllocas = 0;
    long deletedAllocas = 0;
};

PassMeta paroptPass() {
    return compositePassMeta< ConstConditionalJumpElimination, MergeBasicBlocks, ConstAllocaElimination >(
            "paropt", "Parallel-safe optimizations" );
}

} // namespace reduce
} // namespace lart
