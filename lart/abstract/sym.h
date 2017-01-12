// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/common.h>

namespace lart {
namespace abstract {
struct Symbolic : Common {

    Symbolic( llvm::Module & m );

    virtual Constrain constrain( llvm::Value *, llvm::Value * /* constraint */ ) {
        // TODO
        return Constrain();
    }

    virtual llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & );

    virtual bool is( llvm::Type * );

    virtual llvm::Type * abstract( llvm::Type * );
    std::string domain() const { return "sym"; }

    llvm::Type * formula_type;
};

} // namespace abstract
} // namespace lart

