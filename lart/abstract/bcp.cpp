// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/bcp.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

    void BCP::process( llvm::Module & m ) {
        auto assumes = query::query( m ).flatten().flatten()
            .map( query::refToPtr )
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( []( llvm::CallInst * call ) {
                return intrinsic::isAssume( call );
            } )
            .freeze();
        for ( auto ass : assumes )
            process( ass );
    }

    void BCP::process( llvm::Instruction * inst ) {
        //TODO process assume
        //TODO replace all
        inst->eraseFromParent();
    }

} // namespace abstract
} // namespace lart
