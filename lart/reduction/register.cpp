// -*- C++ -*- (c) 2015-2016 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/IR/Dominators.h>

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/analysis/bbreach.h>

#include <brick-assert>
#include <lart/support/query.h>
#include <lart/support/util.h>

#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace lart {
namespace reduction {

struct DeadRegisterZeoring : lart::Pass {

    static PassMeta meta() {
        return passMeta< DeadRegisterZeoring >( "DeadRegisterZeoring", "Zero registers and free allocas after last use." );
    }

    void runFn( llvm::Function &fn ) {
        if ( fn.empty() )
            return;

        auto dt = llvm::DominatorTreeAnalysis().run( fn );
        analysis::BasicBlockSCC sccs( fn );
        analysis::Reachability reach( fn, &sccs );

        auto insts = query::query( fn ).flatten().map( query::refToPtr )
                        .filter( []( llvm::Value *v ) { return !v->getType()->isVoidTy(); } )
                        .freeze();
        for ( auto *i : insts ) {
            bool alloca = llvm::isa< llvm::AllocaInst >( i );
            if ( alloca ) {
                // check if the alloca is not aliased
                bool safe = query::query( i->users() )
                    .all( []( llvm::Value *v ) {
                        bool ok = true;
                        llvmcase( v,
                            [&]( llvm::StoreInst *st ) { ok = st->getPointerOperand() == v; },
                            [&]( llvm::AtomicRMWInst *armw ) { ok = armw->getPointerOperand() == v; },
                            [&]( llvm::AtomicCmpXchgInst *xchg ) { ok = xchg->getPointerOperand() == v; },
                            [&]( llvm::DbgInfoIntrinsic * ) { ok = true; }, // DbgInfoIntrinsic is call, so it needs to be mentioned explicitly before case for call
                            [&]( llvm::CallInst * ) { ok = false; },
                            [&]( llvm::InvokeInst * ) { ok = false; },
                            [&]( llvm::CastInst * ) { ok = false; },
                            [&]( llvm::PHINode * ) { ok = false; } );
                        return ok;
                    } );
                if ( !safe )
                    continue;
            }

            auto inserts = query::query( i->users() )
                .map( query::llvmdyncast< llvm::Instruction > )
                .filter( [&]( llvm::Instruction *v ) {
                    return v && query::query( i->users() )
                        .map( query::llvmdyncast< llvm::Instruction > )
                        .filter( [&]( llvm::Instruction *w ) { return w != v; } )
                        .all( [&]( llvm::Instruction *w ) {
                            return (sccs[ v->getParent() ]->cyclic() && sccs[ v->getParent() ] == sccs[ w->getParent() ])
                                || !reach.strictlyReachable( v, w );
                        } );
                } )
                .map( [&]( llvm::Instruction *u ) {
                    auto *scc = sccs[ u->getParent() ];
                    if ( scc->cyclic() )
                        return query::query( scc->basicBlocks() )
                            .map( [&]( llvm::BasicBlock *bb ) {
                                return query::query( util::fixLLVMIt( succ_begin( bb ) ),
                                                     util::fixLLVMIt( succ_end( bb ) ) )
                                    .filter( [&]( llvm::BasicBlock &x ) {
                                        return sccs[ &x ] != scc;
                                    } );
                            } )
                            .flatten()
                            .map( []( llvm::BasicBlock &bb ) { return &*bb.getFirstInsertionPt(); } )
                            .filter( [&]( llvm::Instruction *ip ) { return dt.dominates( i, ip ); } )
                            .freeze();
                    else if ( llvm::isa< llvm::TerminatorInst >( u ) || llvm::isa< llvm::PHINode >( u ) )
                        return query::owningQuery( std::vector< llvm::Instruction * >{ } ).freeze();
                    else
                        return query::owningQuery( std::vector< llvm::Instruction * >{
                                std::next( llvm::BasicBlock::iterator( u ) )
                            } )
                            .freeze();
                } )
                .flatten()
                .unstableUnique()
                .freeze();
            if ( i->getNumUses() == 0 ) {
                ASSERT( inserts.empty() );
                if ( !llvm::isa< llvm::TerminatorInst >( i ) && !llvm::isa< llvm::PHINode >( i ) )
                    inserts.push_back( std::next( llvm::BasicBlock::iterator( i ) ) );
            }
            for ( auto *ip : inserts ) {
                llvm::IRBuilder<> irb( ip );
                if ( alloca ) {
                    auto cast = irb.CreateBitCast( i, llvm::Type::getInt8PtrTy( fn.getParent()->getContext() ) );
                    irb.CreateCall( _free, { cast } );
                    if ( i != cast )
                        irb.CreateCall( getDrop( cast->getType() ), { cast } );
                }
                irb.CreateCall( getDrop( i->getType() ), { i } );
            }
        }
    }


    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) override {
        util::Timer t( "DeadRegisterZeoring" );
        _module = &m;
        _free = m.getFunction( "__divine_free" );
        ASSERT( _free );
        for ( auto &f : m )
            runFn( f );
        return llvm::PreservedAnalyses::none();
    }

    llvm::Function *getDrop( llvm::Type *ty ) {
        auto it = _drops.find( ty );
        if ( it != _drops.end() )
            return it->second;
        auto &ctx = _module->getContext();
        auto *dropty = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), { ty }, false );
        return _drops[ ty ] = llvm::Function::Create( dropty, llvm::GlobalValue::ExternalLinkage, "__divine_drop_register." + std::to_string( _drops.size() ), _module );
    }


    llvm::Module *_module;
    llvm::Function *_free;
    std::unordered_map< llvm::Type *, llvm::Function * > _drops;
};

PassMeta registerPass() {
    return compositePassMeta< DeadRegisterZeoring >( "register",
        "Optimize register use." );
}

} // namespace reduce
} // namespace lart
