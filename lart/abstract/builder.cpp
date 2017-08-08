// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/builder.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/types/utils.h>
#include <lart/abstract/value.h>
#include <lart/abstract/intrinsic.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {
    using Types = std::vector< Type * >;
    using Values = std::vector< Value * >;

    using Predicate = CmpInst::Predicate;
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

    Types typesOf( const Values & vs ) {
        Types ts;
        std::transform( vs.cbegin(), vs.cend(), std::back_inserter( ts ),
            [] ( const Value * v ) { return v->getType(); } );
        return ts;
    }

	auto remap( const Values & vs, const std::map< Value *, Value * > & vmap ) {
		Values res;
        std::transform( vs.cbegin(), vs.cend(), std::back_inserter( res ),
			[&]( Value * v ) { return vmap.count( v ) ? vmap.at( v ) : v; } );
		return res;
	}

    auto liftsOf( Instruction * i ) {
        return query::query( i->users() )
                     .map( query::llvmdyncast< CallInst > )
                     .filter( query::notnull )
                     .filter( []( CallInst * call ) {
                         return isLift( call );
                     } ).freeze();
    }


    bool equal( const Types & a, const Types & b ) {
        return std::equal( a.begin(), a.end(), b.begin(), b.end() );
    }
}

void AbstractBuilder::store( Value * v1, Value * v2 ) {
    _values[ v1 ] = v2;
}

void AbstractBuilder::store( Function * f1, Function * f2 ) {
	if ( _functions.count( f1 ) )
		_functions[ f1 ].push_back( f2 );
	else
		_functions[ f1 ] = { f2 };
}

Values AbstractBuilder::operands( const AbstractValue & av ) {
    std::vector< Value * > res;
    auto inst = cast< Instruction >( av.value() );
    IRBuilder<> irb( inst );
    const auto & ops = inst->operands();
    std::transform( ops.begin(), ops.end(), std::back_inserter( res ),
        [&] ( const auto & op ) {
            return _values.count( op ) ? _values.at( op ) : lift( op, av.domain(), irb );
        } );
    return res;
}

void AbstractBuilder::clone( const FunctionNodePtr & node ) {
    ValueToValueMapTy vmap;
    auto clone = CloneFunction( node->function, vmap, true, nullptr );
    node->function->getParent()->getFunctionList().push_back( clone );

    std::unordered_set< AbstractValue > origins;
    for ( const auto & origin : node->origins )
        if ( const auto & arg = dyn_cast< Argument >( origin.value() ) ) {
            auto ca = std::next( clone->arg_begin(), arg->getArgNo() );
            origins.emplace( ca, origin.domain() );
        }

    auto it = query::query( *clone ).flatten().begin();
    for ( const auto & inst : query::query( *node->function ).flatten() ) {
        auto origin = std::find_if( node->origins.begin(), node->origins.end(),
            [&] ( const auto & n ) { return n.value() == &inst; } );
        if ( origin != node->origins.end() )
            origins.emplace( &(*it), origin->domain() );
        ++it;
    }

    if ( _functions.count( node->function ) )
        _functions[ clone ] = _functions[ node->function ];

    node->function = clone;
    node->origins = origins;
}


void AbstractBuilder::process( const AbstractValue & av ) {
    llvmcase( av.value(),
        [&] ( Instruction * inst ) {
            if ( inst->getType()->isStructTy() ) {
                inst->dump();
                return;
            }
            if ( !ignore( inst ) )
                if ( auto abstract = create( av ) ) {
                    store( inst, abstract );
                    for ( auto & lift : liftsOf( inst ) )
                        lift->replaceAllUsesWith( abstract );
                }
        },
        [&] ( Argument * arg ) {
            IRBuilder<> irb( arg->getParent()->front().begin() );
            if ( isAbstract( arg->getType() ) ) {
                _values[ arg ] = arg;
            } else {
                _values[ arg ] = lift( arg, av.domain(), irb );
            }
        });
}

