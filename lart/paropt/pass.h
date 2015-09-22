// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <lart/support/util.h>

#include <vector>
#include <unordered_set>
#include <utility>

#include <iostream>

namespace lart {
namespace paropt {

/*MD
# Const Conditional Jump Elimination
*/
struct ConstConditionalJumpElimination : lart::Pass {

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

The alloca can look like this (all in one basic block):

```llvm
%var = alloca <type>
; any number of alloca, llvm.gdb.declare calls, bitcasts, and safe stores
%
store <type> %val, <type>* %var
```

this is definition of alloca `%var` with initial value of register `%val`.

If any every use `%var` is load, then we can eliminate `%var` and replace results
of all loads with `%val`.

*/
struct ConstAllocaElimination : lart::Pass {

    void processFunction( llvm::Function &fn ) {
        std::vector< std::pair< llvm::AllocaInst *, llvm::StoreInst * > > vars;

        for ( auto &bb : fn ) {
            for ( auto &inst : bb ) {
                llvmcase( inst, [&]( llvm::AllocaInst *alloca ) {
                    ++allAllocas;
                    bool seen = false;
                    bool done = false;
                    std::unordered_set< llvm::Value * > tracking{ alloca };

                    for ( auto &i : bb ) {
                        if ( !seen )
                            seen = &i == alloca;
                        llvmcase( i,
                            []( llvm::DbgDeclareInst * ) { },
                            []( llvm::AllocaInst * ) { },
                            [&]( llvm::CastInst *cast ) {
                                if ( tracking.count( cast->getOperand( 0 ) ) )
                                    done = true; // TODO
                                    // tracking.insert( cast );
                            },
                            [&]( llvm::StoreInst *store ) {
                                auto val = store->getValueOperand();
                                if ( tracking.count( val ) ) {
                                    done = true; // pointer to alloca taken (escape)
                                } else {
                                    auto ptr = store->getPointerOperand();
                                    if ( ptr == alloca ) {
                                        vars.emplace_back( alloca, store );
                                        done = true; // found
                                    }
                                }
                            },
                            [&]( llvm::Value * ) { done = true; } ); // may have escaped

                        if ( done )
                            break;
                    }
                } );
            }
        }

        for ( auto var : vars ) {
            bool allLoads = true;
            std::vector< llvm::Value * > users;
            for ( auto uit = var.first->use_begin(), end = var.first->use_end(); allLoads && uit != end; ++uit ) {
                allLoads &= *uit == var.second || llvm::isa< llvm::LoadInst >( *uit );
                users.push_back( *uit );
            }
            if ( allLoads ) {
                auto val = var.second->getValueOperand();
                for ( auto user : users )
                    llvmcase( user,
                        [val]( llvm::LoadInst *load ) {
                            load->replaceAllUsesWith( val );
                            load->eraseFromParent();
                        },
                        []( llvm::DbgDeclareInst *dbg ) {
                            dbg->eraseFromParent(); // TODO: fixme
                        },
                        [var]( llvm::StoreInst *store ) {
                            ASSERT_EQ( store, var.second );
                            store->eraseFromParent();
                        },
                        []( llvm::Value *val ) {
                            val->dump();
                            ASSERT_UNREACHABLE( "unhandled case" );
                        }
                        );
                var.first->eraseFromParent();
                ++deletedAllocas;
            }
        }
    }

    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        for ( auto &fn : m )
            processFunction( fn );

        std::cout << "INFO: removed " << deletedAllocas << " out of " << allAllocas
                  << " (" << double( 100 * deletedAllocas ) / allAllocas
                  << "%) allocas" << std::endl;
        return llvm::PreservedAnalyses::none();
    }

    long allAllocas = 0;
    long deletedAllocas = 0;
};

} // namespace constify
} // namespace lart
