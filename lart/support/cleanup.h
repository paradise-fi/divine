// -*- C++ -*- (c) 2015-2018 Vladimír Štill <xstill@fi.muni.cz>

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
            llvm::StructType::get( ctx, { llvm::Type::getInt8PtrTy( ctx ),
                                          llvm::Type::getInt32Ty( ctx ) } ),
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
                llvm::IRBuilder<> irb( &*unwindBB->getFirstInsertionPt() );
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

using AllocaReprMap = util::Map< std::pair< llvm::AllocaInst *, llvm::BasicBlock * >, llvm::Instruction * >;
using ExitMap = util::Map< llvm::Instruction *, std::vector< llvm::AllocaInst * > >;

struct AllocaPropagator
{
    AllocaPropagator( AllocaReprMap &repr, llvm::AllocaInst *al ) : repr( repr ), al( al ) { }

    void propagate( llvm::Instruction *inst, llvm::BasicBlock *from, llvm::BasicBlock *to )
    {
        auto i = repr.try_emplace( { al, to }, nullptr );
        if ( !i.second )
            return; // done

        auto &repr_here = i.first->second;

        if ( std::next( llvm::pred_begin( to ) ) == llvm::pred_end( to )
                  && *llvm::pred_begin( to ) == from )
            repr_here = inst;
        else {
            ASSERT( !repr_here );

            int n = std::distance( llvm::pred_begin( to ), llvm::pred_end( to ) );
            llvm::PHINode *phi;

            // insert *after* existing PHI nodes so that phiord_preds can respect their
            // argument order
            repr_here = phi = llvm::PHINode::Create( al->getType(), n, "lart.alloca.phi",
                                                     to->getFirstNonPHI() );
            todo_phi.emplace_back( al, phi );
        }
        for ( auto &nxt : util::succs( to ) )
            propagate( repr_here, to, &nxt );
    }

    void finalize() {
        for ( auto p : todo_phi ) {
            auto *al = p.first;
            auto *const repr_here = p.second;
            auto *bb = repr_here->getParent();

            for ( auto &pred : util::phiord_preds( bb ) ) {
                auto it = repr.find( { al, &pred } );
                llvm::Value *val;
                if ( it == repr.end() )
                    val = llvm::ConstantPointerNull::get( llvm::cast< llvm::PointerType >(
                                                                  repr_here->getType() ) );
                else
                    val = it->second;
                repr_here->addIncoming( val, &pred );
            }
        }
    }

    AllocaReprMap &repr;
    llvm::AllocaInst *al;
    std::vector< std::pair< llvm::AllocaInst *, llvm::PHINode * > > todo_phi;
};

static AllocaReprMap propagateAllocas( const util::Set< llvm::AllocaInst * > &allocas )
{
    AllocaReprMap repr;

    for ( auto *al : allocas ) {
        AllocaPropagator prop( repr, al );

        auto *start = al->getParent();
        repr[ { al, start } ] = al;
        for ( auto &nxt : util::succs( start ) )
            prop.propagate( al, start, &nxt );

        prop.finalize();
    }

    return repr;
}

template< typename ShouldClean, typename Cleanup >
void addAllocaCleanups( EhInfo ehi, llvm::Function &fn, ShouldClean &&shouldClean, Cleanup &&cleanup )
{
    if ( fn.empty() )
        return;

    std::vector< llvm::AllocaInst * > allocas = query::query( fn ).flatten()
        .map( query::llvmdyncast< llvm::AllocaInst > )
        .filter( query::notnull ).filter( shouldClean ).freeze();
    if ( allocas.empty() )
        return;

    analysis::BasicBlockSCC scc( fn );
    analysis::Reachability reach( fn, &scc );
    makeExceptionsVisible( ehi, fn, [&]( llvm::CallSite &cs ) {
        return !query::query( allocas )
                    .filter( [&]( llvm::AllocaInst *al ) { return reach.reachable( al, cs.getInstruction() ); } )
                    .empty();
    } );

    // we need to take into account new basic blocks
    scc = analysis::BasicBlockSCC( fn );
    reach = analysis::Reachability( fn, &scc );
    llvm::AnalysisManager< llvm::Function > am;
    llvm::DominatorTree dt = llvm::DominatorTreeAnalysis().run( fn, am );

    ExitMap exits;
    util::Set< llvm::AllocaInst * > nondominant;
    llvm::Instruction *start = &*fn.getEntryBlock().begin();

    atExits( fn, [&]( auto *exit ) { exits[ exit ] = {}; } );

    for ( auto &p : exits ) {
        auto *exit = p.first;
        if ( !reach.reachable( start, exit ) )
            continue;

        for ( auto *al : allocas ) {
            if ( reach.reachable( al, exit ) ) {
                p.second.push_back( al );
                if ( !dt.dominates( al, exit ) ) {
                    nondominant.insert( al );
                }
            }
        }
    }

    AllocaReprMap repr;
    if ( !nondominant.empty() )
        repr = propagateAllocas( nondominant );

    // clean all allocas that are visible at function exit point
    for ( auto &exit : exits ) {
        auto exitbb = exit.first->getParent();
        auto reaching = query::query( exit.second )
                  .map( [&]( auto *alloca ) {
                      auto it = repr.find( { alloca, exitbb } );
                      if ( it == repr.end() )
                          return static_cast< llvm::Instruction * >( alloca );
                      return it->second;
                  } )
                  .freeze();
        cleanup( exit.first, reaching );
    }
}

}
}

#endif // LART_SUPPORT_CLEANUP_H
