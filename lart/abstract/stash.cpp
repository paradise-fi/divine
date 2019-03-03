// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract {

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
            process_arguments( call );

            run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                if ( !meta::has( fn, meta::tag::abstract ) )
                    if ( !stashed.count( fn ) )
                        process_return_value( call, fn );
                stashed.insert( fn );
            } );
        }
    }, m );
}

void Stash::process_return_value( CallInst *call, Function * fn ) {
    if ( auto ret = returns_abstract_value( call, fn ) ) {
        AbstractPlaceholderBuilder builder{ call->getContext() };
        auto stash = builder.construct( llvm::cast< llvm::ReturnInst >( ret ) );
        meta::abstract::inherit( stash.inst, call );
    }
}

void Stash::process_arguments( CallInst *call ) {
    IRBuilder<> irb( call );

    for ( unsigned idx = 0; idx < call->getNumArgOperands(); ++idx ) {
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        if ( isa< Constant >( op ) )
            continue;

        auto dom = Domain::get( op );
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
            meta::abstract::inherit( stash, op );
        }
    }
}

void Unstash::run( Module &m ) {
    run_on_abstract_calls( [&] ( auto call ) {
        if ( !is_transformable( call ) ) {
            run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                if ( !meta::has( fn, meta::tag::abstract ) )
                    if ( !unstashed.count( fn ) )
                        process_arguments( call, fn );
                unstashed.insert( fn );
            } );

            process_return_value( call );
        }
    }, m );
}

void Unstash::process_arguments( CallInst *call, Function * fn ) {
    IRBuilder<> irb( &*fn->getEntryBlock().begin() );

    for ( auto &arg : fn->args() ) {
        auto idx = arg.getArgNo();
        auto op = call->getArgOperand( idx );
        auto ty = op->getType();

        auto dom = Domain::get( &arg );
        if ( is_concrete( dom ) )
            continue;

        auto m = get_module( call );
        if ( is_base_type_in_domain( m, &arg, dom ) ) {
            auto aty = abstract_type( ty, dom );
            auto unstash_fn = unstash_placeholder( m, op, aty );
            auto unstash = irb.CreateCall( unstash_fn, { &arg } );
            meta::abstract::inherit( unstash, &arg );
        }
    }
}

void Unstash::process_return_value( llvm::CallInst * call ) {
    auto returns = query::query( get_potentialy_called_functions( call ) )
        .filter( [call] ( const auto & fn ) {
            return returns_abstract_value( call, fn );
        } )
        .freeze();

    if ( !returns.empty() ) {
        AbstractPlaceholderBuilder builder{ call->getContext() };
        builder.construct< Placeholder::Type::Unstash >( call );
    }
}

} // namespace lart::abstract
