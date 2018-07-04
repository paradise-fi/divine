// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/CaptureTracking.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/analysis/bbreach.h>

#include <brick-assert>
#include <lart/support/query.h>

#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace lart {
namespace reduction {

struct DeadAllocaZeoring {

    static PassMeta meta() {
        return passMeta< DeadAllocaZeoring >( "DeadAllocaZeoring", "Zero allocas after last use" );
    }

    void runFn( llvm::Function &fn ) {
        auto allocas = query::query( fn ).flatten()
                          .map( query::llvmdyncast< llvm::AllocaInst > )
                          .filter( query::notnull )
                          .forall( [&]( auto & ) { ++_allocas; } )
                          .filter( [&]( llvm::AllocaInst *al ) { return !llvm::PointerMayBeCaptured( al, false, true ); } )
                          .forall( [&]( auto & ) { ++_nonescaping; } )
                          .freeze();

        if ( allocas.empty() )
            return;

        auto zerostore = [&]( llvm::IRBuilder<> &irb, llvm::AllocaInst *al ) {
            llvm::Value *siv = nullptr;
            if ( auto cnt = llvm::dyn_cast< llvm::ConstantInt >( al->getArraySize() ) )
                siv = llvm::ConstantInt::get( _mzeroSiT, _dl->getTypeAllocSize( al->getAllocatedType() ) * cnt->getZExtValue() );
            else {
                llvm::Value *cntv = al->getArraySize();
                if ( cntv->getType() != _mzeroSiT )
                    cntv = irb.CreateIntCast( cntv, _mzeroSiT, false );
                siv = irb.CreateAdd(
                        llvm::ConstantInt::get( _mzeroSiT, _dl->getTypeAllocSize( al->getAllocatedType() ) ),
                        cntv );
            }

            llvm::Value *ptr = al;
            if ( al->getType() != _mzeroPtrT )
                ptr = irb.CreateBitCast( al, _mzeroPtrT );

            irb.CreateCall( _mzero, llvm::ArrayRef< llvm::Value * >( { ptr, siv } ) );
        };

        analysis::BasicBlockSCC sccs( fn );
        analysis::Reachability reach( fn, &sccs );

        std::set< std::pair< llvm::Instruction *, llvm::AllocaInst * > > insertPoints;

        for ( auto alloca : allocas ) {

            auto uses = query::query( alloca->users() )
                            .map( query::llvmdyncast< llvm::Instruction > )
                            .filter( query::notnull && query::isnot< llvm::DbgDeclareInst > )
                            .template freezeAs< std::set< llvm::Instruction * > >();

            if ( !query::query( uses ).all( query::is< llvm::StoreInst > || query::is< llvm::LoadInst > ) )
                continue; // TODO: relax

            uses.insert( alloca ); // self reachablity needs to be checked too

            bool zeroed = false;
            // insert instruction after any last use into insertpoints
            query::query( uses ).filter( [&]( llvm::Instruction *use ) {
                    // drop all uses from which any other use can be reached
                    // this includes uses on cycles
                    return !query::query( uses ).any( [&]( llvm::Instruction *later ) {
                        return reach.strictlyReachable( use, later );
                    } );
                } ).forall( [&]( llvm::Instruction *enduse ) {
                    auto insPt = std::next( llvm::BasicBlock::iterator( enduse ) );
                    if ( _meaningfullInsertPoint( &*insPt ) ) {
                        insertPoints.emplace( &*insPt, alloca );
                        zeroed = true;
                    }
                } );

            // now we have to deal with cases when there is no last use, that is uses on cycles
            query::query( uses )
                .filter( [&]( llvm::Instruction *use ) { // filter instructions only on cycles
                    return sccs[ use->getParent() ]->cyclic();
                } )
                .map( [&]( llvm::Instruction *use ) { // transform instructions -> SCCs
                    return sccs[ use->getParent() ];
                } )
                .unstableUnique() // we can have more uses from one SCC
                .concatMap( [&]( const analysis::BasicBlockSCC::SCC *scc ) {
                    // get successor blocks of SCC which lie outside of SCC
                    return query::query( scc->basicBlocks() )
                            .concatMap( []( llvm::BasicBlock *bb ) { return util::succs( bb ); } )
                            .map( query::refToPtr )
                            // scc must be caught by value! It will be used after this returns
                            .filter( [&sccs, scc]( llvm::BasicBlock *bb ) {
                                        return sccs[ bb ] != scc;
                                    } );
                } )
                .unstableUnique() // we can have duplicate blocks
                .filter( [&]( llvm::BasicBlock *zeroing ) {
                    // drop all blocks from which any use can be reached
                    return !query::query( uses ).any( [&]( llvm::Instruction *later ) {
                        return reach.reachable( zeroing, later->getParent() );
                    } );
                } )
                .forall( [&]( llvm::BasicBlock *bb ) {
                    // get first possible insert point in each bb as insert point for zeroing
                    auto insPt = &*bb->getFirstInsertionPt();
                    if ( _meaningfullInsertPoint( insPt ) ) {
                        insertPoints.emplace( insPt, alloca );
                        zeroed = true;
                    }
                } );

                if ( zeroed )
                    ++_zeroed;
        }

        for ( auto ipt : insertPoints ) {
            llvm::IRBuilder<> irb( ipt.first );
            zerostore( irb, ipt.second );
        }
    }