Function * AbstractBuilder::process( const FunctionNodePtr & node ) {
    auto fn = node->function;
    auto rets = query::query( node->postorder() )
        .filter( []( const auto & n ) {
            return isa< ReturnInst >( n.value() );
        } ).freeze();

    auto args = query::query( node->postorder() )
        .filter( []( const auto & n ) {
            return isa< Argument >( n.value() );
        } ).freeze();

    // Signature does not need to be changed
    if ( rets.empty() && args.empty() )
        return fn;

    auto fty = node->function->getFunctionType();
    auto rty = fty->getReturnType();
    if ( !rets.empty() ) {
        // TODO refactore
        auto dom = rets.at( 0 ).domain();
        rty = dom->match(
            [&] ( ComposedDomain ) { return ComposedType( rty, dom ).llvm(); },
            [&] ( UnitDomain ) { return ScalarType( rty, dom ).llvm(); } )
            .value();
    }

    Types argTys;
    if ( !args.empty() ) {
        for ( auto & arg : fn->args() ) {
            auto changed = query::query( args )
                .filter( [&]( const auto & n ) {
                    return cast< Argument >( n.value() ) == &arg;
                } ).freeze();
            if ( changed.empty() )
                argTys.push_back( arg.getType() );
            else
                argTys.push_back( changed.at(0).type()->llvm() );
        }
    } else {
        argTys = fty->params();
    }

    auto newfty = FunctionType::get( rty, argTys, fty->isVarArg() );
    auto newfn = Function::Create( newfty, fn->getLinkage(), fn->getName(), fn->getParent() );
    store( fn, newfn );
    return newfn;
}

#define ASSERT_NOT_POINTER_TYPE( inst ) \
    assert( !inst->getType()->isPointerTy() );

Value * AbstractBuilder::create( const AbstractValue & v ) {
    Value * ret = nullptr;
    llvmcase( v.value(),
        [&]( LoadInst * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createLoad( v );
        },
        [&]( StoreInst * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createStore( v );
        },
        [&]( ICmpInst * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createICmp( v );
        },
        [&]( SelectInst * ) {
            // Every SelectInst should be lowered to branching.
            std::cerr << "ERR: Abstraction of 'select' instruction should never happen.";
            std::exit( EXIT_FAILURE );
        },
        [&]( BranchInst * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createBranch( v );
        },
        [&]( BinaryOperator * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createBinaryOperator( v );
        },
        [&]( CastInst * i ) {
            ret = i->getType()->isPointerTy() ? createPtrCast( v ) : createCast( v );
        },
        [&]( GetElementPtrInst * ) {
            ret = createGEP( v );
        },
        [&]( PHINode * i ) {
            ASSERT_NOT_POINTER_TYPE( i );
            ret = createPhi( v );
        },
        [&]( CallInst * i ) {
            if ( i->getType()->isPointerTy() ) {
                // TODO why?
                auto fn = i->getCalledFunction();
                if ( fn->isIntrinsic() ) {
                    // skip llvm.ptr.annotation function calls
                    assert( fn->getName().startswith( "llvm.ptr.annotation" ) );
                    i->replaceAllUsesWith( i->getArgOperand( 0 ) );
                }
            } else {
                ret = createCall( v );
            }
        },
        [&]( ReturnInst * ) {
            ret = createReturn( v );
        },
        [&]( AllocaInst * ) {
            ret = createAlloca( v );
        },
        [&]( Instruction * inst ) {
            std::cerr << "ERR: unknown instruction: ";
            inst->dump();
            std::exit( EXIT_FAILURE );
        } );
    return ret;
}

void AbstractBuilder::clean( Values & deps ) {
    for ( auto dep : deps ) {
        if ( auto inst = dyn_cast< Instruction >( dep ) )
            for ( auto & lift : liftsOf( inst ) )
                if ( std::find( deps.begin(), deps.end(), lift ) == deps.end() )
                    lift->eraseFromParent();
        if ( auto inst = dyn_cast< Instruction >( dep ) )
            inst->removeFromParent();
    }
    for ( auto dep : deps )
        if ( auto inst = dyn_cast< Instruction >( dep ) )
            inst->replaceAllUsesWith( UndefValue::get( inst->getType() ) );
    for ( auto dep : deps )
        if ( auto inst = dyn_cast< Instruction >( dep ) ) {
            auto val = _values.find( inst );
            if ( val != _values.end() )
                _values.erase( val );
            delete inst;
        }
}

bool AbstractBuilder::ignore( Instruction * i ) {
    if ( auto call = dyn_cast< CallInst >( i ) )
        return isLift( call );
    return false;
}

Value * AbstractBuilder::createAlloca( const AbstractValue & av ) {
    auto i = av.value< AllocaInst >();
    assert( !i->isArrayAllocation() );
    auto rty = av.type()->llvm();
    auto intr = Intrinsic( i->getModule(), rty, intrinsicName( av ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), {} );
}

Value * AbstractBuilder::createLoad( const AbstractValue & av ) {
    auto i = av.value< LoadInst >();
    auto args = { _values[ i->getOperand( 0 ) ] };
    auto rty = av.type()->llvm();
    auto intr = Intrinsic( i->getModule(), rty, intrinsicName( av ), typesOf( args ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), args );
}

