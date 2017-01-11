// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/builder.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/types.h>
#include <lart/abstract/intrinsic.h>

#include <algorithm>

namespace lart {
namespace abstract {

namespace _detail{
	using Predicate = llvm::CmpInst::Predicate;
    const std::map< Predicate, std::string > predicate = {
    	{ Predicate::ICMP_EQ, "eq" },
    	{ Predicate::ICMP_NE, "ne" },
    	{ Predicate::ICMP_UGT, "ugt" },
    	{ Predicate::ICMP_UGE, "uge" },
    	{ Predicate::ICMP_ULT, "ult" },
    	{ Predicate::ICMP_ULE, "ule" },
    	{ Predicate::ICMP_SGT, "sgt" },
    	{ Predicate::ICMP_SGE, "sge" },
    	{ Predicate::ICMP_SLT, "slt" },
		{ Predicate::ICMP_SLE, "sle" }
    };

    std::vector< llvm::Type * > types( std::vector< llvm::Value * > values ) {
        std::vector< llvm::Type * > tys;
        for ( auto & v : values )
            tys.push_back( v->getType() );
        return tys;
    }

	auto remapArgs( const llvm::CallInst * i,
			        const std::map< llvm::Value *, llvm::Value * > & _values )
	             -> std::vector< llvm::Value * >
	{
		std::vector< llvm::Value * > args;
		for ( auto & op : i->arg_operands() ) {
			auto arg = _values.count( op ) ? _values.at( op ) : op;
			args.push_back( arg );
		}
		return args;
	}

    auto liftsOf( llvm::Instruction * i ) ->  std::vector< llvm::CallInst * > {
        return query::query( i->users() )
                     .map( query::llvmdyncast< llvm::CallInst > )
                     .filter( query::notnull )
                     .filter( [&]( llvm::CallInst * call ) {
                         return intrinsic::isLift( call );
                     } ).freeze();
    }

