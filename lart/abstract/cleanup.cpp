// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/cleanup.h>

#include <lart/abstract/domain.h>
#include <lart/abstract/stash.h> // unpacked_argument

#include <lart/support/cleanup.h>
#include <lart/support/query.h>

namespace lart {
namespace abstract {

void Cleanup::run( llvm::Module &m ) {
    auto insts = query::query( m ).flatten().flatten()
        .map( query::refToPtr )
        .freeze();

    for ( auto i : insts ) {
        i->dropUnknownNonDebugMetadata();
    }

    // erase argument matching intrinsics
    auto argintrs = unpacked_arguments( &m );
    auto intrs = query::query( argintrs )
        .map( query::llvmdyncast< llvm::CallInst > )
        .map( [] ( auto call ) {
            return call->getCalledFunction();
        } )
        .freeze();

    std::set< llvm::Function * > unique( intrs.begin(), intrs.end() );

    for ( auto i : argintrs )
        i->eraseFromParent();

    for ( auto u : unique )
        u->eraseFromParent();
}

} // namespace abstract
} // namespace lart
