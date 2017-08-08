// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substituter.h>

#include <lart/abstract/types/common.h>
#include <lart/abstract/domains/tristate.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

Substituter::Substituter() {
    registerAbstraction( std::make_shared< TristateDomain >() );
}

using Abstraction = Substituter::Abstraction;

bool Substituter::visited( llvm::Function * fn ) const {
    return processedFunctions.count( fn );
}

void Substituter::store( llvm::Function * f1, llvm::Function * f2 ) {
    processedFunctions[ f1 ] = f2;
}

void Substituter::process( llvm::Value * val ) {
    if ( llvm::isa< llvm::Argument >( val ) ) {
        processedValues[ val ] = val;
    } else {
        llvmcase( val,
        [&]( llvm::SelectInst * i ) {
            substituteSelect( i );
        },
        [&]( llvm::BranchInst * i ) {
            substituteBranch( i );
        },
        [&]( llvm::PHINode * i ) {
            substitutePhi( i );
        },
        [&]( llvm::CallInst * c ) {
            substituteCall( c );
        },
        [&]( llvm::CastInst * i ) {
            substituteCast( i );
        },
        [&]( llvm::ReturnInst * i ) {
            substituteReturn( i );
        },
        [&]( llvm::GetElementPtrInst * i ) {
            substituteGEP( i );
        },
        [&]( llvm::Instruction *i ) {
            std::cerr << "ERR: unknown instruction: ";
            i->dump();
            std::exit( EXIT_FAILURE );
        } );
    }
}

void Substituter::process( llvm::Function * fn ) {
    std::vector < llvm::Type * > types;
    for ( auto &arg : fn->args() )
        types.push_back( process( arg.getType() ) );
    auto rty = process( fn->getFunctionType()->getReturnType() );
    auto fty = llvm::FunctionType::get( rty, types, fn->getFunctionType()->isVarArg() );
    auto newfn = cloneFunction( fn, fty );
    assert( !processedFunctions.count( fn ) );
    for ( auto & arg : newfn->args() )
        if ( isSubstituted( arg.getType() ) )
            process( &arg );
    processedFunctions[ fn ] = newfn;
}

void Substituter::changeCallFunction( llvm::CallInst * call ) {
    llvm::IRBuilder<> irb( call );
    std::vector< llvm::Value * > args;
    for ( auto & arg : call->arg_operands() )
        args.push_back( arg );
    auto nfn = processedFunctions[ call->getCalledFunction() ];
    auto ncall = irb.CreateCall( nfn, args );
    processedValues[ call ] = ncall ;
}

void Substituter::substitutePhi( llvm::PHINode * phi ) {
    unsigned int niv = phi->getNumIncomingValues();

    std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > incoming;
    for ( unsigned int i = 0; i < niv; ++i ) {
        auto val = llvm::cast< llvm::Value >( phi->getIncomingValue( i ) );
        auto parent = phi->getIncomingBlock( i );
        if ( processedValues.count( val ) )
            incoming.push_back( { processedValues[ val ], parent } );
        else {
            if ( isSubstituted( val ) )
                incoming.push_back( { val, parent } );
        }
    }

    if ( incoming.size() > 0 ) {
        llvm::PHINode * node = nullptr;
        if ( processedValues.count( phi ) )
            node = llvm::cast< llvm::PHINode >( processedValues[ phi ] );
        else {
            llvm::IRBuilder<> irb( phi );
            auto type = incoming.begin()->first->getType();
            node = irb.CreatePHI( type, niv );
            processedValues[ phi ] = node;
        }

        for ( size_t i = 0; i < node->getNumIncomingValues(); ++i )
            node->removeIncomingValue( i,  false );
        for ( auto & in : incoming )
            node->addIncoming( in.first, in.second );
    }
}

void Substituter::substituteSelect( llvm::SelectInst * i ) {
    auto cond = processedValues.find( i->getCondition() );
    if( cond != processedValues.end() ) {
        auto tv = processedValues.find( i->getTrueValue() );
        auto fv = processedValues.find( i->getFalseValue() );
        if ( tv == processedValues.end() || fv == processedValues.end() )
            return;
        llvm::IRBuilder<> irb( i );
        auto newsel = irb.CreateSelect( cond->second, tv->second, fv->second );
        processedValues[ i ] = newsel;
    }
}

void Substituter::substituteBranch( llvm::BranchInst * br ) {
    assert( br->isConditional() );
    if ( processedValues.count( br ) )
        return;
    llvm::IRBuilder<> irb( br );
    auto cond = processedValues.find( br->getCondition() );
    if( cond != processedValues.end() ) {
        auto tbb = br->getSuccessor( 0 );
        auto fbb = br->getSuccessor( 1 );
        auto newbr = irb.CreateCondBr( cond->second, tbb, fbb );
        processedValues[ br ] = newbr;
    }
}

llvm::Type * Substituter::process( llvm::Type * type ) const {
    if ( isAbstract( type ) )
        return getAbstraction( type )->abstract( type );
    return type;
}

using Arguments = std::vector< llvm::Value * >;
Arguments callArgs( llvm::CallInst * call, const ValueMap & vmap ) {
	Arguments res;
    for ( auto &arg : call->arg_operands() ) {
        if ( isAbstract( arg->getType() ) && !vmap.count( arg ) )
            break;
        auto lowered = isAbstract( arg->getType() ) ? vmap.at( arg ) : arg;
        res.push_back( lowered );
    }
    return res;
}