    bool sameTypes( std::vector< llvm::Type * > a, std::vector< llvm::Type * > b ) {
        return std::equal( a.begin(), a.end(), b.begin(), b.end() );
    }
}

void AbstractBuilder::store( llvm::Type * t1, llvm::Type * t2 ) {
    assert( types::isAbstract( t2 ) );
    _types[ t1 ] = t2;
    _types[ t1->getPointerTo() ] = t2->getPointerTo();
}

void AbstractBuilder::store( llvm::Value * v1, llvm::Value * v2 ) {
    assert( types::isAbstract( v2->getType() ) );
    _values[ v1 ] = v2;
}

void AbstractBuilder::store( llvm::Function * f1, llvm::Function * f2 ) {
	if ( _functions.count( f1 ) )
		_functions[ f1 ].push_back( f2 );
	else
		_functions[ f1 ] = { f2 };
}

llvm::Value * AbstractBuilder::process( llvm::Instruction * i ) {
    if ( ignore( i ) ) return nullptr;

    auto type = i->getType()->isPointerTy()
              ? i->getType()->getPointerElementType()
              : i->getType();
    if ( !_types.count( type ) && !type->isVoidTy() && !types::isAbstract( type ) )
        llvmcase( type,
            [&]( llvm::IntegerType * t ) {
                store( t, types::IntegerType::get( t ) );
            },
            [&]( llvm::Type * type ) {
                std::cerr << "ERR: abstracting unsupported type: ";
                type->dump();
                std::exit( EXIT_FAILURE );
            } );

    auto abstract = create( i );
	if ( abstract ) {
        _values[i] = abstract;
        for ( auto & lift : _detail::liftsOf( i ) )
            lift->replaceAllUsesWith( _values[ i ] );
    }
	return abstract ? abstract : i;
}

void AbstractBuilder::annotate() {
    for ( auto & a : _anonymous ) {
        annotate( a.first, a.second );
        a.first->eraseFromParent();
    }
    _anonymous.clear();
}

llvm::Value * AbstractBuilder::process( llvm::Argument * arg ) {
    if ( types::isAbstract( arg->getType() ) )
        _values[ arg ] = arg;
    return arg;
}

llvm::Value * AbstractBuilder::process( llvm::CallInst * call,
                                        llvm::Function * fn )
{
    assert( fn );

    auto args = _detail::remapArgs( call, _values );

    llvm::IRBuilder<> irb( call );
    auto ncall = irb.CreateCall( fn, args );
    _values[ call ] = ncall;
    return ncall;
}

void AbstractBuilder::annotate( llvm::Function * anonymous,
                                const std::string & name )
{
    auto rty = anonymous->getReturnType();

    std::vector< std::pair< llvm::CallInst *, llvm::CallInst *> > replace;
    for ( auto user : anonymous->users() ) {
        auto call = llvm::cast< llvm::CallInst >( user );
        std::vector < llvm::Value * > args;
        for ( auto &arg : call->arg_operands() )
            if ( types::isAbstract( arg->getType() ) )
                args.push_back( arg );
            else {
                llvm::IRBuilder<> irb( call );
                args.push_back( lift( arg, irb ) );
            }

        std::vector< llvm::Type * > types;
        for ( auto & v : args )
            types.push_back( v->getType() );

        auto fty = llvm::FunctionType::get( rty, types, false );
        auto fn = call->getModule()->getOrInsertFunction( name, fty );
        auto ncall = llvm::CallInst::Create( fn, args );
        replace.push_back( { call, ncall } );
    }

    for ( auto &r : replace )
        llvm::ReplaceInstWithInst( r.first, r.second );
}

llvm::Function * AbstractBuilder::changeReturn( llvm::Function * fn ) {
	llvm::Type * rty = _types[ fn->getReturnType() ];
	auto changed = lart::changeReturnType( fn, rty );
    return changed;
}

llvm::Value * AbstractBuilder::create( llvm::Instruction * inst ) {
    llvm::Value * ret = nullptr;
    llvmcase( inst,
        [&]( llvm::AllocaInst * i ) {
            ret = createAlloca( i );
        },
        [&]( llvm::LoadInst * i ) {
            ret = createLoad( i );
        },
        [&]( llvm::StoreInst * i ) {
            ret = createStore( i );
        },
        [&]( llvm::ICmpInst * i ) {
            ret = createICmp( i );
        },
        [&]( llvm::SelectInst * i ) {
            ret = createSelect( i );
        },
        [&]( llvm::BranchInst * i ) {
            ret = createBranch( i );
        },
        [&]( llvm::BinaryOperator * i ) {
            ret = createBinaryOperator( i );
        },
        [&]( llvm::CastInst * i ) {
            ret = createCast( i );
        },
        [&]( llvm::PHINode * i ) {
            ret = createPhi( i );
        },
        [&]( llvm::CallInst * i ) {
            ret = createCall( i );
        },
        [&]( llvm::ReturnInst * i ) {
            ret = createReturn( i );
        },
        [&]( llvm::Instruction * inst ) {
            std::cerr << "ERR: unknown instruction: ";
            inst->dump();
            std::exit( EXIT_FAILURE );
        } );
    return ret;
}

void AbstractBuilder::clean( std::vector< llvm::Value * > & deps ) {
    for ( auto dep : deps ) {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
            for ( auto & lift : _detail::liftsOf( inst ) )
                if ( std::find( deps.begin(), deps.end(), lift ) == deps.end() )
                    lift->eraseFromParent();
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
            inst->removeFromParent();
    }
    for ( auto dep : deps )
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
            inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
    for ( auto dep : deps )
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) ) {
            auto val = _values.find( inst );
            if ( val != _values.end() )
                _values.erase( val );
            delete inst;
        }
}

bool AbstractBuilder::ignore( llvm::Instruction * i ) {
    if ( auto call = llvm::dyn_cast< llvm::CallInst >( i ) )
        return intrinsic::isLift( call );
    return false;
}

llvm::Value * AbstractBuilder::createAlloca( llvm::AllocaInst * i ) {
    assert( !i->isArrayAllocation() );
    auto rty = _types[ i->getType() ];
    auto tag = intrinsic::tag( i ) + "." + types::name( i->getAllocatedType() );
    llvm::IRBuilder<> irb( i );
    return intrinsic::build( i->getModule(), irb, rty, tag );
}

llvm::Value * AbstractBuilder::createLoad( llvm::LoadInst * i ) {
    auto args = { _values[ i->getOperand( 0 ) ] };
    auto rty = _types[ i->getType() ];
    auto tag = intrinsic::tag( i ) + "." + types::name( i->getType() );
    llvm::IRBuilder<> irb( i );

    auto call = intrinsic::anonymous( i->getModule(), irb, rty, args );
    _anonymous[ call->getCalledFunction() ] = tag;

    return call;
}

