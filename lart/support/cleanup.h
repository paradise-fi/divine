// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/ADT/Optional.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-types>
#include <brick-data>

#include <lart/support/query.h>
#include <lart/analysis/bbreach.h>

#ifndef LART_SUPPORT_CLEANUP_H
#define LART_SUPPORT_CLEANUP_H

namespace lart {
namespace cleanup {

struct EhInfo {
    EhInfo() : personality( nullptr ), landingpadType( nullptr ), cleanupSelector( 0 ) { }
    EhInfo( llvm::Function *personality, llvm::Type *landingpadType,
            std::initializer_list< unsigned > idxs, long cleanupSelector ) :
        personality( personality ), landingpadType( landingpadType ),
        selectorIndices( idxs ), cleanupSelector( cleanupSelector )
    { }

    bool valid() const { return personality && landingpadType; }
    operator bool() const { return valid(); }

    static EhInfo cpp( llvm::Module &m ) {
        auto &ctx = m.getContext();
        return EhInfo(
            m.getFunction( "__gxx_personality_v0" ),
            llvm::StructType::get( llvm::Type::getInt8PtrTy( ctx ), llvm::Type::getInt32Ty( ctx ), nullptr ),
            { 1 }, 0 );
    }

    llvm::Function *personality;
    llvm::Type *landingpadType;
    std::vector< unsigned > selectorIndices;
    long cleanupSelector;
};

template< typename ShouldTransformCall >
void makeExceptionsVisible( EhInfo ehi, llvm::Function &fn, ShouldTransformCall &&shouldTransform )
{
    if ( !ehi.valid() ) // no exception information, maybe pure C bitcode…
        return;

    auto calls = query::query( fn ).flatten()
        .filter( query::is< llvm::CallInst > || query::is< llvm::InvokeInst > )
        .map( []( llvm::Instruction &call ) { return llvm::CallSite( &call ); } )
        .filter( []( llvm::CallSite &cs ) { return !cs.doesNotThrow(); } )
        .filter( shouldTransform )
        .freeze(); // avoid changing BBs while iterating through them

    // phase 1 = replace calls with invokes landing in cleanup with resume
    //           and add cleanup with resume to all non-cleanup landing blocks
    bool invokeAdded = false;
    for ( auto &cs : calls ) {

        if ( cs.isInvoke() ) {
            auto *inst = llvm::cast< llvm::InvokeInst >( cs.getInstruction() );
            auto *unwindBB = inst->getUnwindDest();
            auto *lp = llvm::cast< llvm::LandingPadInst >( unwindBB->getFirstNonPHI() );

            if ( !lp->isCleanup() ) {
                lp->setCleanup( true );
                auto *noCleanupBB = unwindBB->splitBasicBlock( unwindBB->getFirstInsertionPt() , "lart.cleanup.lb.orig" );
                auto *cleanupBB = llvm::BasicBlock::Create( fn.getParent()->getContext(), "lart.cleanup.lb.cleanup", &fn );
                llvm::IRBuilder<>( cleanupBB ).CreateResume( lp );

                // note: split invalidated iterator
                llvm::IRBuilder<> irb( unwindBB->getFirstInsertionPt() );
                auto *sel = irb.CreateExtractValue( lp, ehi.selectorIndices, "lart.cleanup.lb.sel" );
                auto *cmp = irb.CreateICmpNE( sel, llvm::ConstantInt::get(
                            llvm::cast< llvm::IntegerType >( sel->getType() ), ehi.cleanupSelector ) );
                irb.CreateCondBr( cmp, noCleanupBB, cleanupBB );
                unwindBB->rbegin()->eraseFromParent(); // br added by splitBasicBlock
            }
        } else {
            auto *inst = llvm::cast< llvm::CallInst >( cs.getInstruction() );
            invokeAdded = true;

            auto *cleanupBB = llvm::BasicBlock::Create( fn.getParent()->getContext(), "lart.cleanup.invoke.unwind", &fn );
            llvm::IRBuilder<> irb( cleanupBB );
            auto *lp = irb.CreateLandingPad( ehi.landingpadType, 0 );
            lp->setCleanup( true );
            irb.CreateResume( lp );

            auto split = std::next( llvm::BasicBlock::iterator( inst ) );
            auto *bb = inst->getParent();
            auto *invokeOkBB = bb->splitBasicBlock( split, "lart.cleanup.invoke.ok" );

            std::vector< llvm::Value * > args( inst->arg_operands().begin(), inst->arg_operands().end() );
            auto *invoke = llvm::InvokeInst::Create( inst->getCalledValue(), invokeOkBB, cleanupBB, args );
            llvm::ReplaceInstWithInst( inst, invoke );
            bb->rbegin()->eraseFromParent(); // br added by splitBasicBlock
        }
    }

    if ( invokeAdded && !fn.hasPersonalityFn() )
        fn.setPersonalityFn( ehi.personality );
}

template< typename AtExit >
void atExits( llvm::Function &fn, AtExit &&atExit ) {
    using query::operator&&;
    using query::operator||;
    auto exits = query::query( fn ).flatten()
            .map( query::refToPtr )
            // exits are ret and resume
            .filter( query::is< llvm::ReturnInst > || query::is< llvm::ResumeInst > )
            .freeze();
    for ( auto *exit : exits )
        atExit( exit );
}

template< typename ShouldClean, typename Cleanup >
void addAllocaCleanups( EhInfo ehi, llvm::Function &fn, ShouldClean &&shouldClean, Cleanup &&cleanup,
        analysis::Reachability *reach = nullptr, llvm::DominatorTree *dt = nullptr )
{
    if ( fn.empty() )
        return;

    llvm::Optional< analysis::BasicBlockSCC > localSCC;
    llvm::Optional< analysis::Reachability > localReach;
    if ( !reach ) {
        localSCC.emplace( fn );
        localReach.emplace( fn, localSCC.getPointer() );
        reach = localReach.getPointer();
    }
    llvm::Optional< llvm::DominatorTree > localDT;
    if ( !dt ) {
        localDT.emplace( llvm::DominatorTreeAnalysis().run( fn ) );
        dt = localDT.getPointer();
    }

    std::vector< llvm::AllocaInst * > allocas = query::query( fn ).flatten()
        .map( query::llvmdyncast< llvm::AllocaInst > )
        .filter( query::notnull ).filter( shouldClean ).freeze();
    if ( allocas.empty() )
        return;

    makeExceptionsVisible( ehi, fn, [&]( llvm::CallSite &cs ) {
        return !query::query( allocas )
                    .filter( [&]( llvm::AllocaInst *al ) { return reach->reachable( al, cs.getInstruction() ); } )
                    .empty();
    } );

    // phase 2: propagate all allocas which might reach bb into it using phi nodes
    struct Local { };
    using AllocaIdentity = brick::types::Either< Local, std::pair< util::Set< llvm::BasicBlock * >, llvm::PHINode * > >;
    util::Map< std::pair< llvm::BasicBlock *, llvm::AllocaInst * >, AllocaIdentity > allocaMap;
    util::Map< llvm::BasicBlock *, util::StableSet< llvm::AllocaInst * > > reachingAllocas;
    std::vector< llvm::BasicBlock * > worklist;
    worklist.push_back( &fn.getEntryBlock() );

    auto allocaReprInBB = [&]( llvm::AllocaInst *alloca, llvm::BasicBlock *bb ) -> llvm::Instruction * {
        if ( alloca->getParent() == bb )
            return alloca;
        auto &id = allocaMap[ { bb, alloca } ].right();
        if ( id.second )
            return id.second;
        return alloca;
    };

    while ( !worklist.empty() ) {
        auto bb = worklist.back();
        worklist.pop_back();

        bool propagate = false;
        auto &reachingHere = reachingAllocas[ bb ];

        // union predecessors
        for ( auto &pbb : util::preds( bb ) ) {
            if ( &pbb == bb )
                continue;
            for ( auto alloca : reachingAllocas[ &pbb ] ) {
                if ( reachingHere.insert( alloca ) )
                    propagate = true;

                bool addToPhi = false;
                auto it = allocaMap.find( { bb, alloca } );
                if ( it == allocaMap.end() ) {
                    allocaMap[ { bb, alloca } ] = std::pair< util::Set< llvm::BasicBlock * >, llvm::PHINode * >{ { &pbb }, nullptr };
                    addToPhi = true;
                } else if ( it->second.isRight() ) {
                    addToPhi = it->second.right().first.insert( &pbb ).second;
                } else {
                    ASSERT( alloca->getParent() == bb );
                }

                if ( addToPhi && !dt->dominates( alloca, bb ) ) {
                    llvm::PHINode *&phi = allocaMap[ { bb, alloca } ].right().second;
                    if ( !phi )
                        phi = llvm::IRBuilder<>( bb, bb->begin() ).CreatePHI( alloca->getType(), 0 );
                    phi->addIncoming( allocaReprInBB( alloca, &pbb ), &pbb );
                }
            }
        }

        // allocas here
        for ( auto &inst : *bb ) {
            if ( auto *alloca = llvm::dyn_cast< llvm::AllocaInst >( &inst ) ) {
                if ( reachingHere.insert( alloca ) ) {
                    propagate = true;
                    allocaMap[ { bb, alloca } ] = Local();
                }
            }
        }

        if ( propagate )
            std::copy( llvm::succ_begin( bb ), llvm::succ_end( bb ), std::back_inserter( worklist ) );
    }

    // predecessors from which alloca cannot be obtained must be represented by null
    for ( auto &bb : fn ) {
        for ( auto *alloca : reachingAllocas[ &bb ] ) {
            auto &id = allocaMap[ { &bb, alloca } ];
            if ( id.isRight() && id.right().second ) {
                llvm::PHINode *phi = id.right().second;
                for ( auto &pbb : util::preds( bb ) ) {
                    if ( id.right().first.count( &pbb ) == 0 )
                        phi->addIncoming( llvm::ConstantPointerNull::get( alloca->getType() ), &pbb );
                }
            }
        }
    }

    // reorder all phi nodes to have same order of arguments
    std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > saved;
    for ( auto &bb : fn ) {
        const auto *phi0 = llvm::dyn_cast< llvm::PHINode >( &*bb.begin() );
        if ( !phi0 )
            continue;
        int n = phi0->getNumIncomingValues();

        llvm::PHINode *phi;
        for ( auto it = std::next( bb.begin() );
              ( phi = llvm::dyn_cast< llvm::PHINode >( &*it ) );
              ++it )
        {
            saved.clear();
            ASSERT_EQ( phi->getNumIncomingValues(), n );
            for ( int i = 0; i < n; ++i ) {
                saved.emplace_back( phi->getIncomingValue( i ), phi->getIncomingBlock( i ) );
            }
            for ( int i = 0; i < n; ++i ) {
                auto refidx = phi0->getBasicBlockIndex( saved[ i ].second );
                phi->setIncomingValue( refidx, saved[ i ].first );
                phi->setIncomingBlock( refidx, saved[ i ].second );
            }
        }
    }

    // phase 3: clean all allocas that are visible at function exit point
    atExits( fn, [&]( llvm::Instruction *exit ) {
        auto exitbb = exit->getParent();
        auto reaching = query::query( reachingAllocas[ exitbb ] )
                  .map( [&]( llvm::AllocaInst *alloca ) { return allocaReprInBB( alloca, exitbb ); } )
                  .freeze();
        cleanup( exit, reaching );
    } );
}

}
}

#endif // LART_SUPPORT_CLEANUP_H
