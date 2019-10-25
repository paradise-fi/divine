// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>

namespace lart::divine
{

struct Coverage {

    static PassMeta meta() {
        return passMeta< Coverage >( "coverage", "" );
    }

    llvm::Function * _choose;
    std::vector< llvm::CallSite > _chooses;

    void run( llvm::Module &m );

    void assign_indices();
};

} // namespace lart::divine
