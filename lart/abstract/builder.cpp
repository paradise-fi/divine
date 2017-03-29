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

llvm::Value * AbstractBuilder::process( llvm::Value * v ) {
    if ( llvm::isa< llvm::Instruction >( v ) )
        return _process( llvm::dyn_cast< llvm::Instruction >( v ) );
    else
        return _process( llvm::dyn_cast< llvm::Argument >( v ) );
}

llvm::Value * AbstractBuilder::_process( llvm::Instruction * i ) {
    if ( ignore( i ) ) return nullptr;

    auto type = types::stripPtr( i->getType() );
    if ( !_types.count( type ) && !type->isVoidTy() && !types::isAbstract( type ) )
        llvmcase( type,
            [&]( llvm::IntegerType * t ) {
                store( t, types::IntegerType::get( t ) );
            },
            [&]( llvm::StructType * t ) {
                assert( t->getNumElements() == 1 );
                auto et = t->getElementType( 0 );
                assert( llvm::isa< llvm::IntegerType>( et ) );
                store( t, types::IntegerType::get( et ) );
            },
            [&]( llvm::Type * type ) {
                std::cerr << "ERR: Abstracting unsupported type: ";
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

llvm::Value * AbstractBuilder::_process( llvm::Argument * arg ) {
    if ( types::isAbstract( arg->getType() ) )
        _values[ arg ] = arg;
    else
        store( arg->getType(), types::lift( arg->getType() ) );
    return arg;
}

llvm::Value * AbstractBuilder::process( const FunctionNodePtr & node ) {
    auto fn = node->function;
    auto rets = query::query( node->postorder() )
               .filter( []( const ValueNode & n ) {
                    return llvm::isa< llvm::ReturnInst >( n.value );
                } ).freeze();

    auto args = query::query( node->postorder() )
               .filter( []( const ValueNode & n ) {
                    return llvm::isa< llvm::Argument >( n.value );
                } ).freeze();

    if ( rets.empty() && args.empty() )
        // Signature does not need to be changed
        return fn;

    auto fty = node->function->getFunctionType();
    auto ret_type = rets.empty()
                  ? fty->getReturnType()
                  : types::lift( fty->getReturnType() );
    std::vector< llvm::Type * > arg_types;
    if ( !args.empty() ) {
        for ( auto & arg : fn->args() ) {
            bool changed = query::query( args ).any( [&]( ValueNode & n ) {
                               return llvm::cast< llvm::Argument >( n.value ) == &arg;
                           } );
            if ( changed )
                arg_types.push_back( types::lift( arg.getType() ) );
            else
                arg_types.push_back( arg.getType() );
        }
    } else {
        arg_types = fty->params();
    }

    auto newfty = llvm::FunctionType::get( ret_type, arg_types, fty->isVarArg() );
    auto newfn = llvm::Function::Create( newfty, fn->getLinkage(), fn->getName(), fn->getParent() );
    store( fn, newfn );
    return newfn;
}

llvm::Value * AbstractBuilder::create( llvm::Instruction * inst ) {
    if ( inst->getType()->isPointerTy() )
        return createPtrInst( inst );
    else
        return createInst( inst );
}

llvm::Value * AbstractBuilder::createPtrInst( llvm::Instruction * inst ) {
    llvm::Value * ret = nullptr;
    llvmcase( inst,
        [&]( llvm::AllocaInst * i ) {
            ret = createAlloca( i );
        },
        [&]( llvm::CastInst * i ) {
            ret = createPtrCast( i );
        },
        [&]( llvm::CallInst * i ) {
            auto fn = i->getCalledFunction();
            if ( fn->isIntrinsic() ) {
                // skip llvm.ptr.annotation function calls
                assert( fn->getName().startswith( "llvm.ptr.annotation" ) );
                i->replaceAllUsesWith( i->getArgOperand( 0 ) );
            }
        },
        [&]( llvm::Instruction * inst ) {
            std::cerr << "ERR: unknown pointer instruction: ";
            inst->dump();
            std::exit( EXIT_FAILURE );
        } );
    return ret;
}

llvm::Value * AbstractBuilder::createInst( llvm::Instruction * inst ) {
    llvm::Value * ret = nullptr;

    llvmcase( inst,
        [&]( llvm::LoadInst * i ) {
            ret = createLoad( i );
        },
        [&]( llvm::StoreInst * i ) {
            ret = createStore( i );
        },
        [&]( llvm::ICmpInst * i ) {
            ret = createICmp( i );
        },
        [&]( llvm::SelectInst * ) {
            // Every SelectInst should be lowered to branching.
            std::cerr << "ERR: Abstraction of 'select' instruction should never happen.";
            std::exit( EXIT_FAILURE );
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
    return intrinsic::build( i->getModule(), irb, rty, tag, args );
}

llvm::Value * AbstractBuilder::createStore( llvm::StoreInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto val = _values.count( i->getOperand( 0 ) )
             ? _values[ i->getOperand( 0 ) ]
             : lift( i->getOperand( 0 ), irb );
    auto ptr = _values.count( i->getOperand( 1 ) )
             ? _values[ i->getOperand( 1 ) ]
             : lift( i->getOperand( 1 ), irb );
    auto rty = llvm::Type::getVoidTy( i->getContext() );
    auto tag = intrinsic::tag( i ) + "." + types::lowerTypeName( val->getType() );

    return intrinsic::build( i->getModule(), irb, rty, tag, { val, ptr } );
}

llvm::Value * AbstractBuilder::createICmp( llvm::ICmpInst * i ) {
    llvm::IRBuilder<> irb( i );
    std::vector< llvm::Value * > args;
    for ( const auto & op : i->operands() ) {
        auto arg = _values.count( op ) ? _values.at( op ) : lift( op, irb );
        args.push_back( arg );
    }
    auto type = types::lower( args[ 0 ]->getType() );
	auto tag = intrinsic::tag( i ) + "_"
             + _detail::predicate.at( i->getPredicate() ) + "."
             + types::name( type );
    return intrinsic::build( i->getModule(), irb,
           types::IntegerType::get( i->getContext(), 1 ), tag, args );
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
    llvm::IRBuilder<> irb( i );

    std::vector< llvm::Value * > args;
    for ( auto & arg : i->operands() ) {
        auto v = _values.count( arg ) ? _values[ arg ] : lift( arg, irb );
        args.push_back( v );
    }
    auto tag = intrinsic::tag( i ) + "."
             + types::name( i->getType() );
    auto rty = _types[ i->getType() ];

    return intrinsic::build( i->getModule(), irb, rty, tag, args );
}

llvm::Value * AbstractBuilder::createCast( llvm::CastInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto op = i->getOperand( 0 );
    auto arg = _values.count( op ) ? _values[ op ] : lift( op, irb );

    auto tag = intrinsic::tag( i ) + "." +
               types::name( i->getSrcTy() ) + "." +
               types::name( i->getDestTy() );

    auto dest = i->getDestTy();
    store( dest, types::lift( dest ) );
    auto rty = _types[ dest ];

    return intrinsic::build( i->getModule(), irb, rty, tag, { arg } );
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
                auto term = _values.count( parent->getTerminator() )
                            ? _values[ parent->getTerminator() ]
                            : parent->getTerminator();
                auto nbb =  parent->splitBasicBlock( llvm::cast< llvm::Instruction >( term ) );
                llvm::IRBuilder<> b( nbb->begin() );
                phi->addIncoming( lift( val, b ), nbb );
            }
        }
    }
    return phi;
}

llvm::Value * AbstractBuilder::createCall( llvm::CallInst * i ) {
    if ( intrinsic::isLift( i ) ) {
        return processLiftCall( i );
    } else if ( intrinsic::is( i ) ) {
        return clone( i );
    } else if ( i->getCalledFunction()->isIntrinsic() ){
        return processIntrinsic( llvm::cast< llvm::IntrinsicInst >( i ) );
    } else {
        return processCall( i );
    }
}

llvm::Value * AbstractBuilder::createReturn( llvm::ReturnInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto ret = _values.count( i->getReturnValue() )
             ? _values[ i->getReturnValue() ]
             : i->getReturnValue();
    return irb.CreateRet( ret );
}

llvm::Value * AbstractBuilder::createPtrCast( llvm::CastInst * i ) {
    assert( llvm::isa< llvm::BitCastInst >( i ) &&
           "ERR: Only bitcast is supported for pointer cast instractions." );

    llvm::IRBuilder<> irb( i );
    auto destTy = _types[ i->getDestTy() ];
    auto val = _values[ i->getOperand( 0 ) ];
    assert( val && "ERR: Trying to bitcast value, that is not abstracted." );

    return irb.CreateBitCast( val, destTy );
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
    auto tag = "lart." + types::domain( type ) + ".lift." + types::lowerTypeName( type );
    auto m = irb.GetInsertBlock()->getModule();
    auto fn = m->getOrInsertFunction( tag, fty );
    return irb.CreateCall( fn , v );
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

	llvm::IRBuilder<> irb( i );
	return irb.CreateCall( stored, args );
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