llvm::Value * AbstractBuilder::createStore( llvm::StoreInst * i ) {
    auto val = _values.count( i->getOperand( 0 ) )
             ? _values[ i->getOperand( 0 ) ]
             : i->getOperand( 0 );
    auto ptr = _values.count( i->getOperand( 1 ) )
             ? _values[ i->getOperand( 1 ) ]
             : i->getOperand( 1 );

    llvm::IRBuilder<> irb( i );
    if ( !types::isAbstract( val->getType() ) )
        val = lift( val, irb );
    if ( !types::isAbstract( ptr->getType() ) )
        ptr = lift( ptr, irb );

    auto rty = llvm::Type::getVoidTy( i->getContext() );
    auto tag = intrinsic::tag( i ) + "." + types::lowerTypeName( val->getType() );

    auto call = intrinsic::anonymous( i->getModule(), irb, rty, { val, ptr } );
    _anonymous[ call->getCalledFunction() ] = tag;

    return call;
}

llvm::Value * AbstractBuilder::createICmp( llvm::ICmpInst * i ) {
    std::vector< llvm::Value * > args;
    for ( const auto & op : i->operands() ) {
        auto arg = _values.count( op ) ? _values.at( op ) : op;
        args.push_back( arg );
    }
    auto type = types::isAbstract( args[ 0 ]->getType() )
              ? types::lower( args[ 0 ]->getType() )
              : args[ 0 ]->getType();
	auto tag = intrinsic::tag( i ) + "_"
             + _detail::predicate.at( i->getPredicate() ) + "."
             + types::name( type );
    llvm::IRBuilder<> irb( i );
    auto call = intrinsic::anonymous( i->getModule(), irb,
                types::IntegerType::get( i->getContext(), 1 ), args );
    _anonymous[ call->getCalledFunction() ] = tag;
    return call;
}

llvm::Value * AbstractBuilder::createSelect( llvm::SelectInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto cond = _values.count( i->getCondition() )
              ? lower( i, irb )
              : i->getCondition();

    auto tv = i->getTrueValue();
    auto fv = i->getFalseValue();

    if ( !types::isAbstract( tv->getType() ) )
        tv = _values.count( tv ) ? _values[ tv ] : lift( tv, irb );
    if ( !types::isAbstract( fv->getType() ) )
        fv = _values.count( fv ) ? _values[ fv ] : lift( fv, irb );

    return irb.CreateSelect( cond, tv, fv );
}

llvm::Value * AbstractBuilder::createBranch( llvm::BranchInst * i ) {
    llvm::IRBuilder<> irb( i );
    if ( i->isUnconditional() ) {
        auto dest = i->getSuccessor( 0 );
        return irb.CreateBr( dest );
    } else {
        auto cond = _values.count( i->getCondition() )
                  ? lower( toTristate( _values[ i->getCondition() ], irb ), irb )
                  : i->getCondition();
        auto tbb = i->getSuccessor( 0 );
        auto fbb = i->getSuccessor( 1 );
        return irb.CreateCondBr( cond, tbb, fbb );
    }
}

llvm::Value * AbstractBuilder::createBinaryOperator( llvm::BinaryOperator * i ) {
    std::vector< llvm::Value * > args;
    for ( auto & arg : i->operands() ) {
        auto v = _values.count( arg ) ? _values[ arg ] : arg;
        args.push_back( v );
    }
    auto tag = intrinsic::tag( i ) + "."
             + types::name( i->getType() );
    auto rty = _types[ i->getType() ];

    llvm::IRBuilder<> irb( i );
    auto call = intrinsic::anonymous( i->getModule(), irb, rty, args );
    _anonymous[ call->getCalledFunction() ] = tag;
    return call;
}