void Substituter::substituteAbstractIntrinsic( llvm::CallInst * intr ) {
    assert( isIntrinsic( intr ) );

    auto args = callArgs( intr, processedValues );
    //skip if do not have enough substituted arguments
    if ( intr->getNumArgOperands() != args.size() )
        return;
    auto abst = abstractions[ Intrinsic( intr ).domain() ];
    processedValues[ intr ] = abst->process( intr, args );
}

void Substituter::substituteCall( llvm::CallInst * call ) {
    if ( isIntrinsic( call ) ) {
        substituteAbstractIntrinsic( call );
    } else {
	    auto args = callArgs( call, processedValues );
        //skip if do not have enough substituted arguments
        if ( call->getNumArgOperands() != args.size() )
            return;
        llvm::Module * m = call->getCalledFunction()->getParent();

        auto fn = m->getFunction( call->getCalledFunction()->getName() );
        fn = processedFunctions.count( fn ) ? processedFunctions[ fn ] : fn;
        assert ( fn != nullptr );

        llvm::IRBuilder<> irb( call );
        processedValues[ call ] = irb.CreateCall( fn, args );
    }
}

void Substituter::substituteCast( llvm::CastInst * cast ) {
    assert( llvm::isa< llvm::BitCastInst >( cast ) &&
           "ERR: Only bitcast is supported for pointer cast instructions." );

    llvm::IRBuilder<> irb( cast );
    auto destTy = process( cast->getDestTy() );
    auto val = processedValues[ cast->getOperand( 0 ) ];
    assert( val && "ERR: Trying to bitcast value, that is not abstracted." );

    processedValues[ cast ] = irb.CreateBitCast( val, destTy );
}

void Substituter::substituteReturn( llvm::ReturnInst * ret ) {
    auto val = processedValues.find( ret->getReturnValue() );
    if ( val != processedValues.end() ) {
        llvm::IRBuilder<> irb( ret );
        processedValues[ ret ] = irb.CreateRet( val->second );
    }
}

void Substituter::substituteGEP( llvm::GetElementPtrInst * gep ) {
    llvm::IRBuilder<> irb( gep );
    auto op = gep->getPointerOperand();
    assert( isAbstract( op->getType() ) );
    auto val = processedValues[ op ];
    std::vector< llvm::Value * > idxs = { gep->idx_begin(), gep->idx_end() };
    auto type = val->getType()->getScalarType()->getPointerElementType();
    processedValues[ gep ] = irb.CreateGEP( type, processedValues[ op ], idxs, gep->getName() );
}

void Substituter::clean( llvm::Function * fn ) {
    auto abstracted = query::query( processedValues )
        .map( [&]( const auto & pair ) {
            return pair.first;
        } )
        .map( query::llvmdyncast< llvm::Instruction > )
        .filter( query::notnull )
        .filter( [&]( llvm::Instruction * i ) {
            return i->getParent()->getParent() == fn;
        } )
        .freeze();

    for ( auto & v : abstracted )
        processedValues.erase( v );
    for ( auto & v : abstracted ) {
        v->replaceAllUsesWith( llvm::UndefValue::get( v->getType() ) );
        v->eraseFromParent();
    }
}

void Substituter::clean( llvm::Module & m ) {
    auto abstracted = query::query( processedValues )
        .map( [&]( const auto & pair ) {
            return pair.first;
        } )
        .map( query::llvmdyncast< llvm::Instruction > )
        .filter( query::notnull )
        .freeze();

    processedValues.clear();
    for ( auto & v : abstracted ) {
        v->replaceAllUsesWith( llvm::UndefValue::get( v->getType() ) );
        v->eraseFromParent();
    }

    std::vector< llvm::Function * > remove;
    for ( auto it : processedFunctions )
        remove.push_back( it.first );
    for ( auto & fn : remove ) {
        fn->replaceAllUsesWith( llvm::UndefValue::get( fn->getType() ) );
        fn->eraseFromParent();
    }

    auto intrinsics = query::query( m )
        .map( query::refToPtr )
        .filter( []( llvm::Function * fn ) {
            return isIntrinsic( fn );
        } ).freeze();
    for ( auto & in : intrinsics )
        in->eraseFromParent();
}

void Substituter::registerAbstraction( Abstraction && abstraction ) {
    auto dom = abstraction->domain();
    abstractions.emplace( dom, std::move( abstraction ) );
}

const Abstraction Substituter::getAbstraction( llvm::Value * value ) const {
    return getAbstraction( value->getType() );
}

const Abstraction Substituter::getAbstraction( llvm::Type * type ) const {
    assert( isAbstract( type ) );
    auto sty = llvm::cast< llvm::StructType >( stripPtr( type ) );
    auto domain = TypeName( sty ).domain().value();
    if ( !abstractions.count( domain ) )
        throw std::runtime_error( "unknown abstraction domain" );
    return abstractions.at( domain );
}

bool Substituter::isSubstituted( llvm::Value * value ) const {
    return isSubstituted( value->getType() );
}

bool Substituter::isSubstituted( llvm::Type * type ) const {
    for ( const auto& a : abstractions )
        if ( a.second->is( type ) )
            return true;
    return false;
}

/*Intrinsic Substituter::intrinsic( llvm::CallInst * call ) const {
    return Intrinsic();
}*/

} // namespace abstract
} // namespace lart

