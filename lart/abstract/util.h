// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>
#include <lart/support/util.h>

#include <lart/abstract/metadata.h>
#include <lart/abstract/domain.h>

namespace lart {
namespace abstract {

std::vector< llvm::Function* > get_potentialy_called_functions( llvm::CallInst* call );

llvm::Function * get_some_called_function( llvm::CallInst * call );

template< typename Fn >
void run_on_potentialy_called_functions( llvm::CallInst * call, Fn functor ) {
    for ( auto fn : get_potentialy_called_functions( call ) )
        functor( fn );
}

using Types = std::vector< llvm::Type * >;
using Values = std::vector< llvm::Value * >;
using Functions = std::vector< llvm::Function * >;

template< typename T >
struct CRTP {
    T& self() { return static_cast< T& >( *this ); }
    T const& self() const { return static_cast< T const& >( *this ); }
};

template< typename Values >
Types types_of( const Values & vs ) {
    return query::query( vs ).map( [] ( const auto & v ) {
        return v->getType();
    } ).freeze();
}

template< typename... Ts >
bool is_one_of( llvm::Value *v ) {
    return ( llvm::isa< Ts >( v ) || ... );
}

template< typename Fn >
void run_on_abstract_calls( Fn functor, llvm::Module &m ) {
    for ( auto &mdv : abstract_metadata( m ) ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( mdv.value() ) ) {
            auto fns = get_potentialy_called_functions( call );

            bool process = query::query( fns ).any( [] ( auto fn ) {
                return fn != nullptr && !fn->isIntrinsic() && !fn->empty();
            } );

            if ( process )
                functor( call );
        }
    }
}

Domain get_domain( llvm::Type *type );

bool is_intr( llvm::CallInst *intr, std::string name );
bool is_lift( llvm::CallInst *intr );
bool is_lower( llvm::CallInst *intr );
bool is_assume( llvm::CallInst *intr );
bool is_thaw( llvm::CallInst *intr );
bool is_freeze( llvm::CallInst *intr );
bool is_tobool( llvm::CallInst *intr );
bool is_cast( llvm::CallInst *intr );

std::string llvm_name( llvm::Type *type );

Values taints( llvm::Module &m );

llvm::Function* get_function( llvm::Argument *a );
llvm::Function* get_function( llvm::Instruction *i );
llvm::Function* get_function( llvm::Value *v );

llvm::Function* get_or_insert_function( llvm::Module*, llvm::FunctionType*, llvm::StringRef );

llvm::Module* get_module( llvm::Value *val );

llvm::Type* abstract_type( llvm::Type *orig, Domain dom );

llvm::Instruction* get_placeholder( llvm::Value *val );
llvm::Instruction* get_unstash_placeholder( llvm::Value *val );
llvm::Instruction* get_placeholder_in_domain( llvm::Value *val, Domain dom );

bool has_placeholder( llvm::Value *val );
bool has_placeholder( llvm::Value *val, const std::string &name );

bool has_placeholder_in_domain( llvm::Value *val, Domain dom );

bool is_placeholder( llvm::Instruction* );

std::vector< llvm::Instruction* > placeholders( llvm::Module & );

bool is_base_type( llvm::Type *type );

inline Domain get_domain( llvm::Argument *arg ) {
    auto fmd = FunctionMetadata( get_function( arg ) );
    return fmd.get_arg_domain( arg->getArgNo() );
}

inline Domain get_domain( llvm::Instruction *inst ) {
    return MDValue( inst ).domain();
}

inline Domain get_domain( llvm::Value *val ) {
    if ( llvm::isa< llvm::Constant >( val ) )
        return Domain::Concrete();
    else if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
        return get_domain( arg );
    else
        return get_domain( llvm::cast< llvm::Instruction >( val ) );
}

inline bool is_concrete( Domain dom ) {
    return dom == Domain::Concrete();
}

inline bool is_concrete( llvm::Value *val ) {
    return is_concrete( get_domain( val ) );
}

inline bool is_terminal_intruction( llvm::Value * val ) {
    return llvm::isa< llvm::ReturnInst >( val ) ||
           llvm::isa< llvm::UnreachableInst >( val );
}

inline llvm::Value * returns_abstract_value( llvm::Function * fn ) {
    auto retty = fn->getReturnType();
    if ( retty->isVoidTy() || retty->isPointerTy() )
        // TODO check abstraction settings for ignored types
        return nullptr; // no return value to stash

    auto rets = query::query( *fn ).flatten()
        .map( query::refToPtr )
        .filter( is_terminal_intruction )
        .freeze();

    ASSERT( rets.size() == 1 && "No single terminator instruction found." );
    if ( llvm::isa< llvm::UnreachableInst >( rets[ 0 ] ) )
        return nullptr; // do not unstash from noreturn functions

    return rets[ 0 ];
}

} // namespace abstract
} // namespace lart