llvm::Value * AbstractBuilder::createCast( llvm::CastInst * i ) {
    auto op0 = i->getOperand( 0 );
    auto arg = _values.count( op0 ) ? _values[ op0 ] : op0;
    auto tag = intrinsic::tag( i ) + "." +
               types::name( i->getSrcTy() ) + "." +
               types::name( i->getDestTy() );
    auto dest = i->getDestTy();
    store( dest, types::lift( dest ) );
    auto rty = _types[ dest ];

    llvm::IRBuilder<> irb( i );
    auto call = intrinsic::anonymous( i->getModule(), irb, rty, { arg } );
    _anonymous[ call->getCalledFunction() ] = tag;
    return call;
}

llvm::Value * AbstractBuilder::createPhi( llvm::PHINode * n ) {
    auto rty = types::isAbstract( n->getType() )
             ? n->getType()
             : _types[ n->getType() ];

    unsigned int niv = n->getNumIncomingValues();
    llvm::IRBuilder<> irb( n );
    auto phi = irb.CreatePHI( rty, niv );
    _values[ n ] = phi;
    if ( types::isAbstract( n->getType() ) )
        n->replaceAllUsesWith( phi );

    for ( unsigned int i = 0; i < niv; ++i ) {
        auto val = llvm::cast< llvm::Value >( n->getIncomingValue( i ) );
        auto parent = n->getIncomingBlock( i );
        if ( _values.count( val ) ) {
            phi->addIncoming( _values[ val ], parent );
        } else {
            if ( types::isAbstract( val->getType() ) )
                phi->addIncoming( val, parent );
            else {
                auto nbb =  parent->splitBasicBlock( parent->getTerminator() );
                llvm::IRBuilder<> b( nbb->getTerminator() );
                phi->addIncoming( lift( val, b ), nbb );
            }
        }
    }
    return phi;
}

llvm::Value * AbstractBuilder::createCall( llvm::CallInst * i ) {
    if ( intrinsic::isLift( i ) )
        return processLiftCall( i );
    if ( intrinsic::isLower( i ) )
        return clone( i );
    if ( intrinsic::name( i ) == "bool_to_tristate" )
        return clone( i );
    if ( i->getCalledFunction()->isIntrinsic() )
        return processIntrinsic( llvm::cast< llvm::IntrinsicInst >( i ) );
    if ( _anonymous.count( i->getCalledFunction() ) )
    	return processAnonymous( i );
    return processCall( i );
}

llvm::Value * AbstractBuilder::createReturn( llvm::ReturnInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto ret = _values.count( i->getReturnValue() )
             ? _values[ i->getReturnValue() ]
             : i->getReturnValue();
    return irb.CreateRet( ret );
}

llvm::Value * AbstractBuilder::lower( llvm::Value * v, llvm::IRBuilder<> & irb ) {
    assert( types::isAbstract( v->getType() ) );
    assert( !v->getType()->isPointerTy() );

    auto fty = llvm::FunctionType::get( types::lower( v->getType() ),
                                        { v->getType() }, false );
    auto tag = types::domain( v->getType() ) == "tristate"
             ? "lart.tristate.lower"
             : "lart." + types::domain( v->getType() ) + ".lower." +
               types::lowerTypeName( v->getType() );
    auto m = irb.GetInsertBlock()->getModule();
    auto fn = m->getOrInsertFunction( tag, fty );
    auto call = irb.CreateCall( fn , v );

    _values[ v ] = call;
    return call;
}

llvm::Value * AbstractBuilder::lift( llvm::Value * v, llvm::IRBuilder<> & irb ) {
    auto type = _types[ v->getType() ];
    auto fty = llvm::FunctionType::get( type, { v->getType() }, false );
    auto tag = types::domain( type ) == "tristate"
             ? "lart.tristate.lift"
             : "lart." + types::domain( type ) + ".lift." +
                types::lowerTypeName( type );
    auto m = irb.GetInsertBlock()->getModule();
    auto fn = m->getOrInsertFunction( tag, fty );
    auto call = irb.CreateCall( fn , v );

    _values[ v ] = call;
    return call;
}

llvm::Value * AbstractBuilder::toTristate( llvm::Value * v, llvm::IRBuilder<> & irb ) {
    assert( types::IntegerType::isa( v->getType(), 1 ) );
    auto tristate = types::Tristate::get( v->getContext() );
    auto fty = llvm::FunctionType::get( tristate, { v->getType() }, false );
    auto tag = "lart." + types::domain( v->getType() ) + ".bool_to_tristate";
    auto m = irb.GetInsertBlock()->getModule();
    auto fn = m->getOrInsertFunction( tag, fty );
    return irb.CreateCall( fn , v );
}

