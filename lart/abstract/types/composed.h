// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/transform.h>

namespace lart {
namespace abstract {

struct ElementsDomains {
    // TODO make elements domains recursive structure
    // using ElementDomain = brick::Union< Domain::Value, ElementsDomains >;

    ElementsDomains( const StructType * type )
    {
        domains.resize( type->getNumElements(), Domain::Value::LLVM );
    }

    ElementsDomains( const StructType * type, const DomainMap & dmap )
        : ElementsDomains( type )
    {
        for( const auto & e : dmap ) {
            assert( e.first < domains.size() );
            domains[ e.first ] = e.second;
        }
    }

    size_t size() const { return domains.size(); }
    Domain::Value get( size_t i ) const { return domains.at( i ); }

private:
    std::vector< Domain::Value > domains;
};


struct ComposedType : public AbstractType {

    ComposedType( StructType * type, const ElementsDomains & domains )
        : AbstractType( type, Domain::Value::Struct )
    {
        assert( domains.size() == type->getNumElements() );
        for( size_t i = 0; i < type->getNumElements(); ++i ) {
            if ( llvm::dyn_cast< StructType >( type->getElementType( i ) ) ) {
                assert( domains.get( i ) == Domain::Value::Struct );
                NOT_IMPLEMENTED();
            } else {
                assert( domains.get( i ) != Domain::Value::Struct );
                _elements.push_back( std::make_shared< ScalarType >(
                                     type->getElementType( i ),
                                     domains.get( i ) ) );
            }
        }
    }

    ComposedType( StructType * type, const DomainMap & dmap )
        : ComposedType( type, ElementsDomains( type, dmap ) ) {}

    ComposedType( StructType * type )
        : ComposedType( type, { type } ) {}

    StructType * llvm() const override final {
        const auto & ctx = origin()->getContext();
        auto structName = name();
        if ( auto lookup = ctx.pImpl->NamedStructTypes.lookup( structName ) )
            return lookup;

        std::vector< llvm::Type * > types;
        for ( const auto& e : _elements )
            types.push_back( e->llvm() );
        return llvm::StructType::create( types, structName );
    }

    Domain::Value domain() const {
        return Domain::Value::Struct;
    }

    std::string prefixName( StructType * type ) const {
        // TODO anonymous structs
        assert( type->hasName() );
        assert( type->getName().startswith( "struct" ) );
        return type->getName().str();
    }

    std::string elementsName( const std::vector< AbstractTypePtr > & elems ) const {
        std::string n = "[";
        for ( size_t i = 0; i < elems.size(); ++i ) {
            auto b = elems[ i ]->base();
            if ( i != 0 )
                n += ",";
            if ( b == TypeBase::Value::Struct ) {
                // TODO
                assert( false && "Nested structs are not yet supported" );
            } else {
                // TODO pointers
                n += Domain::name( elems[ i ]->domain() ) + "." + TypeBase::name( b );
            }
        }
        n += "]";
        return n;
    }

    // Returns name representing struct, where name obey the following grammar
    //  STRUCT  = lart.struct.<struct name>.[ELEMENT]
    //          | lart.struct.<struct name>.[]
    //  PREFIX  = struct.<struct name>.<?ptr>
    //  ELEMENT = PREFIX | SCALAR | ELEMENT,ELEMENT
    //  SCALAR  = <scalar domain>.<scalar base>.<?ptr>
    //
    // Example:
    //
    // struct S {
    //     sym int a;
    //     struct B {
    //         bool b;
    //     } * B;
    // };
    //
    // then lart name for abstracted Struct is lart.struct.S.[sym.i32,struct.B.ptr]
    std::string name() const override final {
        auto st = llvm::cast< StructType >( origin() );
        assert( st->getNumElements() == _elements.size() );

        // TODO pointer type?
        return "lart." + prefixName( st ) + "." + elementsName( _elements );
        // TODO recursive structures - use original struct name: lart.struct.name...
        // return _name;
        // TODO cache name
    }
private:
    std::vector< AbstractTypePtr > _elements;
};

} // namespace abstract
} // namespace lart

