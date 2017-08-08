// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-types>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

template < typename ...Ts >
using Union = brick::types::Union< Ts... >;

template< typename T >
using Maybe = brick::types::Maybe< T >;

using Type = llvm::Type;
using IntegerType = llvm::IntegerType;
using StructType = llvm::StructType;
using Context = llvm::LLVMContext;

static inline Type * stripPtr( Type * type ) {
    return type->isPointerTy() ? type->getPointerElementType() : type;
}

// TODO solve pointers in TypeBase
namespace TypeBase {

    enum class Value : uint16_t {
        Int1,
        Int8,
        Int16,
        Int32,
        Int64,
        Struct,
        LastBaseType = Struct
    };

    namespace {
    const brick::data::Bimap< Value, std::string > BaseTypeMap = {
         { Value::Int1,  "i1" }
        ,{ Value::Int8,  "i8" }
        ,{ Value::Int16, "i16" }
        ,{ Value::Int32, "i32" }
        ,{ Value::Int64, "i64" }
        ,{ Value::Struct, "struct" }
    };
    } // empty namespace

    using MaybeBase = Maybe< TypeBase::Value >;

    static inline std::string name( const Value & v ) {
        return BaseTypeMap[ v ];
    }

    static inline MaybeBase value( const std::string & s ) {
        if ( BaseTypeMap.right().count( s ) )
            return MaybeBase::Just( BaseTypeMap[ s ] );
        else
            return MaybeBase::Nothing();
    }

    static inline Type * llvm( const Value & v, Context & ctx ) {
        switch( v ) {
            case Value::Int1:
                return llvm::IntegerType::get( ctx, 1 );
            case Value::Int8:
                return llvm::IntegerType::get( ctx, 8 );
            case Value::Int16:
                return llvm::IntegerType::get( ctx, 16 );
            case Value::Int32:
                return llvm::IntegerType::get( ctx, 32 );
            case Value::Int64:
                return llvm::IntegerType::get( ctx, 64 );
            case Value::Struct:
                UNREACHABLE( "Trying to get llvm of struct type." );
        }
        UNREACHABLE( "Trying to get llvm of unsupported type." );
    }

    static inline Value get( Type * t ) {
        auto& ctx = t->getContext();
        if ( t  == llvm::IntegerType::get( ctx, 1 ) )
            return TypeBase::Value::Int1;
        if ( t  == llvm::IntegerType::get( ctx, 8 ) )
            return TypeBase::Value::Int8;
        if ( t  == llvm::IntegerType::get( ctx, 16 ) )
            return TypeBase::Value::Int16;
        if ( t  == llvm::IntegerType::get( ctx, 32 ) )
            return TypeBase::Value::Int32;
        if ( t  == llvm::IntegerType::get( ctx, 64 ) )
            return TypeBase::Value::Int64;
        if ( t->isStructTy() )
            return TypeBase::Value::Struct;
       assert( false );
        UNREACHABLE( "Trying to get base of unsupported type." );
    }
} // namespace TypeBase

using MaybeDomain = Maybe< Domain::Value >;
using MaybeBase = Maybe< TypeBase::Value >;

struct TypeName {
    using StructElements = std::vector< TypeName >;

    TypeName( const StructType * type )
        : _domain( MaybeDomain::Nothing() )
        , _base( MaybeBase::Nothing() )
    {
        parse( type );
    }

    MaybeDomain domain() const { return _domain; }
    MaybeBase base() const { return _base; }

private:
    using Tokens = std::vector< std::string >;

    void parse( const StructType * type ) {
        auto ts = tokens( type );
        _domain = parseDomain( ts );
        _base = parseBase( ts );
    }

    MaybeDomain parseDomain( const Tokens & tokens ) const {
        assert( tokens.size() >= 2 );
        return Domain::value( tokens[1] );
    }

    MaybeBase parseBase( const Tokens & tokens ) const {
        if ( _domain.isNothing() )
            return MaybeBase::Nothing();

        switch( _domain.value() ) {
            case Domain::Value::Tristate :
                return MaybeBase::Just( TypeBase::Value::Int1 );
            case Domain::Value::Struct :
                return MaybeBase::Just( TypeBase::Value::Struct );
            default:
                assert( tokens.size() >= 3 );
                return TypeBase::value( tokens[2] );
        }
        // TODO elements
    }

    static Tokens tokens( const StructType * type ) {
        if ( !type->hasName() )
            return {};
        std::stringstream ss;
        ss.str( type->getStructName().str() );

        std::vector< std::string > ts;
        std::string t;
        while( std::getline( ss, t, '.' ) )
            ts.push_back( t );
        return ts;
    }

    MaybeDomain _domain;
    MaybeBase _base;
    // StructElements elements;
};

// Base class representing abstract type
//
// Stores:
//  'origin()' - original llvm type
//  'domain()' - abstract domain of abstracted type
//
// Enables:
//  'llvm()'   - lowering of 'AbstractType' to llvm representation
//  'name()'   - name of abstract type in llvm representation
//  'base()'   - TODO remove? origin has same purpose
struct AbstractType {

    AbstractType( Type * origin, Domain::Value domain )
        : _origin( origin ), _domain( domain ) {}

    /*AbstractType( Type * abstract ) {
        assert( isAbstract( abstract ) );
        auto st = llvm::cast< StructType >( abstract );
        _origin = TypeBase::llvm( Type( st ).value(), st->getContext() );
        _domain = TypeName( st ).domain().value();
    }*/

    AbstractType( const AbstractType & ) = default;
    AbstractType( AbstractType && ) = default;

    AbstractType &operator=( const AbstractType & other ) = default;
    AbstractType &operator=( AbstractType && other ) = default;

    virtual StructType * llvm() const = 0;

    virtual std::string name() const = 0;

    TypeBase::Value base() const {
        return TypeBase::get( stripPtr( _origin ) );
    }

    std::string baseName() const {
        // TODO ptr?
        return TypeBase::name( base() );
    }

    // TODO let only ScalarType to have domain
    Domain::Value domain() const {
        return _domain;
    }

    std::string domainName() const {
        return Domain::name( _domain );
    }

    Type * origin() const {
        return _origin;
    }
private:
    Type * _origin;
    Domain::Value _domain;
};

using AbstractTypePtr = std::shared_ptr< AbstractType >;


static inline std::string llvmname( const Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

// TODO remove and use AbstractValue.base() instead
static inline Maybe< std::string > basename( const StructType * type ) {
    if ( auto b = TypeName( type ).base() )
        return Maybe< std::string >::Just( TypeBase::name( b.value() ) );
    else
        return Maybe< std::string >::Nothing();
}

static inline bool isAbstract( Type * type ) {
    if ( auto st = llvm::dyn_cast< llvm::StructType >( stripPtr( type ) ) )
        if ( st->hasName() )
            if ( auto dom = TypeName( st ).domain() )
                return dom.value() != Domain::Value::LLVM;
    return false;
}

static inline Type * VoidType( Context & ctx ) {
   return Type::getVoidTy( ctx );
}

static inline Type * BoolType( Context & ctx ) {
    return IntegerType::get( ctx, 1 );
}

} // namespace abstract
} // namespace lart
