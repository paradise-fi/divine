// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/builder.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/transform.h>
#include <lart/abstract/intrinsic.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {
    using Types = std::vector< Type * >;
    using Values = std::vector< Value * >;

    const Domain::Value AbstractDomain = Domain::Value::Abstract;

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

    Types typesOf( const Values & vs ) {
        Types ts;
        std::transform( vs.begin(), vs.end(), std::back_inserter( ts ),
            [] ( const Value * v ) { return v->getType(); } );
        return ts;
    }

	auto remap( const Values & vs, const std::map< Value *, Value * > & vmap ) {
		Values res;
        std::transform( vs.begin(), vs.end(), std::back_inserter( res ),
			[&]( Value * v ) { return vmap.count( v ) ? vmap.at( v ) : v; } );
		return res;
	}

    auto liftsOf( llvm::Instruction * i ) {
        return query::query( i->users() )
                     .map( query::llvmdyncast< llvm::CallInst > )
                     .filter( query::notnull )
                     .filter( []( llvm::CallInst * call ) {
                         return intrinsic::isLift( call );
                     } ).freeze();
    }


    bool equal( const Types & a, const Types & b ) {
        return std::equal( a.begin(), a.end(), b.begin(), b.end() );
    }
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

void AbstractBuilder::clone( const FunctionNodePtr & node ) {
    llvm::ValueToValueMapTy vmap;
    auto clone = llvm::CloneFunction( node->function, vmap, true, nullptr );
    node->function->getParent()->getFunctionList().push_back( clone );

    FunctionNode::Entries entries;
    for ( const auto & entry : node->entries )
        if ( const auto & arg = llvm::dyn_cast< llvm::Argument >( entry.value ) ) {
            entries.emplace( std::next( clone->arg_begin(), arg->getArgNo() ),
                             entry.annotation );
        }

    auto it = query::query( *clone ).flatten().begin();
    for ( auto & inst :  query::query( *node->function ).flatten() ) {
        auto entry = std::find_if( node->entries.begin(), node->entries.end(),
            [&] ( const ValueNode & n ) { return n.value == &inst; } );
        if ( entry != node->entries.end() )
            entries.emplace( &*it, entry->annotation );
        ++it;
    }

    if ( _functions.count( node->function ) )
        _functions[ clone ] = _functions[ node->function ];

    node->function = clone;
    node->entries = entries;
}


llvm::Value * AbstractBuilder::process( llvm::Value * v ) {
    if ( llvm::isa< llvm::Instruction >( v ) )
        return _process( llvm::dyn_cast< llvm::Instruction >( v ) );
    else
        return _process( llvm::dyn_cast< llvm::Argument >( v ) );
}

llvm::Value * AbstractBuilder::_process( llvm::Instruction * i ) {
    if ( ignore( i ) ) return nullptr;
    if ( auto abstract = create( i ) ) {
        _values[i] = abstract;
        for ( auto & lift : liftsOf( i ) )
            lift->replaceAllUsesWith( _values[ i ] );
        return abstract;
    }
	return i;
}

