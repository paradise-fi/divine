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

namespace lart {
namespace abstract {
namespace types {

using Type = llvm::Type;
using StructType = llvm::StructType;

struct AbstractType {
    using AbstractTypePtr = std::shared_ptr< AbstractType >;

    explicit AbstractType( Type * origin, Domain::Value domain )
        : origin( origin ), domain( domain ) {}

    explicit AbstractType( Type * abstract ) {
        assert( isAbstract( abstract ) );
        auto st = llvm::cast< StructType >( abstract );
        origin = types::origin( st );
        domain = types::domain( st ).value();
    }

    virtual StructType * llvm() = 0;

    virtual std::string name() = 0;

    Type * origin;
    Domain::Value domain;
};


template< TypeBase::Value base >
struct ScalarBase : AbstractType {
    static_assert( base != TypeBase::Value::Struct );
    using AbstractType::AbstractType;

    StructType * llvm() override final {
        const auto & ctx = this->origin->getContext();
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name() ) )
            return lookup;
        return llvm::StructType::create( { this->origin }, name() );
    }
};


template< TypeBase::Value base >
struct AbstractInt : ScalarBase< base > {
    using ScalarBase< base >::ScalarBase;

    std::string name() override {
        return "lart." + Domain::name( this->domain ) + "." + TypeBase::name( base );
    }
};


struct Tristate : AbstractInt< TypeBase::Value::Int1 > {
    explicit Tristate( Type * type )
        : AbstractInt( type, Domain::Value::Tristate ) {}

    std::string name() override final {
        return "lart." + Domain::name( this->domain );
    }
};


} //namespace types
} // namespace abstract
} // namespace lart