llvm::Value * AbstractBuilder::processLiftCall( llvm::CallInst * i ) {
    assert( _values[ i->getArgOperand( 0 ) ] );
    i->replaceAllUsesWith( _values[ i->getArgOperand( 0 ) ] );
    return i;
}

llvm::Value * AbstractBuilder::clone( llvm::CallInst * i ) {
    auto clone = i->clone();
    clone->insertBefore( i );
    i->replaceAllUsesWith( clone );
    return clone;
}

llvm::Value * AbstractBuilder::processIntrinsic( llvm::CallInst * i ) {
    auto intr = llvm::cast< llvm::IntrinsicInst >( i );
	auto name = llvm::Intrinsic::getName( intr->getIntrinsicID() );
    if ( ( name == "llvm.lifetime.start" ) || ( name == "llvm.lifetime.end" ) ) {
        //ignore
    }
    else if ( name == "llvm.var.annotation" ) {
        //ignore
    }
    else {
        std::cerr << "ERR: unknown intrinsic: ";
        i->dump();
        std::exit( EXIT_FAILURE );
    }
    return nullptr;
}

llvm::Value * AbstractBuilder::processAnonymous( llvm::CallInst * i ) {
    auto args = _detail::remapArgs( i, _values );
    auto fn = i->getCalledFunction();
	auto m = i->getModule();
	auto rty = fn->getReturnType();

	llvm::IRBuilder<> irb( i );
	//auto tag = fn->getName(); //this is weird
	//auto call = intrinsic::build( m, irb, rty, tag, args );
	auto call = intrinsic::anonymous( m, irb, rty, args );

	auto find = _anonymous[ fn ];
	i->replaceAllUsesWith( call );
	_anonymous[ call->getCalledFunction() ] = find;
	return call;
}

llvm::Value * AbstractBuilder::processCall( llvm::CallInst * i ) {
    auto args = _detail::remapArgs( i, _values );
    std::vector< llvm::Type * > types = arg_types( i );
    auto fn = i->getCalledFunction();
    auto at = types::isAbstract( i->getType() )
            ? i->getType()
            : _types[ i->getType() ];

	bool same = _detail::sameTypes( fn->getFunctionType()->params(), types )
			 && fn->getReturnType() == at;

	auto stored = same ? fn : getStoredFn( fn, types );

	if ( !stored && _functions.count( fn ) ) {
		auto res = getStoredFn( fn, fn->getFunctionType()->params() );
		if ( res->empty() ) {
			assert( res->getName().startswith( "lart" ) );
			stored = res;
		}
	}
    assert( stored );
	/*if ( !stored ) {
		return nullptr; // let abstraction handle unknown call
	}*/

	llvm::IRBuilder<> irb( i );
	return irb.CreateCall( stored, args );
}

bool AbstractBuilder::needToPropagate( llvm::CallInst * i ) {
    if (  ( intrinsic::isLift( i ) )
       || ( intrinsic::isLower( i ) )
       || ( i->getCalledFunction()->isIntrinsic() )
       || ( _anonymous.count( i->getCalledFunction() ) ) )
       return false;
    auto args = _detail::remapArgs( i, _values );
    std::vector< llvm::Type * > types = arg_types( i );

    auto fn = i->getCalledFunction();
    auto at = types::isAbstract( i->getType() )
            ? i->getType()
            : _types[ i->getType() ];

    bool same = _detail::sameTypes( fn->getFunctionType()->params(), types )
			 && fn->getReturnType() == at;

	auto stored = same ? fn : getStoredFn( fn, types );
    return stored == nullptr;
}

llvm::Function * AbstractBuilder::getStoredFn( llvm::Function * fn,
                                  llvm::ArrayRef< llvm::Type * >  params)
{
	if ( _functions.count( fn ) )
	  for ( auto stored : _functions[ fn ] )
		  if ( _detail::sameTypes( stored->getFunctionType()->params(), params ) )
			  return stored;
	return nullptr;
}

std::vector< llvm::Type * > AbstractBuilder::arg_types( llvm::CallInst * call ) {
    auto args = _detail::remapArgs( call, _values );
    return _detail::types( args );
}

} // namespace abstract
} // namespace lart
