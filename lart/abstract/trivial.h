// -*- C++ -*- (c) 2014 Petr Rockai <me@mornfall.net>
// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/common.h>

namespace lart {
namespace abstract {

struct Trivial : Common {
    virtual Constrain constrain( llvm::Value *, llvm::Value * /* constraint */ ) {
        return Constrain();
    }

    virtual llvm::Value * process( llvm::CallInst *, std::vector< llvm::Value * > & );

    virtual bool is( llvm::Type * );

    virtual llvm::Type * abstract( llvm::Type * );
    std::string domain() const { return "trivial"; }
};

} // namespace abstract
} // namespace lart
