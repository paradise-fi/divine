// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/cleanup.h>

#include <lart/abstract/domain.h>

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
}

} // namespace abstract
} // namespace lart