Value * AbstractBuilder::createStore( const AbstractValue & av ) {
    auto i = av.value< StoreInst >();
    IRBuilder<> irb( i );
    auto args = operands( av );
    auto rty = VoidType( i->getContext() );
    auto storedName = TypeName( cast< StructType >( args[0]->getType() ) ).base().name();
    auto name = intrinsicName( av ) + "." + storedName;
    auto intr = Intrinsic( i->getModule(), rty, name, typesOf( args ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), args );
}

Value * AbstractBuilder::createICmp( const AbstractValue & av ) {
    auto i = av.value< ICmpInst >();
    auto args = operands( av );
    auto rty = liftTypeLLVM( BoolType( i->getContext() ), av.domain() );
	auto name = intrinsicName( av ) + "_" + predicate.at( i->getPredicate() )
             + "." + TypeName( cast< StructType >( args[0]->getType() ) ).base().name();
    auto intr = Intrinsic( i->getModule(), rty, name, typesOf( args ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), args );
}

Value * AbstractBuilder::createBranch( const AbstractValue & av ) {
    auto i = av.value< BranchInst >();
    IRBuilder<> irb( i );
    if ( i->isUnconditional() ) {
        auto dest = i->getSuccessor( 0 );
        return irb.CreateBr( dest );
    } else {
        Value * cond;
        if ( _values.count( i->getCondition() ) ) {
            auto tristate = toTristate( _values[ i->getCondition() ], av.domain(), irb );
            cond = lower( tristate, irb );
        } else {
            cond = i->getCondition();
        }
        return irb.CreateCondBr( cond, i->getSuccessor( 0 ), i->getSuccessor( 1 ) );
    }
}

Value * AbstractBuilder::createBinaryOperator( const AbstractValue & av ) {
    auto i = av.value< BinaryOperator >();
    IRBuilder<> irb( i );
    auto args = operands( av );
    auto rty = av.type()->llvm();
    auto intr = Intrinsic( i->getModule(), rty, intrinsicName( av ), typesOf( args ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), args );
}

Value * AbstractBuilder::createCast( const AbstractValue & av ) {
    auto i = av.value< CastInst >();
    IRBuilder<> irb( i );
    auto args = operands( av );
    auto rty = liftTypeLLVM( i->getDestTy(), av.domain() );
    auto intr = Intrinsic( i->getModule(), rty, intrinsicName( av ), typesOf( args ) );
    return IRBuilder<>( i ).CreateCall( intr.declaration(), args );
}

Value * AbstractBuilder::createPhi( const AbstractValue & av ) {
    auto n = av.value< PHINode >();
    auto rty = av.type()->llvm();

    unsigned int niv = n->getNumIncomingValues();
    IRBuilder<> irb( n );
    auto phi = irb.CreatePHI( rty, niv );
    _values[ n ] = phi;
    if ( isAbstract( n->getType() ) )
        n->replaceAllUsesWith( phi );

    for ( unsigned int i = 0; i < niv; ++i ) {
        auto val = n->getIncomingValue( i );
        auto parent = n->getIncomingBlock( i );
        if ( _values.count( val ) ) {
            phi->addIncoming( _values[ val ], parent );
        } else {
            if ( isAbstract( val->getType() ) )
                phi->addIncoming( val, parent );
            else {
                auto term = _values.count( parent->getTerminator() )
                            ? _values[ parent->getTerminator() ]
                            : parent->getTerminator();
                auto nbb =  parent->splitBasicBlock( cast< Instruction >( term ) );
                IRBuilder<> b( nbb->begin() );
                phi->addIncoming( lift( val, av.domain(), b ), nbb );
            }
        }
    }
    return phi;
}

Value * AbstractBuilder::createCall( const AbstractValue & av ) {
    auto i = av.value< CallInst >();
    if ( isLift( i ) ) {
        return processLiftCall( i );
    } else if ( isIntrinsic( i ) ) { // Abstract intrinsic
        return clone( i );
    } else if ( i->getCalledFunction()->isIntrinsic() ){ // llvm intrinsic
        return processIntrinsic( cast< IntrinsicInst >( i ) );
    } else {
        return processCall( av );
    }
}

Value * AbstractBuilder::createReturn( const AbstractValue & av ) {
    auto i = av.value< ReturnInst >();
    IRBuilder<> irb( i );
    auto ret = _values.count( i->getReturnValue() )
             ? _values[ i->getReturnValue() ]
             : i->getReturnValue();
    return irb.CreateRet( ret );
}

