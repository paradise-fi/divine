// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <iostream>
#include <variant>
#include <optional>

#include <brick-data>
#include <brick-types>

#include <lart/support/query.h>

#include <lart/abstract/types.h>
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

template< typename Values >
static inline Types typesOf( const Values & vs ) {
    return query::query( vs ).map( [] ( const auto & v ) {
        return v->getType();
    } ).freeze();
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
        auto p = data.find( fn );
        if ( ( signature( fn ) == sig ) && p == data.end() )
            return fn;
        if ( p != data.end() ) {
            for ( const auto & r : p->second )
                if ( equal( signature( r ), sig ) )
                    return r;
        }
        return nullptr;
    }

    void assign( const Function & a, const Function & b ) {
        data[ a ] = data[ b ];
    }

    void insert( Function a, Function b ) {
        data[ a ].push_back( b );
    }

    size_t count( const Function & fn ) const {
        return data.count( fn );
    }
private:
    Signature signature( const Function & fn ) const {
        return fn->getFunctionType()->params().vec();
    }

    Map< Function, Functions > data;
};

template< typename T >
struct FieldTrie {
    using key_type = size_t;
    using Path = std::vector< key_type >;

    void insert( const Path & keys, T val ) {
        std::get< Internal >( *createPath( keys ) ).setValue( val );
    }

    std::optional< T > search( const Path & keys ) {
        auto level = root.get();
        for ( auto k : keys ) {
            if ( !level )
                return {};
            auto & i = std::get< Internal >( *level );
            if ( !i.children.count( k ) )
                return {};
            level = i.children.at( k ).get();
        }
        return std::get< Internal >( *level ).value();
    }

private:
    struct TrieNode;
    using TrieNodePtr = std::unique_ptr< TrieNode >;

    struct Leaf {
        Leaf( T v ) : value( v ) {}
        T value;
    };

    struct Internal {
        ArrayMap< key_type, TrieNodePtr > children;

        bool isLeaf() const {
            return children.size() == 1 && children.count( 0 ) &&
                   std::holds_alternative< Leaf >( *children.at( 0 ) );
        }

        void setValue( T val ) {
            if ( children.empty() )
                children[ 0 ] = TrieNode::make_leaf( val );
            assert( isLeaf() && value() == val );
        }

        T & value() const {
            assert( isLeaf() );
            return std::get< Leaf >( *children.at( 0 ) ).value;
        }

        TrieNode * getOrInsertChild( key_type key ) {
            if ( !children.count( key ) )
                children[ key ] = TrieNode::make_internal();
            return children[ key ].get();
        }
    };

    using Storage = std::variant< Leaf, Internal >;
    struct TrieNode : Storage {
        using Storage::Storage;

        static TrieNodePtr make_internal() {
            return std::make_unique< TrieNode >( Internal{} );
        }

        static TrieNodePtr make_leaf( T val ) {
            return std::make_unique< TrieNode >( Leaf{ val } );
        }
    };

    TrieNode * createPath( const Path & keys ) {
        if ( !root )
            root = TrieNode::make_internal();
        auto level = root.get();
        for ( auto k : keys ) {
            auto & i = std::get< Internal >( *level );
            level = i.getOrInsertChild( k );
        }
        return level;
    }

    TrieNodePtr root;
};

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

} // namespace abstract
} // namespace lart