    static std::unordered_set< llvm::Instruction * > reachable( llvm::Instruction *i ) {
        std::unordered_set< llvm::Instruction * > seen;

        // care must be taken that we include all instructions from this block
        // if it is on a cycle, for this reason, we start DFS from all successors
        // of parent block of this instruction
        auto bb = query::query( *i->getParent() ).map( query::refToPtr );
        auto beg = std::find( bb.begin(), bb.end(), i );
        ASSERT( beg != bb.end() );
        seen.insert( beg, bb.end() );

        for ( auto &b : util::succs( i->getParent() ) )
            bbPreorder( b, [&]( llvm::BasicBlock *bb ) {
                    for ( llvm::Instruction &i : *bb )
                        seen.insert( &i );
                } );
        return seen;
    }

    llvm::Module *_m;

    void run( llvm::Module &m ) {
        _m = &m;
        _dl = std::make_unique< llvm::DataLayout >( &m );
        _mzero = _mkmzero( m );
        for ( auto &fn : m )
            if ( &fn != _mzero )
                runFn( fn );
        std::cerr << "INFO: zeroed " << _zeroed << " out of " << _allocas << " allocas ("
                  << double( _zeroed * 100 ) / _allocas << "%), (" << _nonescaping << " nonescaping)" << std::endl;
    }

    llvm::Function *_mkmzero( llvm::Module &m ) {

        auto &ctx = m.getContext();
        _mzeroPtrT = llvm::Type::getInt8PtrTy( ctx );
        _mzeroSiT = llvm::Type::getInt64Ty( ctx );
        auto mzerotype = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), llvm::ArrayRef< llvm::Type * >( { _mzeroPtrT, _mzeroSiT } ), false );
        auto mzero = llvm::cast< llvm::Function >( m.getOrInsertFunction( "lart.alloca.mzero", mzerotype ) );
        if ( !mzero->empty() )
            return mzero; // already present

        auto args = mzero->arg_begin();
        llvm::Value *ptrarg = &*args++;
        ptrarg->setName( "ptr" );
        llvm::Value *siarg = &*args;
        siarg->setName( "si" );

        auto *ret = llvm::BasicBlock::Create( ctx, "ret", mzero );
        auto *unlock = llvm::BasicBlock::Create( ctx, "unlock", mzero, ret );
        auto *end = llvm::BasicBlock::Create( ctx, "end", mzero, unlock );
        auto *body = llvm::BasicBlock::Create( ctx, "body", mzero, end );
        auto *entry = llvm::BasicBlock::Create( ctx, "entry", mzero, body );

        llvm::IRBuilder<> irb( entry );
        auto *mask = irb.CreateCall( m.getFunction( "__divine_interrupt_mask" ), { } );
        auto *shouldUnlock = irb.CreateICmpEQ( mask, llvm::ConstantInt::get( mask->getType(), 0 ), "shouldUnlock" );
        auto *undefAlloca = irb.CreateAlloca( llvm::Type::getInt8Ty( ctx ) );
        auto *undef = irb.CreateLoad( undefAlloca, "undefval" );
        irb.CreateBr( body );

        irb.SetInsertPoint( end );
        irb.CreateCondBr( shouldUnlock, unlock, ret );

        irb.SetInsertPoint( ret );
        irb.CreateRetVoid();

        irb.SetInsertPoint( unlock );
        irb.CreateCall( m.getFunction( "__divine_interrupt_unmask" ), { } );
        irb.CreateRetVoid();

        irb.SetInsertPoint( body );

        auto ptrphi = irb.CreatePHI( ptrarg->getType(), 2 );
        ptrphi->addIncoming( ptrarg, entry );
        auto siphi = irb.CreatePHI( siarg->getType(), 2 );
        siphi->addIncoming( siarg, entry );

        irb.CreateStore( undef, ptrphi );
        auto ptrnext = irb.CreateGEP( ptrphi, llvm::ConstantInt::get( llvm::Type::getInt64Ty( ctx ), 1 ) );
        auto sinext = irb.CreateAdd( siphi, llvm::ConstantInt::get( _mzeroSiT, -1 ) );
        ptrphi->addIncoming( ptrnext, body );
        siphi->addIncoming( sinext, body );
        auto isend = irb.CreateICmpEQ( sinext, llvm::ConstantInt::get( _mzeroSiT, 0 ) );
        irb.CreateCondBr( isend, end, body );

        // mzero->dump();
        return mzero;
    }

  private:
    std::unique_ptr< llvm::DataLayout > _dl;
    llvm::Function *_mzero;
    llvm::Type *_mzeroPtrT;
    llvm::Type *_mzeroSiT;
    long _zeroed = 0;
    long _nonescaping = 0;
    long _allocas = 0;

    bool _meaningfullInsertPoint( llvm::Instruction *ins ) {
        return !llvm::isa< llvm::ReturnInst >( ins ) && !llvm::isa< llvm::ResumeInst >( ins );
    }
};

PassMeta allocaPass() {
    return compositePassMeta< DeadAllocaZeoring >( "alloca",
        "Optimize alloca use. Deprecated: use register pass instead." );
}

} // namespace reduce
} // namespace lart
