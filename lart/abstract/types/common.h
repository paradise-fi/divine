// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
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
    while ( type->isPointerTy() )
        type = type->getPointerElementType();
    return type;
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

struct TypeBase : brick::types::Eq {
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

inline bool operator==(const TypeBase& lhs, const TypeBase& rhs) {
    return lhs.value == rhs.value;
}

inline bool operator==(const TypeBase& lhs, const TypeBaseValue& rhs) {
    return lhs.value == rhs;
}

inline bool operator==(const TypeBaseValue& lhs, const TypeBase& rhs) {
    return lhs == rhs.value;
}

inline bool operator!=(const TypeBase& lhs, const TypeBaseValue& rhs) {
    return !( lhs.value == rhs);
}

inline bool operator!=(const TypeBaseValue& lhs, const TypeBase& rhs) {
    return !(lhs == rhs.value);
}


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

    static const size_t dom_idx = 0;
    static const size_t struct_name_idx = 1;
    static const size_t base_idx = 1;

    TypeNameParser( StructType * type )
        : ctx( type->getContext() )
    {
        std::tie( tokens, elems ) = parse( type );
    }

    TypeNameParser( LLVMContext & ctx, const Tokens & tokens, const Tokens & elems )
        : ctx( ctx ), tokens( tokens ), elems( elems ) {}
    TypeNameParser( const TypeNameParser & ) = default;
    TypeNameParser &operator=( const TypeNameParser & other ) = default;

    DomainPtr getDomain() const {
        if ( tokens.at( dom_idx ) == "struct" ) {
            DomainMap dmap;
            std::vector< Type * > structTypes;
            size_t idx = 0;
            for ( const auto & e : elems ) {
                auto p = parse( e );
                auto tnp = TypeNameParser( ctx, p.first, p.second );
                dmap[ idx ] = tnp.getDomain();
                structTypes.push_back( tnp.getBase().llvm( ctx ) );
                ++idx;
            }
            return Domain::make( StructType::create( structTypes ), dmap );
        } else {
            return domain( tokens.at( dom_idx ) );
        }
    }

    TypeBase getBase() const {
        return getDomain()->match(
        [&] ( const UnitDomain & dom ) -> TypeBase {
            switch( dom.value ) {
                case DomainValue::Tristate : return TypeBaseValue::Int1;
                default : return typebase( tokens.at( base_idx ) );
            }
        },
        [] ( const ComposedDomain & /* dom */ ) -> TypeBase {
            return TypeBaseValue::Struct;
        } ).value();
    }

    static std::pair< Tokens, Tokens > parse( const std::string & name ) {
        using std::istringstream;

        auto elem_begin = name.find( '[' );
        auto elem_end = name.find( ']' );

        istringstream ss;
        if ( elem_begin == std::string::npos )
            ss.str( name );
        else
            ss.str( name.substr( 0, elem_begin ) );

        std::string t;
        Tokens tokens, elems;
        while( std::getline( ss, t, '.' ) )
            tokens.push_back( t );

        if ( elem_begin != std::string::npos ) {
            istringstream es( name.substr(elem_begin + 1, elem_end - elem_begin - 1) );
            while( std::getline( es, t, ',' ) )
                elems.push_back( t );
        }
        return { tokens, elems };
    }

    static std::pair< Tokens, Tokens > parse( StructType * type ) {
        assert( type->hasName() );
        auto name = type->getStructName().str();
        assert( name.substr( 0, 5 ) == "lart." );
        name = name.substr( 5 );
        return parse( name );
    }

    static bool parsable( StructType * type ) {
        if ( type && type->hasName() && type->getName().startswith( "lart" ) ) {
            Tokens tokens, elems;
            std::tie( tokens, elems ) = parse( type );
            if ( tokens.size() == 1 )
                return tokens.at( dom_idx ) == "tristate";
            return tokens.size() > 1;
        }
        return false;
    }

    LLVMContext & ctx;
    Tokens tokens;
    Tokens elems;
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

static bool isAbstract( Type * type ) {
    auto st = llvm::dyn_cast< llvm::StructType >( stripPtr( type ) );
    if ( TypeNameParser::parsable( st ) )
        return TypeName( st ).domain()->isAbstract();
    return false;
}


struct AbstractType {
    AbstractType( Type * origin, DomainPtr domain )
        : _origin( stripPtr( origin ) )
        , _domain( domain )
        , _ptr( origin->isPointerTy() )
    {}

    AbstractType( Type * abstract )
        : _origin( getOriginFrom( abstract ) )
        , _domain( TypeName( llvm::cast< StructType >( stripPtr( abstract ) ) )
                  .domain() )
        , _ptr( abstract->isPointerTy() )
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
    bool isAbstract() { return ::lart::abstract::isAbstract( llvm() ); }

    template< typename A, typename T, typename D >
    static decltype( auto ) make( T && org, D && dom ) {
        return std::make_shared< A >( std::forward< T >( org ), std::forward< D >( dom ) );
    }

    template< typename A, typename T >
    static decltype( auto ) make( T && abstract ) {
        return std::make_shared< A >( std::forward< T >( abstract ) );
    }

    Type * wrapPtr( Type * type ) const {
        return _ptr ? type->getPointerTo() : type;
    }

    friend bool operator==( const AbstractType & l, const AbstractType & r );
private:
    static Type * getOriginFrom( Type * type ) {
        auto sty = llvm::cast< llvm::StructType >( stripPtr( type ) );
        auto tnp = TypeNameParser( sty );
        if ( tnp.getBase().value == TypeBaseValue::Struct ) {
            auto & ctx = type->getContext();
            auto name = tnp.tokens[ TypeNameParser::struct_name_idx ];
            if ( auto s = ctx.pImpl->NamedStructTypes.lookup( name ) )
                return s;
            auto elems = query::query( tnp.elems )
                .map( [] ( const auto & e ) {
                    return TypeNameParser::parse( e );
                } )
                .map( [&] ( const auto & parsed ) {
                    return TypeNameParser( ctx, parsed.first, parsed.second );
                } )
                .map( [&] ( const auto & parser ) {
                    return parser.getBase().llvm( ctx );
                } ).freeze();
            return StructType::create( ctx, elems, name);
        } else {
            return tnp.getBase().llvm( type->getContext() );
        }
    }

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

static inline Type * VoidType( LLVMContext & ctx ) {
   return Type::getVoidTy( ctx );
}

static inline Type * BoolType( LLVMContext & ctx ) {
    return IntegerType::get( ctx, 1 );
}

} // namespace abstract
} // namespace lart
