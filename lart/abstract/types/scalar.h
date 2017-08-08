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

struct ScalarType : public AbstractType {

    ScalarType( Type * origin, Domain::Value domain )
        : AbstractType( origin, domain )
    {
        assert( origin->isSingleValueType() || origin->isVoidTy() );
    }

    StructType * llvm() const override final {
        const auto & ctx = origin()->getContext();
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name() ) )
            return lookup;
        return llvm::StructType::create( { origin() }, name() );
    }

    std::string name() const override final {
        if ( domain() == Domain::Value::Tristate )
            return "lart." + Domain::name( domain() );
        else
            return "lart." + Domain::name( domain() ) + "."
                  + TypeBase::name( base() );
    }

};

using AbstractInt = ScalarType;

struct Tristate : AbstractInt {
    Tristate( Type * type )
        : AbstractInt( type, Domain::Value::Tristate ) {}
};

} // namespace abstract
} // namespace lart

