// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

using namespace lart::abstract;
using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

namespace {

Function* stash_placeholder( Module *m, Type *in ) {
    auto void_type = Type::getVoidTy( m->getContext() );
    auto fty = llvm::FunctionType::get( void_type, { in }, false );
    std::string name = "lart.stash.placeholder.";
    if ( auto s = dyn_cast< StructType >( in ) )
        name += s->getName().str();
    else
        name += llvm_name( in );
    return get_or_insert_function( m, fty, name );
}

Function* unstash_placeholder( Module *m, Value *val, Type *out ) {
    auto fty = llvm::FunctionType::get( out, { val->getType() }, false );
    std::string name = "lart.unstash.placeholder.";
    if ( auto s = dyn_cast< StructType >( out ) )
        name += s->getName().str();
    else
        name += llvm_name( out );
    return get_or_insert_function( m, fty, name );
}


} // anonymous namespace

void Stash::run( Module &m ) {
    run_on_abstract_calls( [&] ( auto call ) {
        if ( !is_transformable( call ) ) {
            run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                if ( !fn->getMetadata( "lart.abstract" ) )
                    if ( !stashed.count( fn ) )
                        arg_unstash( call, fn );
                stashed.insert( fn );
            } );

            ret_unstash( call );
        }
    }, m );

    stashed.clear();

    run_on_abstract_calls( [&] ( auto call ) {
        if ( !is_transformable( call ) ) {
            arg_stash( call );

            run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                if ( !fn->getMetadata( "lart.abstract" ) )
                    if ( !stashed.count( fn ) )
                        ret_stash( call, fn );
                stashed.insert( fn );
            } );
        }
    }, m );
}

void Stash::arg_unstash( CallInst *call, Function * fn ) {
    if ( fn->empty() ) {
        throw std::runtime_error( "Trying to transform undefined function: " + fn->getName().str() );
    }

    IRBuilder<> irb( &*fn->getEntryBlock().begin() );

    FunctionMetadata fmd{ fn };

    for ( auto &arg : fn->args() ) {
        auto idx = arg.getArgNo();
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        auto dom = fmd.get_arg_domain( idx );
        if ( is_concrete( dom ) )
            continue;

        auto m = get_module( call );
        if ( is_base_type_in_domain( m, &arg, dom ) ) {
            auto aty = abstract_type( ty, dom );
            auto unstash_fn = unstash_placeholder( m, op, aty );
            auto unstash = irb.CreateCall( unstash_fn, { &arg } );
            add_abstract_metadata( unstash, dom );
        }
    }
}

void Stash::arg_stash( CallInst *call ) {
    IRBuilder<> irb( call );

    for ( unsigned idx = 0; idx < call->getNumArgOperands(); ++idx ) {
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        if ( isa< Constant >( op ) )
            continue;

        auto dom = get_domain( op );
        if ( is_concrete( dom ) )
            continue;

        auto m = get_module( call );
        if ( is_base_type_in_domain( m, op, dom ) ) {
            auto aty = abstract_type( ty, dom );
            auto stash_fn = stash_placeholder( m, aty );

            Instruction * stash = nullptr;
            if ( isa< CallInst >( op ) || isa< Argument >( op ) )
                stash = irb.CreateCall( stash_fn, { get_unstash_placeholder( op ) } );
            else if ( has_placeholder( op ) )
                stash = irb.CreateCall( stash_fn, { get_placeholder( op ) } );
            else {
                auto undef = UndefValue::get( stash_fn->getFunctionType()->getParamType( 0 ) );
                stash = irb.CreateCall( stash_fn, { undef } );
            }
            add_abstract_metadata( stash, dom );
        }
    }
}

void Stash::ret_stash( CallInst *call, Function * fn ) {
    auto dom = get_domain( call );
    if ( auto terminator = returns_abstract_value( call, fn ) ) {
        auto ret = cast< ReturnInst >( terminator );
        auto val = ret->getReturnValue();
        auto aty = abstract_type( fn->getReturnType(), dom );

        IRBuilder<> irb( ret );
        auto stash_fn = stash_placeholder( get_module( call ), aty );

        Value *tostash = nullptr;
        if ( has_placeholder( val ) ) {
            tostash = get_placeholder( val );
        } else if ( isa< Argument >( val ) || isa< CallInst >( val ) ) {
            if ( !has_placeholder( val, "lart.unstash.placeholder" ) ) {
                return; // does not return abstract value
            }
            tostash = get_unstash_placeholder( val );
        } else {
            tostash = UndefValue::get( aty );
        }

        auto stash = irb.CreateCall( stash_fn, { tostash } );
        add_abstract_metadata( stash, dom );
        make_duals( stash, ret );
    }
}

void Stash::ret_unstash( CallInst *call ) {
    auto dom = get_domain( call );

    Values terminators;
    run_on_potentialy_called_functions( call, [&] ( auto fn ) {
        terminators.push_back( returns_abstract_value( call, fn ) );
    } );

    size_t num_of_returns = std::count( terminators.begin(), terminators.end(), nullptr );

    if ( num_of_returns != terminators.size() ) { // there is at least one return
        auto fty = cast< FunctionType >( call->getCalledValue()->stripPointerCasts()
                                             ->getType()->getPointerElementType() );

        auto aty = abstract_type( fty->getReturnType(), dom );

        IRBuilder<> irb( call );

        Instruction * arg = call;
        if ( auto ce = dyn_cast< ConstantExpr >( call->getCalledValue() ) ) {
            ASSERT( ce->isCast() );
            Type * from = fty->getReturnType();
            Type * to = call->getType();
            if ( to->isPointerTy() && from->isIntegerTy() )
                arg = cast< Instruction >( irb.CreatePtrToInt( call, fty->getReturnType() ) );
            else if ( to == from )
                arg = call;
            else
                UNREACHABLE( "Unsupported unstash of constant expression." );
        }

        auto unstash_fn = unstash_placeholder( get_module( call ), arg, aty );
        auto unstash = irb.CreateCall( unstash_fn, { arg } );
        add_abstract_metadata( unstash, dom );

        call->removeFromParent();
        if ( call == arg )
            call->insertBefore( unstash );
        else
            call->insertBefore( arg );

        make_duals( unstash, call );
    }
}
