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

using brick::types::Union;
using brick::types::Maybe;

using llvm::Type;
using llvm::IntegerType;
using llvm::StructType;
using llvm::LLVMContext;

static inline Type * stripPtr( Type * type ) {
    return type->isPointerTy() ? type->getPointerElementType() : type;
}

enum class TypeBaseValue : uint16_t {
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
const brick::data::Bimap< TypeBaseValue, std::string > TypeBaseTable {
     { TypeBaseValue::Void,  "void" }
    ,{ TypeBaseValue::Int1,  "i1" }
    ,{ TypeBaseValue::Int8,  "i8" }
    ,{ TypeBaseValue::Int16, "i16" }
    ,{ TypeBaseValue::Int32, "i32" }
    ,{ TypeBaseValue::Int64, "i64" }
    ,{ TypeBaseValue::Struct, "struct" }
};
} // anonymous namespace

struct TypeBase {
    TypeBase( TypeBaseValue value ) : value( value ) {}
    std::string name() const { return TypeBaseTable[ value ]; }

    Type * llvm( LLVMContext & ctx ) const {
        switch( value ) {
            case TypeBaseValue::Void:  return Type::getVoidTy( ctx );
            case TypeBaseValue::Int1:  return IntegerType::get( ctx, 1 );
            case TypeBaseValue::Int8:  return IntegerType::get( ctx, 8 );
            case TypeBaseValue::Int16: return IntegerType::get( ctx, 16 );
            case TypeBaseValue::Int32: return IntegerType::get( ctx, 32 );
            case TypeBaseValue::Int64: return IntegerType::get( ctx, 64 );
            case TypeBaseValue::Struct:
                UNREACHABLE( "Trying to get llvm of struct type." );
        }
        UNREACHABLE( "Trying to get llvm of unsupported type." );
    }

    TypeBaseValue value;
};

static TypeBase typebase( const std::string & s ) {
    return TypeBaseTable[ s ];
}

static TypeBase typebase( Type * t ) {
    auto& ctx = t->getContext();
    if ( t->isVoidTy() ) return TypeBaseValue::Void;
    if ( t == llvm::IntegerType::get( ctx, 1 ) )  return TypeBaseValue::Int1;
    if ( t == llvm::IntegerType::get( ctx, 8 ) )  return TypeBaseValue::Int8;
    if ( t == llvm::IntegerType::get( ctx, 16 ) ) return TypeBaseValue::Int16;
    if ( t == llvm::IntegerType::get( ctx, 32 ) ) return TypeBaseValue::Int32;
    if ( t == llvm::IntegerType::get( ctx, 64 ) ) return TypeBaseValue::Int64;
    if ( t->isStructTy() ) return TypeBaseValue::Struct;
    UNREACHABLE( "Trying to get base of unsupported type." );
}


struct TypeNameParser {
    using Tokens = std::vector< std::string >;

    TypeNameParser( StructType * type )
        : tokens( parse( type ) ) {}
    TypeNameParser( const TypeNameParser & ) = default;
    TypeNameParser &operator=( const TypeNameParser & other ) = default;

    Tokens parse( StructType * type ) const {
        if ( !type->hasName() )
            return {};
        std::stringstream ss;
        ss.str( type->getStructName().str() );
        Tokens ts;
        std::string t;
        while( std::getline( ss, t, '.' ) )
            ts.push_back( t );
        return ts;
    }

    DomainPtr getDomain() const { return domain( tokens.at( 1 ) ); }
    TypeBase getBase() const {
        return getDomain()->match(
        [&] ( const UnitDomain & dom ) -> TypeBase {
            switch( dom.value ) {
                case DomainValue::Tristate : return TypeBaseValue::Int1;
                default : return typebase( tokens[2] );
            }
        },
        [] ( const ComposedDomain & /* dom */ ) -> TypeBase {
            return TypeBaseValue::Struct;
        } ).value();
    }

    bool parsable() const {
        if ( tokens.size() == 2 )
            return tokens[ 1 ] == "tristate";
        return tokens.size() > 2;
    }

    Tokens tokens;
};


struct TypeName {
    TypeName( DomainPtr domain, TypeBase base )
        : _domain( domain ), _base( base ) {}
    TypeName( const TypeNameParser & prs )
        : TypeName( prs.getDomain(), prs.getBase() ) {}
    TypeName( StructType * type )
        : TypeName( TypeNameParser( type ) ) {}

    DomainPtr domain() const { return _domain; }
    TypeBase base() const { return _base; }
private:
    DomainPtr _domain;
    TypeBase _base;
};

struct AbstractType {
    AbstractType( Type * origin, DomainPtr domain )
        : _origin( stripPtr( origin ) ),
          _domain( domain ),
          _ptr( origin->isPointerTy() )
    {}

    AbstractType( const AbstractType & ) = default;
    AbstractType( AbstractType && ) = default;
    AbstractType &operator=( const AbstractType & other ) = default;
    AbstractType &operator=( AbstractType && other ) = default;

    virtual Type * llvm() = 0;
    virtual std::string name()  = 0;

    TypeBase base() const { return typebase( stripPtr( _origin ) ); }
    std::string baseName() const { return base().name(); }
    DomainPtr domain() const { return _domain; }
    std::string domainName() { return _domain->name(); }
    Type * origin() const { return _ptr ? _origin->getPointerTo() : _origin; }
    bool pointer() const { return _ptr; }

    friend bool operator==( const AbstractType & l, const AbstractType & r );
private:
    Type * _origin;
    DomainPtr _domain;
    bool _ptr;
};

inline bool operator==( const AbstractType & l, const AbstractType & r ) {
    return std::tie( l._origin, l._domain, l._ptr )
        == std::tie( r._origin, r._domain, r._ptr );
}

using AbstractTypePtr = std::shared_ptr< AbstractType >;

static inline std::string llvmname( const Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

static bool isAbstract( Type * type ) {
    auto st = llvm::dyn_cast< llvm::StructType >( stripPtr( type ) );
    if ( st && st->hasName() && st->getName().startswith( "lart" )
            && TypeNameParser( st ).parsable() )
        return TypeName( st ).domain()->isAbstract();
    return false;
}

static inline Type * VoidType( LLVMContext & ctx ) {
   return Type::getVoidTy( ctx );
}

static inline Type * BoolType( LLVMContext & ctx ) {
    return IntegerType::get( ctx, 1 );
}

} // namespace abstract
} // namespace lart
