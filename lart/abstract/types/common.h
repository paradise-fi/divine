// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-types>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {
namespace types {

template< typename T >
using Maybe = brick::types::Maybe< T >;

using Type = llvm::Type;
using IntegerType = llvm::IntegerType;
using StructType = llvm::StructType;
using Context = llvm::LLVMContext;

namespace TypeBase {

    enum class Value : uint16_t {
        Void,
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
         { Value::Void,  "void" }
        ,{ Value::Int1,  "i1" }
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
            case Value::Void:
                return Type::getVoidTy( ctx );
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
} // namespace TypeBase

using MaybeDomain = Maybe< Domain::Value >;
using MaybeBase = Maybe< TypeBase::Value >;

struct TypeName {
    using StructElements = std::vector< TypeName >;

    TypeName( const StructType * type )
        : domain( MaybeDomain::Nothing() ), base( MaybeBase::Nothing() )
    {
        parse( type );
    }

    // type format: lart.<domain>.<lower type>.<domain e1>.<type e1>.<domain e2>...
    void parse( const StructType * type ) {
        auto ts = tokens( type );
        assert( ts.size() >= 2 );
        domain = Domain::value( ts[1] );
        if ( !domain.isNothing() ) {
            if ( domain.value() == Domain::Value::Tristate ) {
                base = MaybeBase::Just( TypeBase::Value::Int1 );
            } else {
                assert( ts.size() >= 3 );
                base = TypeBase::value( ts[2] );
            }
            // TODO elements
        }
    }

    static std::vector< std::string > tokens( const StructType * type ) {
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

    MaybeDomain domain;
    MaybeBase base;
    StructElements elements;
};

static inline Type * stripPtr( Type * type ) {
    return type->isPointerTy() ? type->getPointerElementType() : type;
}

static inline MaybeDomain domain( const StructType * type ) {
    return TypeName( type ).domain;
}

static inline MaybeBase base( const StructType * type ) {
    return TypeName( type ).base;
}

static inline Maybe< std::string > basename( const StructType * type ) {
    auto b = base( type );
    return b.isNothing() ? Maybe< std::string >::Nothing()
                         : Maybe< std::string >::Just( TypeBase::name( b.value() ) );
}

static inline std::string llvmname( const Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

static inline bool isAbstract( Type * type ) {
    type = stripPtr( type );
    if ( auto st = llvm::dyn_cast< llvm::StructType >( type ) ) {
        auto tokens = TypeName::tokens( st );
        if ( tokens.size() < 2 )
            return false;
        auto dom = domain( st );
        if ( !dom.isNothing() )
            return dom.value() != Domain::Value::LLVM;
    }
    return false;
}

static inline Type * origin( StructType * type ) {
    assert( isAbstract( type ) );
    return TypeBase::llvm( base( type ).value(), type->getContext() );
}

static inline Type * VoidType( Context & ctx ) {
   return Type::getVoidTy( ctx );
}

static inline Type * BoolType( Context & ctx ) {
    return IntegerType::get( ctx, 1 );
}

} //namespace types
} // namespace abstract
} // namespace lart
