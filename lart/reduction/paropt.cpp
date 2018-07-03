// -*- C++ -*- (c) 2015-2017 Vladimír Štill <xstill@fi.muni.cz>
//
DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DIBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/support/meta.h>
#include <lart/support/pass.h>
#include <lart/support/query.h>

#include <brick-types>

#include <vector>
#include <unordered_set>
#include <utility>

#include <iostream>

namespace lart {
namespace reduction {

/*MD
# Const Conditional Jump Elimination
*/
struct ConstConditionalJumpElimination {

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

    void run( llvm::Module &m ) {
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
        // std::cerr << "INFO: changed " << changed << " branches, removed " << removed << " basic blocks" << std::endl;
    }

  private:
    long changed = 0;
    long removed = 0;
};

/*MD
# Merge Basic Blocks
*/
struct MergeBasicBlocks {

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

    void run( llvm::Module &m ) {
        for ( auto &f : m )
            if ( !f.empty() )
                mergeBB( &f.getEntryBlock() );
        // std::cerr << "INFO: merged " << merged << " basic blocks" << std::endl;
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

Note that calls to llvm.dbg.declare do not count as uses.
*/
struct ConstAllocaElimination {

    static auto meta() {
        return passMeta< ConstAllocaElimination >( "ConstAllocaElimination" );
    }

    void processFunction( llvm::Function &fn ) {
        std::vector< std::pair< llvm::AllocaInst *, llvm::StoreInst * > > vars;
        llvm::AnalysisManager< llvm::Function > am;
        auto dt = brick::types::lazy( [&]()
                                      {
                                          return llvm::DominatorTreeAnalysis().run( fn, am );
                                      } );

        auto stacksaves = query::query( fn ).flatten()
                            .map( query::refToPtr )
                            .map( query::llvmdyncast< llvm::IntrinsicInst > )
                            .filter( query::notnull )
                            .filter( []( llvm::IntrinsicInst *ii ) {
                                        return ii->getIntrinsicID() == llvm::Intrinsic::stacksave;
                                    } )
                            .freeze();

        for ( auto &inst: query::query( fn ).flatten() ) {
            if ( auto *alloca = llvm::dyn_cast< llvm::AllocaInst >( &inst ) ) {
                ++allAllocas;
                bool captured = llvm::PointerMayBeCaptured( alloca, false, true );
                // we can't safely eliminate allocas within scope of stacksave
                // as they can be later freed before the function exits --
                // eliminating them could hide memory errors
                bool stacksaveDominated = query::query( stacksaves ).any( [&]( auto *ss ) {
                                return dt->dominates( ss, alloca );
                            } );

                if ( !captured && !stacksaveDominated ) {
                    auto users = query::query( alloca->users() )
                                    .filter( query::isnot< llvm::DbgDeclareInst > && query::isnot< llvm::DbgValueInst > );
                    llvm::StoreInst *store = users.map( query::llvmdyncast< llvm::StoreInst > )
                            .filter( query::notnull )
                            .singleton()
                            .getOr( nullptr );
                    if ( store && store->getPointerOperand() == alloca // single store
                            && users.filter( query::isnot< llvm::StoreInst > )
                                    .all( query::is< llvm::LoadInst > ) // + only loads and dbg info
                            && users.map( query::llvmdyncast< llvm::LoadInst > ) // store dominates all loads
                                    .filter( query::notnull )
                                    .all( [&]( auto l ) { return dt->dominates( store, l ); } )
                       )
                        vars.emplace_back( alloca, store );
                }
            }
        }

        for ( auto var : vars ) {
            std::vector< llvm::DbgInfoIntrinsic * > dbgs;
            auto val = var.second->getValueOperand();
            std::vector< llvm::Instruction * > toDelete;
            auto dbgdec = llvm::FindAllocaDbgDeclare( var.first );
            if ( dbgdec ) {
                auto expr = dbgdec->getExpression();
                if ( expr->getElements().size() ) {
                    std::cerr << brick::string::format(
                                 "don't know what to do with this dbg.declare:",
                                 dbgdec, "for", var.first );
                    dbgdec = nullptr;
                }
            }
            for ( auto user : var.first->users() )
                llvmcase( user,
                    [val,&toDelete]( llvm::LoadInst *load ) {
                        load->replaceAllUsesWith( val );
                        toDelete.push_back( load );
                    },
                    [&]( llvm::DbgInfoIntrinsic *dbg ) {
                        toDelete.push_back( dbg );
                    },
                    [&]( llvm::StoreInst *store ) {
                        ASSERT_EQ( store, var.second );
                        toDelete.push_back( store );

                        if ( !dbgdec )
                            return;

                        auto local = dbgdec->getVariable();
                        auto localSubprogram = local->getScope()->getSubprogram();
                        auto dbgloc = store->getDebugLoc().get();
                        // TODO: dbg.value needs subprograms to match, but where can we find the right dbgloc?
                        if ( dbgloc && dbgloc->getScope()->getSubprogram() != localSubprogram )
                            dbgloc = nullptr;

                        if ( !dbgloc )
                            return;

                        llvm::DIBuilder dib( *fn.getParent() );
                        dib.insertDbgValueIntrinsic( val, 0, local, dbgdec->getExpression(), dbgloc, store );
                    },
                    [&]( llvm::Value *val ) {
                        std::cerr << "in " << fn.getName().str() << std::endl;
                        fn.dump();
                        var.first->dump();
                        var.second->dump();
                        val->dump();
                        UNREACHABLE( "unhandled case" );
                    } );
            for ( auto i : toDelete )
                i->eraseFromParent();
            var.first->eraseFromParent();
            ++deletedAllocas;
        }
    }

    void run( llvm::Module &m ) {

        for ( auto &fn : m )
            processFunction( fn );

        // std::cerr << "INFO: removed " << deletedAllocas << " out of " << allAllocas
        //          << " (" << double( 100 * deletedAllocas ) / allAllocas
        //          << "%) allocas" << std::endl;
    }

    long allAllocas = 0;
    long deletedAllocas = 0;
};

PassMeta paroptPass() {
    return compositePassMeta< ConstConditionalJumpElimination, MergeBasicBlocks
        , ConstAllocaElimination >( "paropt", "Parallel-safe optimizations" );
}

} // namespace reduce
} // namespace lart
