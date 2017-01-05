// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/sbuilder.h>

#include <lart/abstract/types.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/util.h>

namespace lart {
namespace abstract {

void SubstitutionBuilder::store( llvm::Value * a, llvm::Value * b ) {
    _values[ a ] = b;
}

void SubstitutionBuilder::store( llvm::Function * a, llvm::Function * b ) {
    _functions[ a ] = b;
}

bool SubstitutionBuilder::stored( llvm::Function * fn ) const {
    return _functions.count( fn );
}

void SubstitutionBuilder::process( llvm::Instruction * inst ) {
    llvmcase( inst,
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
        [&]( llvm::ReturnInst * i ) {
            substituteReturn( i );
        },
        [&]( llvm::Instruction *i ) {
            std::cerr << "ERR: unknown instruction: ";
            i->dump();
            std::exit( EXIT_FAILURE );
        } );
}

void SubstitutionBuilder::process( llvm::Argument * arg ) {
    _values[ arg ] = arg;
}

void SubstitutionBuilder::process( llvm::Function * fn ) {
    std::vector < llvm::Type * > types;
    for ( auto &arg : fn->args() ) {
        auto t = types::isAbstract( arg.getType() )
               ? abstraction->abstract( arg.getType() )
               : arg.getType();
        types.push_back( t );
    }
    auto rty = fn->getFunctionType()->getReturnType();
    auto arty = types::isAbstract( rty ) ? abstraction->abstract( rty ) : rty;
    auto fty = llvm::FunctionType::get( arty, types, fn->getFunctionType()->isVarArg() );
    auto newfn = cloneFunction( fn, fty );

    assert( !_functions.count( fn ) );
    _functions[ fn ] = newfn;
}

void SubstitutionBuilder::changeCallFunction( llvm::CallInst * call ) {
    llvm::IRBuilder<> irb( call );
    std::vector< llvm::Value * > args;
    for ( auto & arg : call->arg_operands() )
        args.push_back( arg );
    auto ncall = irb.CreateCall( _functions[ call->getCalledFunction() ], args );
    _values[ call ] = ncall ;
}

void SubstitutionBuilder::substitutePhi( llvm::PHINode * phi ) {
    unsigned int niv = phi->getNumIncomingValues();

    std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > incoming;
    for ( unsigned int i = 0; i < niv; ++i ) {
        auto val = llvm::cast< llvm::Value >( phi->getIncomingValue( i ) );
        auto parent = phi->getIncomingBlock( i );
        if ( _values.count( val ) )
            incoming.push_back( { _values[ val ], parent } );
        else {
            if ( abstraction->is( val ) )
                incoming.push_back( { val, parent } );
        }
    }

    if ( incoming.size() > 0 ) {
        llvm::PHINode * node = nullptr;
        if ( _values.count( phi ) )
            node = llvm::cast< llvm::PHINode >( _values[ phi ] );
        else {
            llvm::IRBuilder<> irb( phi );
            auto type = incoming.begin()->first->getType();
            node = irb.CreatePHI( type, niv );
            _values[ phi ] = node;
        }

        for ( size_t i = 0; i < node->getNumIncomingValues(); ++i )
            node->removeIncomingValue( i,  false );
        for ( auto & in : incoming )
            node->addIncoming( in.first, in.second );
    }
}

void SubstitutionBuilder::substituteSelect( llvm::SelectInst * i ) {
    auto cond = _values.find( i->getCondition() );
    if( cond != _values.end() ) {
        auto tv = _values.find( i->getTrueValue() );
        auto fv = _values.find( i->getFalseValue() );
        if ( tv == _values.end() || fv == _values.end() )
            return;
        llvm::IRBuilder<> irb( i );
        auto newsel = irb.CreateSelect( cond->second, tv->second, fv->second );
        _values[ i ] = newsel;
    }
}

void SubstitutionBuilder::substituteBranch( llvm::BranchInst * br ) {
    assert( br->isConditional() );
    if ( _values.count( br ) )
        return;
    llvm::IRBuilder<> irb( br );
    auto cond = _values.find( br->getCondition() );
    if( cond != _values.end() ) {
        auto tbb = br->getSuccessor( 0 );
        auto fbb = br->getSuccessor( 1 );
        auto newbr = irb.CreateCondBr( cond->second, tbb, fbb );
        _values[ br ] = newbr;
    }
}
void SubstitutionBuilder::substituteCall( llvm::CallInst * call ) {
    if ( intrinsic::is( call ) )
        abstraction->process( call, _values );
    else
        processCall( call );
}

void SubstitutionBuilder::substituteReturn( llvm::ReturnInst * ret ) {
    auto val = _values.find( ret->getReturnValue() );
    if ( val != _values.end() ) {
        llvm::IRBuilder<> irb( ret );
        auto newret = irb.CreateRet( val->second );
        _values[ ret ] = newret;
    }
}

void SubstitutionBuilder::processCall( llvm::CallInst * call ) {
    llvm::Module * m = call->getCalledFunction()->getParent();
    auto name = call->getCalledFunction()->getName();

    auto fn = m->getFunction( name );
    fn = _functions.count( fn ) ? _functions[ fn ] : fn;
    assert ( fn != nullptr );

    std::vector < llvm::Value * > args;

    for ( auto &arg : call->arg_operands() ) {
        if ( types::isAbstract( arg->getType() ) && !_values.count( arg ) )
            //not all incoming values substituted
            //wait till have all args
            break;
        auto tmp = types::isAbstract( arg->getType() ) ? _values[ arg ] : arg;
        args.push_back( tmp );
    }

    //skip if do not have enough substituted arguments
    if ( call->getNumArgOperands() == args.size() ) {
        llvm::IRBuilder<> irb( call );
        auto ncall = irb.CreateCall( fn, args );
        _values[ call ] = ncall;
    }
}

void SubstitutionBuilder::clean( llvm::Function * fn ) {
    auto abstracted = query::query( _values )
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
        _values.erase( v );
    for ( auto & v : abstracted ) {
        v->replaceAllUsesWith( llvm::UndefValue::get( v->getType() ) );
        v->eraseFromParent();
    }
}

void SubstitutionBuilder::clean( llvm::Module & m ) {
    auto abstracted = query::query( _values )
                      .map( [&]( const auto & pair ) {
                          return pair.first;
                      } )
					  .map( query::llvmdyncast< llvm::Instruction > )
					  .filter( query::notnull )
                      .freeze();

    _values.clear();
    for ( auto & v : abstracted ) {
        v->replaceAllUsesWith( llvm::UndefValue::get( v->getType() ) );
        v->eraseFromParent();
    }

    std::vector< llvm::Function * > remove;
    for ( auto it : _functions )
        remove.push_back( it.first );
    for ( auto & fn : remove ) {
        fn->replaceAllUsesWith( llvm::UndefValue::get( fn->getType() ) );
        fn->eraseFromParent();
    }

    auto intrinsics = query::query( m )
                    .map( query::refToPtr )
                    .filter( []( llvm::Function * fn ) {
                        return intrinsic::is( fn );
                    } ).freeze();
    for ( auto & in : intrinsics )
        in->eraseFromParent();
}

} // namespace abstract
} // namespace lart