llvm::Value * AbstractBuilder::_process( llvm::Argument * arg ) {
    llvm::IRBuilder<> irb( arg->getParent()->front().begin() );
    auto a = types::isAbstract( arg->getType() ) ? arg : lift( arg, irb );
    _values[ arg ] = a;
    return a;
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
                  : types::lift( fty->getReturnType(), AbstractDomain );
    Types argTys;
    if ( !args.empty() ) {
        for ( auto & arg : fn->args() ) {
            bool changed = query::query( args ).any( [&]( ValueNode & n ) {
                               return llvm::cast< llvm::Argument >( n.value ) == &arg;
                           } );
            if ( changed )
                argTys.push_back( types::lift( arg.getType(), AbstractDomain ) );
            else
                argTys.push_back( arg.getType() );
        }
    } else {
        argTys = fty->params();
    }

    auto newfty = llvm::FunctionType::get( ret_type, argTys, fty->isVarArg() );
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
        [&]( llvm::GetElementPtrInst * i ) {
            ret = createGEP( i );
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

void AbstractBuilder::clean( Values & deps ) {
    for ( auto dep : deps ) {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
            for ( auto & lift : liftsOf( inst ) )
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
    auto lifted = llvm::cast< llvm::StructType >( types::lift( i->getAllocatedType(),
                                                            AbstractDomain ) );
    auto rty = lifted->getPointerTo();
    auto tag = intrinsic::tag( i ) + "." + types::basename( lifted ).value();
    llvm::IRBuilder<> irb( i );
    return intrinsic::build( i->getModule(), irb, rty, tag );
}

llvm::Value * AbstractBuilder::createLoad( llvm::LoadInst * i ) {
    auto args = { _values[ i->getOperand( 0 ) ] };
    auto rty = types::lift( i->getType(), AbstractDomain );
    auto base = types::basename( cast< StructType >( types::stripPtr( rty ) ) );
    auto tag = intrinsic::tag( i ) + "." + base.value();
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
    auto rty = types::VoidType( i->getContext() );
    auto base = types::basename( cast< StructType >( types::stripPtr( val->getType() ) ) );
    auto tag = intrinsic::tag( i ) + "." + base.value();

    return intrinsic::build( i->getModule(), irb, rty, tag, { val, ptr } );
}

llvm::Value * AbstractBuilder::createICmp( llvm::ICmpInst * i ) {
    llvm::IRBuilder<> irb( i );
    Values args;
    for ( const auto & op : i->operands() ) {
        auto arg = _values.count( op ) ? _values.at( op ) : lift( op, irb );
        args.push_back( arg );
    }

    auto rty = types::lift( types::BoolType( i->getContext() ), Domain::Value::Abstract );
    auto base = types::basename( cast< StructType >( types::stripPtr( args[0]->getType() ) ) );
	auto tag = intrinsic::tag( i ) + "_" + predicate.at( i->getPredicate() ) + "." + base.value();

    return intrinsic::build( i->getModule(), irb, rty, tag, args );
}

llvm::Value * AbstractBuilder::createBranch( llvm::BranchInst * i ) {
    llvm::IRBuilder<> irb( i );
    if ( i->isUnconditional() ) {
        auto dest = i->getSuccessor( 0 );
        return irb.CreateBr( dest );
    } else {
        llvm::Value * cond;
        if ( _values.count( i->getCondition() ) ) {
            auto tristate =  toTristate( _values[ i->getCondition() ], Domain::Value::Abstract, irb );
            cond = lower( tristate, irb );
        } else {
            cond = i->getCondition();
        }
        auto tbb = i->getSuccessor( 0 );
        auto fbb = i->getSuccessor( 1 );
        return irb.CreateCondBr( cond, tbb, fbb );
    }
}

llvm::Value * AbstractBuilder::createBinaryOperator( llvm::BinaryOperator * i ) {
    llvm::IRBuilder<> irb( i );

    Values args;
    for ( auto & arg : i->operands() ) {
        auto v = _values.count( arg ) ? _values[ arg ] : lift( arg, irb );
        args.push_back( v );
    }
    auto rty = types::lift( i->getType(), AbstractDomain );
    auto st = llvm::cast< llvm::StructType >( types::stripPtr( rty ) );
    auto tag = intrinsic::tag( i ) + "." + types::basename( st ).value();

    return intrinsic::build( i->getModule(), irb, rty, tag, args );
}

llvm::Value * AbstractBuilder::createCast( llvm::CastInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto op = i->getOperand( 0 );
    auto arg = _values.count( op ) ? _values[ op ] : lift( op, irb );

    auto tag = intrinsic::tag( i ) + "." +
               types::llvmname( i->getSrcTy() ) + "." +
               types::llvmname( i->getDestTy() );

    auto rty = types::lift( i->getDestTy(), AbstractDomain );

    return intrinsic::build( i->getModule(), irb, rty, tag, { arg } );
}

llvm::Value * AbstractBuilder::createPhi( llvm::PHINode * n ) {
    auto rty = types::lift( n->getType(), AbstractDomain );

    unsigned int niv = n->getNumIncomingValues();
    llvm::IRBuilder<> irb( n );
    auto phi = irb.CreatePHI( rty, niv );
    _values[ n ] = phi;
    if ( types::isAbstract( n->getType() ) )
        n->replaceAllUsesWith( phi );

    for ( unsigned int i = 0; i < niv; ++i ) {
        auto val = n->getIncomingValue( i );
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
    auto destTy = types::lift( i->getDestTy(), AbstractDomain );
    auto val = _values[ i->getOperand( 0 ) ];
    assert( val && "ERR: Trying to bitcast value, that is not abstracted." );
    return ( val->getType() == destTy ) ? val : irb.CreateBitCast( val, destTy );
}

llvm::Value * AbstractBuilder::createGEP( llvm::GetElementPtrInst * i ) {
    llvm::IRBuilder<> irb( i );
    auto type = types::lift( i->getResultElementType(), AbstractDomain );
    auto val = _values[ i->getPointerOperand() ];
    Values idxs = { i->idx_begin() , i->idx_end() };
    return irb.CreateGEP( type, val, idxs, i->getName() );
}

llvm::Value * AbstractBuilder::lower( llvm::Value * v, llvm::IRBuilder<> & irb ) {
    const auto & type = llvm::cast< llvm::StructType >( v->getType() );
    assert( types::isAbstract( type ) && !type->isPointerTy() );

    auto fty = llvm::FunctionType::get( types::lower( type ), { type }, false );
    auto dom = types::domain( type ).value();
    auto tag = "lart." + Domain::name( dom ) + ".lower";
    if ( dom != Domain::Value::Tristate )
        tag += "." + types::basename( type ).value();
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
    auto call = irb.CreateCall( fn , v );

    _values[ v ] = call;
    return call;
}

llvm::Value * AbstractBuilder::lift( llvm::Value * v, llvm::IRBuilder<> & irb ) {
    auto type = types::lift( v->getType(), AbstractDomain );
    auto fty = llvm::FunctionType::get( type, { v->getType() }, false );
    auto st = llvm::cast< llvm::StructType >( types::stripPtr( type ) );
    auto dom = types::domain( st ).value();
    auto tag = "lart." + Domain::name( dom ) + ".lift." + types::basename( st ).value();
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
    return irb.CreateCall( fn , v );
}

llvm::Value * AbstractBuilder::toTristate( llvm::Value * v, Domain::Value domain,
                                           llvm::IRBuilder<> & irb )
{
    const auto & type = llvm::cast< llvm::StructType >( types::lift( v->getType(), domain ) );
    assert( types::base( type ).value() ==  types::TypeBase::Value::Int1 );
    auto tristate = types::Tristate( v->getType() ).llvm();
    auto fty = llvm::FunctionType::get( tristate, { v->getType() }, false );
    auto tag = "lart." + Domain::name( types::domain( type ).value() ) + ".bool_to_tristate";
    auto fn = irb.GetInsertBlock()->getModule()->getOrInsertFunction( tag, fty );
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
    Values args = { i->arg_operands().begin(), i->arg_operands().end() };
    Types types = argTypes( i );
    auto fn = i->getCalledFunction();
    auto at = types::lift( i->getType(), AbstractDomain );

	bool same = equal( fn->getFunctionType()->params(), types )
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
	auto call = irb.CreateCall( stored, remap( args, _values ) );
    if ( call->getType() == i->getType() )
        i->replaceAllUsesWith( call );
    return call;
}

llvm::Function * AbstractBuilder::getStoredFn( llvm::Function * fn,
                                  llvm::ArrayRef< llvm::Type * >  params)
{
	if ( _functions.count( fn ) )
	  for ( auto stored : _functions[ fn ] )
          if ( equal( stored->getFunctionType()->params(), params ) )
			  return stored;
	return nullptr;
}

Types AbstractBuilder::argTypes( llvm::CallInst * call ) {
    Values args = { call->arg_operands().begin(), call->arg_operands().end() };
    return typesOf( remap( args, _values ) );
}

} // namespace abstract
} // namespace lart
