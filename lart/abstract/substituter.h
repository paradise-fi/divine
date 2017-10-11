// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/common.h>
#include <lart/abstract/domains/tristate.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>
#include <lart/abstract/data.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

template< typename TMap >
bool isAGEPCast( llvm::CastInst * bc, TMap & tmap ) {
    return isAbstract( bc->getDestTy(), tmap ) &&
           !isAbstract( bc->getSrcTy(), tmap ) &&
           llvm::isa< llvm::GetElementPtrInst >( bc->getOperand( 0 ) );
}


template< typename TMap, typename Fns >
struct Substituter {
    using IRB = llvm::IRBuilder<>;

    using Abstraction = std::shared_ptr< Common >;

    Substituter( TMap & tmap, Fns & fns ) : tmap( tmap ), fns( fns ) {}

    ~Substituter() {
        for ( const auto & v : vmap ) {
            if ( auto i = llvm::dyn_cast< llvm::Instruction >( v.first ) ) {
                i->replaceAllUsesWith( llvm::UndefValue::get( i->getType() ) );
                i->eraseFromParent();
            }
        }
    }

    void process( llvm::Argument * arg ) {
        vmap[ arg ] = argLift( arg );
    }

    void process( llvm::Value * val ) {
        if ( llvm::isa< llvm::Argument >( val ) ) {
            UNREACHABLE( "Arguments should be substituted before processing of values." );
        } else {
            llvmcase( val
            ,[&]( llvm::CastInst * i ) { doCast( i ); }
            ,[&]( llvm::BranchInst * i ) { doBr( i ); }
            ,[&]( llvm::PHINode * i ) { doPhi( i ); }
            ,[&]( llvm::ReturnInst * i ) { doRet( i ); }
            ,[&]( llvm::CallInst * c ) {
                return isIntrinsic( c ) ? doIntrinsic( c ) : doCallSite( c );
            }
            ,[&]( llvm::InvokeInst * i ) {
                return doCallSite( i );
            }
            ,[&]( llvm::Instruction *i ) {
                std::cerr << "ERR: unknown instruction: ";
                i->dump();
                std::exit( EXIT_FAILURE );
            } );
        }
    }

    llvm::Type * process( llvm::Type * type ) {
        if ( !isAbstract( type ) )
            return type;
        return getAbstraction( type )->abstract( type );
    }

    llvm::Function * prototype( llvm::Function * fn ) {
        auto rty = process( fn->getReturnType() );
        auto args = remapFn( fn->args(), [&] ( const auto & a ) { return process( a.getType() ); } );
        auto fty = llvm::FunctionType::get( rty, args, fn->getFunctionType()->isVarArg() );
        return llvm::Function::Create( fty, fn->getLinkage(), fn->getName(), fn->getParent() );
    }

    /* Add 'abstraction' to available abstractions for builder */
    void addAbstraction( Abstraction && a ) {
        abs.emplace( a->domain(), std::move( a ) );
    }

private:
    llvm::Value * argLift( llvm::Argument * arg ) {
        auto rty = process( arg->getType() );
        if ( arg->getType()->isPointerTy() )
            return IRB( getFunction( arg )->front().begin() ).CreateBitCast( arg, rty );

        auto dom = tmap.origin( stripPtrs( arg->getType() ) ).second;
        std::string name = "__lart_lift_" + DomainTable[ dom ];
        auto abstraction = abs[ dom ];
        auto fty = llvm::FunctionType::get( rty, { arg->getType() }, false );
        auto lift = getModule( arg )->getOrInsertFunction( name, fty );
        return IRB( getFunction( arg )->front().begin() ).CreateCall( lift, { arg } );
    }

    Types remap( const Types & tys ) const {
        return remapFn( tys, [&] ( const auto type ) {
            return isAbstract( type )
            ? getAbstraction( type )->abstract( tmap.origin( type ).first )
            : type;
        } );
    }

    template< typename Vs >
    Values remapArgs( const Vs & vals ) const {
        return remapFn( vals, [&] ( const auto & u ) {
            auto v = u.get();
            return isAbstract( v->getType() ) ? vmap.at( v ) : v;
        } );
    }