Value * AbstractBuilder::createPtrCast( const AbstractValue & av ) {
    auto i = cast< CastInst >( av.value() );
    assert( isa< BitCastInst >( i ) &&
           "ERR: Only bitcast is supported from pointer cast instractions." );

    IRBuilder<> irb( i );
    auto destTy = liftTypeLLVM( i->getDestTy(), av.domain() );
    auto val = _values[ i->getOperand( 0 ) ];
    assert( val && "ERR: Trying to bitcast value, that is not abstracted." );
    return ( val->getType() == destTy ) ? val : irb.CreateBitCast( val, destTy );
}

Value * AbstractBuilder::createGEP( const AbstractValue & av ) {
    auto i = cast< GetElementPtrInst >( av.value() );
    IRBuilder<> irb( i );
    auto val = _values[ i->getPointerOperand() ];
    Values idxs = { i->idx_begin() , i->idx_end() };
    auto type = val->getType()->getScalarType()->getPointerElementType();
    return irb.CreateGEP( type, val, idxs, i->getName() );
}

Value * AbstractBuilder::lower( Value * v, IRBuilder<> & irb ) {
    const auto & type = cast< StructType >( v->getType() );
    bool abs = isAbstract( type );
    assert( abs && !type->isPointerTy() );

    auto fty = FunctionType::get( lowerTypeLLVM( type ), { type }, false );
    auto dom = TypeName( type ).domain();
    auto tag = "lart." + dom->name() + ".lower";
    if ( dom->get< UnitDomain >().value != DomainValue::Tristate )
        tag += "." + TypeName( type ).base().name();
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
    auto call = irb.CreateCall( fn , v );

    _values[ v ] = call;
    return call;
}

Value * AbstractBuilder::lift( Value * v, DomainPtr domain, IRBuilder<> & irb ) {
    auto type = liftType( v->getType(), domain );
    auto fty = FunctionType::get( type->llvm(), { v->getType() }, false );
    auto tag = "lart." + domain->name() + ".lift." + type->baseName();
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
    return irb.CreateCall( fn , v );
}

Value * AbstractBuilder::toTristate( Value * v, DomainPtr domain, IRBuilder<> & irb ) {
    auto type = liftType( v->getType(), domain );
    auto tristate = Tristate( type->origin() ).llvm();
    auto fty = FunctionType::get( tristate, { v->getType() }, false );
    auto tag = "lart." + domain->name() + ".bool_to_tristate";
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
    return irb.CreateCall( fn , v );
}

Value * AbstractBuilder::processLiftCall( CallInst * i ) {
    assert( _values[ i->getArgOperand( 0 ) ] );
    i->replaceAllUsesWith( _values[ i->getArgOperand( 0 ) ] );
    return i;
}

Value * AbstractBuilder::clone( CallInst * i ) {
    auto clone = i->clone();
    clone->insertBefore( i );
    i->replaceAllUsesWith( clone );
    return clone;
}

Value * AbstractBuilder::processIntrinsic( CallInst * i ) {
    auto intr = cast< IntrinsicInst >( i );
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

Value * AbstractBuilder::processCall( const AbstractValue & av ) {
    auto i = av.value< CallInst >();
    Values args = { i->arg_operands().begin(), i->arg_operands().end() };
    Types types = argTypes( i );
    auto fn = i->getCalledFunction();

	bool same = equal( fn->getFunctionType()->params(), types )
			 && fn->getReturnType() == av.type()->llvm();

	auto stored = same ? fn : getStoredFn( fn, types );
	if ( !stored && _functions.count( fn ) ) {
		auto res = getStoredFn( fn, fn->getFunctionType()->params() );
		if ( res->empty() ) {
			assert( res->getName().startswith( "lart" ) );
			stored = res;
		}
	}
    assert( stored );

	IRBuilder<> irb( i );
	auto call = irb.CreateCall( stored, remap( args, _values ) );
    if ( call->getType() == i->getType() )
        i->replaceAllUsesWith( call );
    return call;
}

Function * AbstractBuilder::getStoredFn( Function * fn,
                                  ArrayRef< Type * >  params)
{
	if ( _functions.count( fn ) )
	  for ( auto stored : _functions[ fn ] )
          if ( equal( stored->getFunctionType()->params(), params ) )
			  return stored;
	return nullptr;
}

Types AbstractBuilder::argTypes( CallInst * call ) {
    Values args = { call->arg_operands().begin(), call->arg_operands().end() };
    return typesOf( remap( args, _values ) );
}

} // namespace abstract
} // namespace lart
