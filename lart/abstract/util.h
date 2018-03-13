// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <iostream>
#include <optional>

#include <brick-data>
#include <brick-types>

#include <lart/support/query.h>

#include <lart/abstract/types.h>
#include <lart/analysis/postorder.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

using brick::types::Union;
using brick::types::Maybe;

using brick::data::ArrayMap;
using brick::data::Bimap;

template< typename T >
using Lift = std::pair< T, Domain >;

template< typename K, typename V >
using Map = std::map< K, V >;

template< typename T >
using ReMap = Map< T, T >;

template < typename V >
using Set = std::unordered_set< V >;

using Types = std::vector< llvm::Type * >;
using Values = std::vector< llvm::Value * >;
using Functions = std::vector< llvm::Function * >;

using Insts = std::vector< llvm::Instruction * >;

using Args = Values;
using ArgTypes = Types;

using ArgDomains = std::vector< Domain >;
using ArgIndices = std::vector< size_t >;

template< typename T >
using ConstifyPtr = std::add_pointer_t< std::add_const_t< std::remove_pointer_t< T > > >;

template< typename Values >
static inline Types types_of( const Values & vs ) {
    return query::query( vs ).map( [] ( const auto & v ) {
        return v->getType();
    } ).freeze();
}

static inline std::string llvm_name( const llvm::Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

template< typename... Ts >
inline bool is_one_of( llvm::Value *v ) {
    return ( llvm::isa< Ts >( v ) || ... );
}

inline Values taints( llvm::Module &m ) {
    Values res;
    for ( auto &fn : m )
        if ( fn.getName().startswith( "__vm_test_taint" ) )
            for ( auto u : fn.users() )
                res.push_back( u );
    return res;
}

template< typename Range >
static inline bool equal( const Range & a, const Range & b ) {
    return std::equal( a.begin(), a.end(), b.begin(), b.end() );
}

template< typename K, typename V >
struct LiftMap {
    using MaybeK = Maybe< K >;
    using MaybeV = Maybe< V >;

    MaybeV safeLift( const K & key ) const {
        return data.left().count( key )
            ? MaybeV::Just( data.left()[ key ] )
            : MaybeV::Nothing();
    }

    MaybeK safeOrigin( const V & val ) const {
        return data.right().count( val )
             ? MaybeK::Just( data.right()[ val ] )
             : MaybeK::Nothing();
    }

    const V& lift( const K & key ) const {
        assert( data.left().count( key ) );
        return data.left()[ key ];
    }

    const K& origin( const V & val ) const {
        assert( data.right().count( val ) );
        return data.right()[ val ];
    }

    void insert( const K & key, const V & val ) {
        data.insert( key, val );
    }

    void erase( const K & key, const V & val ) {
        if ( data.left().count( key ) && data.right().count( val ) )
            data.erase( { key, val } );
    }

    void clear() {
        data.clear();
    }

    Bimap< K, V > data;
};

template< typename Function, typename Signature >
struct FunctionMap {
    Function get( const Function & fn, const Signature & sig ) const {
        auto p = _data.find( fn );
        if ( p != _data.end() ) {
            auto val = p->second.find( sig );
            if ( val != p->second.end() )
                return val->second;
        }
        return nullptr;
    }

    void assign( const Function & a, const Function & b ) {
        _data[ a ].insert( _data[ b ].begin(), _data[ b ].end() );
    }

    void insert( Function a, Signature s, Function b ) {
        _data[ a ][ s ] = b;
    }

    size_t count( const Function & fn ) const {
        return _data.count( fn );
    }
private:
    Map< Function, Map< Signature, Function > > _data;
};

static inline bool isScalarType( llvm::Type * type ) {
    return type->isIntegerTy();
}

static inline bool isScalarType( llvm::Value * val ) {
    return isScalarType( val->getType() );
}

template< typename Vs, typename Fn >
static inline auto remapFn( const Vs & vs, Fn fn ) {
    return query::query( vs ).map( [&] ( const auto & v ) {
        return fn( v );
    } ).freeze();
}

template< typename Vs, typename RMap >
static inline Vs remap( const Vs & vs, const RMap & rmap ) {
    return remapFn( vs, [&] ( const auto & v ) {
        auto l = rmap.safeLift( v );
        return l.isJust() ? l.value() : v;
    } );
}

template< typename I, typename Vs >
static inline auto filterA( const Vs & vs ) {
    return query::query( vs ).filter( []( const auto & n ) {
        return n.template isa< I >();
    } ).freeze();
}

template< typename I, typename Vs >
static inline auto llvmFilter( const Vs & vs ) {
    return query::query( vs )
        .map( query::llvmdyncast< I > )
        .filter( query::notnull )
        .freeze();
};

template< typename I >
static inline auto llvmFilter( llvm::Function * fn ) {
    return llvmFilter< I >( query::query( *fn ).flatten()
        .map( query::refToPtr ).freeze() );
};

template< typename I >
static inline auto llvmFilter( llvm::Module * m ) {
    return llvmFilter< I >( query::query( *m ).flatten().flatten()
        .map( query::refToPtr ).freeze() );
};

static inline llvm::Value * argAt( llvm::Function * fn, size_t idx ) {
    return std::next( fn->arg_begin(), idx );
}

static inline llvm::Function * get_function( llvm::Argument * a ) {
    return a->getParent();
}

static inline llvm::Function * get_function( llvm::Instruction * i ) {
    return i->getParent()->getParent();
}

static inline llvm::Function * get_function( llvm::Value * val ) {
    if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
        return get_function( arg );
    return get_function( llvm::cast< llvm::Instruction >( val ) );
}

static inline llvm::Module * get_module( llvm::Value * val ) {
    return get_function( val )->getParent();
}

static inline llvm::Function * getFunction( llvm::Argument * a ) {
    return a->getParent();
}

static inline llvm::Function * getFunction( llvm::Instruction * i ) {
    return i->getParent()->getParent();
}

static inline llvm::Function * getFunction( llvm::Value * val ) {
    if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
        return getFunction( arg );
    return getFunction( llvm::cast< llvm::Instruction >( val ) );
}

static inline llvm::Module * getModule( llvm::Value * val ) {
    return getFunction( val )->getParent();
}

template< typename Arguments >
static inline ArgDomains argDomains( llvm::Function * fn, const Arguments & args ) {
    auto doms = ArgDomains( fn->arg_size(),  Domain::LLVM );
    for ( const auto & a : args )
        doms[ a.template get< llvm::Argument >()->getArgNo() ] = a.domain;
    return doms;
};

template< typename Arguments >
static inline ArgDomains argDomains( const Arguments & args ) {
    if ( args.empty() )
        return {};
    auto fn = getFunction( args[ 0 ].value );
    return argDomains( fn, args );
};

template< typename Arguments >
static inline std::vector< size_t > argIndices( const Arguments & args ) {
    std::vector< size_t > ind;
    for ( auto & a : args )
        ind.push_back( llvm::cast< llvm::Argument >( a.value )->getArgNo() );
    std::sort( ind.begin(), ind.end() );
    return ind;
}


static inline bool isBaseStructTy( llvm::Type * type ) {
    return stripPtrs( type )->isStructTy();
}

static inline bool operatesWithStructTy( llvm::Value * value ) {
    if ( llvm::isa< llvm::GetElementPtrInst >( value ) && !isBaseStructTy( value->getType() ) )
        return false;
    if ( isBaseStructTy( value->getType() ) )
        return true;
    if ( auto user = llvm::dyn_cast< llvm::User >( value ) )
        for ( const auto & o : user->operands() )
            if ( isBaseStructTy( o->getType() ) )
                return true;
    return false;
}

inline Values value_succs( llvm::Value *v ) { return { v->user_begin(), v->user_end() }; }

inline Values reach_from( Values roots ) {
    return lart::analysis::postorder( roots, value_succs );
};

inline Values reach_from( llvm::Value *root ) { return reach_from( Values{ root } ); }

inline llvm::Type * void_type( llvm::LLVMContext &ctx ) {
    return llvm::Type::getVoidTy( ctx );
}

} // namespace abstract
} // namespace lart