    void doBr( llvm::BranchInst * br ) {
        assert( br->isConditional() );
        if ( vmap.count( br ) )
            return;
        auto cond = vmap.find( br->getCondition() );
        if( cond != vmap.end() ) {
            auto tbb = br->getSuccessor( 0 );
            auto fbb = br->getSuccessor( 1 );
            vmap[ br ] = IRB( br ).CreateCondBr( cond->second, tbb, fbb );
        }
    }

    void doPhi( llvm::PHINode * phi ) {
        auto niv = phi->getNumIncomingValues();

        std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > incoming;
        for ( size_t i = 0; i < niv; ++i ) {
            auto val = llvm::cast< llvm::Value >( phi->getIncomingValue( i ) );
            assert( vmap.count( val ) );
            auto parent = phi->getIncomingBlock( i );
            incoming.push_back( { vmap[ val ], parent } );
        }

        if ( incoming.size() > 0 ) {
            llvm::PHINode * node = nullptr;
            if ( vmap.count( phi ) )
                node = llvm::cast< llvm::PHINode >( vmap[ phi ] );
            else {
                auto type = incoming.begin()->first->getType();
                node = IRB( phi ).CreatePHI( type, niv );
                vmap[ phi ] = node;
            }

            for ( size_t i = 0; i < node->getNumIncomingValues(); ++i )
                node->removeIncomingValue( i, false );
            for ( auto & in : incoming )
                node->addIncoming( in.first, in.second );
        }
    }

    void doCast( llvm::CastInst * cast ) {
        IRB irb( cast );
        auto destTy = process( cast->getDestTy() );
        auto op = cast->getOperand( 0 );
        auto val = vmap.count( op ) ? vmap[ op ] : op;
        if ( val->getType() != destTy ) {
            if ( isAGEPCast( cast, tmap ) )
                assert( val->getType()->getPrimitiveSizeInBits() ==
                        destTy->getPrimitiveSizeInBits() );
            vmap[ cast ] = irb.CreateBitCast( val, destTy );
        } else {
            vmap[ cast ] = val;
        }
    }

    void doRet( llvm::ReturnInst * ret ) {
        auto val = vmap.find( ret->getReturnValue() );
        if ( val != vmap.end() )
            vmap[ ret ] = IRB( ret ).CreateRet( val->second );
    }

    void doCallSite( const llvm::CallSite & cs ) {
        auto args = remapArgs( cs.args() );
        auto fn = cs.getCalledFunction();
        fn = fns.count( fn ) ? fns[ fn ] : fn;
        auto i = cs.getInstruction();
        if ( cs.isCall() ) {
            vmap[ i ] = IRB( i ).CreateCall( fn, args );
        } else if ( cs.isInvoke() ) {
            auto inv = llvm::dyn_cast< llvm::InvokeInst >( i );
            vmap[ i ] = IRB( i ).CreateInvoke( fn, inv->getNormalDest(), inv->getUnwindDest(), args );
        }
        if ( !isAbstract( i->getType() ) )
            i->replaceAllUsesWith( vmap[ i ] );
    }

    void doIntrinsic( llvm::CallInst * i ) {
        assert( isIntrinsic( i ) );
        auto abstraction = abs[ domain( i ) ];
        auto args = remapArgs( i->arg_operands() );
        vmap[ i ] =  abstraction->process( i, args );
    }

    const Abstraction getAbstraction( llvm::Value * v ) const {
        return getAbstraction( v->getType() );
    }

    const Abstraction getAbstraction( llvm::Type * t ) const {
        return abs.at( tmap.origin( stripPtr( t ) ).second );
    }

    bool isAbstract( llvm::Type * t ) const { return tmap.isAbstract( t ); }

    Map< Domain, Abstraction > abs;
    Map< llvm::Value *, llvm::Value * > vmap;

    TMap & tmap;
    Fns & fns;
};

} // namespace abstract
} // namespace lart
