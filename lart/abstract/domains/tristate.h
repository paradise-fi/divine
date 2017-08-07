// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/common.h>

namespace lart {
namespace abstract {

struct TristateDomain : Common {

    TristateDomain() = default;

    virtual llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & );

    virtual Constrain constrain( llvm::Value *, llvm::Value * /* constraint */ ) {
        UNSUPPORTED_BY_DOMAIN
    }

    virtual bool is( llvm::Type * ) {
        return false;
    }

    virtual llvm::Type * abstract( llvm::Type * ) {
        UNSUPPORTED_BY_DOMAIN
    }

    Domain::Value domain() const {
        return Domain::Value::Tristate;
    }
};


} // namespace abstract
} // namespace lart
