// -*- C++ -*- (c) 2015, 2018 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
// #include <llvm/Transforms/Utils/BasicBlockUtils.h>
// #include <llvm/Transforms/Utils/Cloning.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-string>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/support/cleanup.h>

namespace lart {
namespace svcomp {

#ifndef NDEBUG
#define LART_DEBUG( X ) X
#else
#define LART_DEBUG( X ) ((void)0)
#endif

struct Atomic
{
    static PassMeta meta() {
        return passMeta< Atomic >( "atomic", "" );
    }

    void run( llvm::Module &m ) {
        auto vbegin = m.getFunction( "__VERIFIER_atomic_begin" ),
             vend = m.getFunction( "__VERIFIER_atomic_end" ),
             dios_mask = m.getFunction( "__dios_mask" );

        if ( !dios_mask )
            UNREACHABLE( "__dios_mask not found" );

        llvm::StringRef atomicPrefix( "__VERIFIER_atomic_" );

        for ( auto &f : m ) {
            if ( &f != vbegin && &f != vend && !f.empty()
                && f.getName().startswith( atomicPrefix ) )
            {
                llvm::IRBuilder<> irb( &*f.getEntryBlock().getFirstInsertionPt() );
                irb.CreateCall( dios_mask, { irb.getInt32( 1 ) } );
                irb.CreateFence( llvm::AtomicOrdering::SequentiallyConsistent );
                cleanup::atExits( f, [dios_mask]( llvm::Instruction *exit ) {
                    llvm::IRBuilder<> irb( exit );
                    irb.CreateFence( llvm::AtomicOrdering::SequentiallyConsistent );
                    irb.CreateCall( dios_mask, { irb.getInt32( 0 ) } );
                } );
            }
        }
    }
};

struct NondetTracking {

    enum class Signed { No = 0, Yes = 1 };

    static PassMeta meta() {
        return passMeta< NondetTracking >( "nondeterminism-tracking", "" );
    }

    void track( llvm::Function &fn ) {
        auto calls = query::query( fn ).flatten()
                          .map( []( auto &i ) { return llvm::CallSite( &i ); } )
                          .filter( [this]( auto &cs ) {
                              if ( !cs )
                                  return false;
                              auto *fn = llvm::dyn_cast< llvm::Function >(
                                              cs.getCalledValue()->stripPointerCasts() );
                              return fn && fn->getName().startswith( _nondet );
                          } )
                          .freeze();

        for ( auto &cs : calls ) {
            if ( cs->user_begin() == cs->user_end() ) {
                cs->eraseFromParent();
                continue;
            }
            if ( std::next( cs->user_begin() ) != cs->user_end() )
                continue;

            auto *i = llvm::dyn_cast< llvm::Instruction >( *cs->user_begin() );
            if ( !i )
                continue;

            if ( auto *icmp = llvm::dyn_cast< llvm::CmpInst >( i ) ) {
                if ( icmp->isEquality() ) {
                    llvm::IRBuilder<> irb( icmp );
                    auto choose = irb.CreateCall( _choice, { val( 2 ) } );
                    llvm::ReplaceInstWithInst( icmp, new llvm::TruncInst( choose, irb.getInt1Ty() ) );
                    cs->eraseFromParent();
                }
            }
            else if ( auto *sw = llvm::dyn_cast< llvm::SwitchInst >( i ) ) {
                ASSERT_EQ( sw->getCondition(), cs.getInstruction() );
                auto succs = sw->getNumSuccessors();
                auto cases = sw->getNumCases();
                ASSERT_EQ( succs - 1, cases );
                std::vector< llvm::BasicBlock * > succ_bbs;
                for ( unsigned i = 0; i < succs; ++i ) {
                    auto s = sw->getSuccessor( i );
                    if ( s != sw->getDefaultDest() )
                        succ_bbs.push_back( s );
                }
                while ( sw->getNumCases() )
                    sw->removeCase( sw->case_begin() );

                for ( unsigned i = 0; i < cases; ++i )
                    sw->addCase( val( i ), succ_bbs[i] );

                llvm::IRBuilder<> irb( sw );
                sw->setCondition( irb.CreateCall( _choice, { val( succs ) } ) );
                cs->eraseFromParent();
            }
        }
    }

    llvm::ConstantInt *val( int v ) {
        return llvm::cast< llvm::ConstantInt >( llvm::ConstantInt::get( _ctype, v ) );
    }

    void run( llvm::Module &m ) {
        _nondet = "__VERIFIER_nondet_";
        _dl = std::make_unique< llvm::DataLayout >( &m );
        _ctx = &m.getContext();
        _choice = m.getFunction( "__vm_choose" );
        _ctype = _choice->getFunctionType()->getParamType( 0 );

        for ( auto &fn : m )
            track( fn );
    }

  private:
    std::unique_ptr< llvm::DataLayout > _dl;
    llvm::LLVMContext *_ctx;
    llvm::StringRef _nondet;
    llvm::Function *_choice;
    llvm::Type *_ctype;
};

PassMeta svcompPass() {
    return compositePassMeta< Atomic, NondetTracking >( "svcomp", "" );
}

#undef LART_DEBUG

}
}
