// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>

#include <lart/abstract/walker.h>

#include <lart/analysis/postorder.h>

#include <vector>

namespace lart {
namespace abstract {

AbstractWalker::AbstractWalker( llvm::Module & m ) : _m(&m) {
    annotations = getAbstractAnnotations( m );
}

using Value = llvm::Value;
using Function = llvm::Function;
using Instruction = llvm::Instruction;

std::vector< Function * > AbstractWalker::functions() {
    assert( _m );
    std::set< Function * > fns;

    for ( const auto &a : annotations )
        fns.insert( a.alloca_->getParent()->getParent() );

    return analysis::callPostorder< Function * >( *_m, { fns.begin(), fns.end() } );
}

std::vector< Value * > AbstractWalker::entries( Function * fn ) {
    std::vector< Value * > entries;
    for ( const auto & a : annotations )
        if ( a.alloca_->getParent()->getParent() == fn )
            entries.push_back( a.alloca_ );
    return entries;
}
std::vector< Value * > AbstractWalker::dependencies( Value * v ) {
    return analysis::postorder< Value * >( v );
}

} // namespace abstract
} // namespace lart
