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
    std::string domain() const { return "zero"; }
    std::string LLVMTypeName() const { return "struct.abstract::Zero"; }

    struct Tristate {
        Tristate( llvm::Module & );
        virtual bool is( llvm::Type * );
        virtual llvm::Type * abstract( llvm::Type * );
        std::string domain() const { return "tristate"; }
        std::string LLVMTypeName() const { return "struct.abstract::Tristate"; }

        llvm::Type * tristate_type;
    };

    Tristate tristate;

    std::map< llvm::Value *, llvm::Value * > from_tristate_map;
    llvm::Type * zero_type;
};


} // namespace abstract
} // namespace lart
