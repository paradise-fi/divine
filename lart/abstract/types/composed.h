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
#include <lart/abstract/types/scalar.h>

namespace lart {
namespace abstract {

using TypeContainer = std::vector< AbstractTypePtr >;
static std::string structTypeName( llvm::StructType * type );
static std::string elementsName( const TypeContainer & elems );
AbstractTypePtr liftType( llvm::Type * type, DomainPtr dom );

struct ComposedType;
using ComposedTypePtr = std::shared_ptr< ComposedType >;

struct ComposedType : public AbstractType {
    ComposedType( Type * type, DomainPtr domain )
        : AbstractType( type, domain )
    {
        auto sty = llvm::cast< StructType >( stripPtr( type ) );
        const auto & values = domain->cget< ComposedDomain>().values;
        assert( values.size() == sty->getNumElements() );
        auto dom = values.begin();
        for ( const auto & elem : sty->elements() ) {
            _elements.push_back( liftType( elem, *dom ) );
            ++dom;
        }
    }

    ComposedType( Type * type, const DomainMap & dmap )
        : ComposedType( type, Domain::make( type, dmap ) ) {}

    Type * llvm() override final {
        const auto & ctx = origin()->getContext();
        auto structName = name();
        Type * res;
        if ( auto lookup = ctx.pImpl->NamedStructTypes.lookup( structName ) ) {
            res = lookup;
        } else {
            std::vector< llvm::Type * > types;
            for ( const auto& e : _elements )
                types.push_back( e->llvm() );
            res = llvm::StructType::create( types, structName );
        }
        return pointer() ? res->getPointerTo() : res;
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
    std::string name() override final {
        auto st = llvm::cast< StructType >( stripPtr( origin() ) );
        assert( st->getNumElements() == _elements.size() );
        return "lart." + structTypeName( st ) + "." + elementsName( _elements );
        // TODO recursive structures - use original struct name: lart.struct.name...
        // return _name;
        // TODO cache name
    }

    static ComposedTypePtr make( llvm::Type * type, const DomainMap & dmap ) {
        return std::make_shared< ComposedType >( type, dmap );
    }

    static ComposedTypePtr make( llvm::Type * type, DomainPtr dom ) {
        return std::make_shared< ComposedType >( type, dom );
    }
private:
    TypeContainer _elements;
};

static std::string structTypeName( llvm::StructType * type ) {
    // TODO anonymous structs
    assert( type->hasName() );
    assert( type->getName().startswith( "struct" ) );
    return type->getName().str();
}

static std::string elementsName( const TypeContainer & elems ) {
    std::string n = "[";
    for ( size_t i = 0; i < elems.size(); ++i ) {
        if ( i != 0 ) n += ",";
        if ( elems[ i ]->base().value == TypeBaseValue::Struct )
            assert( false && "Nested structs are not yet supported" ); // TODO
        else
            n += elems[ i ]->domainName() + "." + elems[ i ]->baseName();
        if ( elems[ i ]->pointer() ) n += ".ptr";
    }
    n += "]";
    return n;
}


} // namespace abstract
} // namespace lart

