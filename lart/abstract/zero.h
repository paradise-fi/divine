// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/common.h>

namespace lart {
namespace abstract {

struct Zero : Common {

    Zero( llvm::Module & m );

    virtual Constrain constrain( llvm::Value *, llvm::Value * /* constraint */ ) {
        return Constrain();
    }

    virtual llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & );

    virtual bool is( llvm::Type * );

    virtual llvm::Type * abstract( llvm::Type * );

    Domain::Value domain() const {
        return Domain::Value::Zero;
    }


    llvm::Type * zero_type;
};


} // namespace abstract
} // namespace lart
