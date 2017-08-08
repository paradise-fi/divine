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

struct ScalarType;
using ScalarTypePtr = std::shared_ptr< ScalarType >;

struct ScalarType : public AbstractType {

    ScalarType( Type * origin, DomainValue dom )
        : AbstractType( origin, Domain::make( dom ) )
    {
        assert( origin->isSingleValueType() || origin->isVoidTy() );
    }

    ScalarType( Type * origin, DomainPtr dom )
        : AbstractType( origin, dom )
    {
      assert( origin->isSingleValueType() || origin->isVoidTy() );
      assert( dom->is< UnitDomain >() );
    }

    Type * llvm() override final {
        const auto & ctx = origin()->getContext();
        Type * res = nullptr;
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name() ) )
            res = lookup;
        else
            res = llvm::StructType::create( { origin() }, name() );
        return pointer() ? res->getPointerTo() : res;
    }

    std::string name() override final {
        auto dom = domain()->cget< UnitDomain >();
        if ( dom.value == DomainValue::Tristate )
            return "lart." + domainName();
        else
            return "lart." + domainName() + "." + baseName();
    }

    static ScalarTypePtr make( Type * origin, DomainValue dom ) {
        return std::make_shared< ScalarType >( origin, dom );
    }

    static ScalarTypePtr make( Type * origin, DomainPtr dom ) {
        return std::make_shared< ScalarType >( origin, dom );
    }
};

template< size_t bw >
struct AbstractInt : ScalarType {
    AbstractInt( Type * type, DomainValue domain )
        : ScalarType( type, domain )
    { assert( type->isIntegerTy( bw ) ); }
};

struct Tristate : AbstractInt< 1 > {
    Tristate( Type * type ) : AbstractInt< 1 >( type, DomainValue::Tristate ) {}
};

} // namespace abstract
} // namespace lart

