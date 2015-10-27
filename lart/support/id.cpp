#include <lart/support/id.h>

#if LLVM_MAJOR >= 3 && LLVM_MINOR >= 7

namespace lart {

llvm::MDNode *makeUniqueMDNode( llvm::Module &m ) {
    return llvm::MDNode::getDistinct( m.getContext(), {} );
}

void updateIDs( llvm::Module &m ) {
    for ( auto &f : m )
        for ( auto &b: f )
            for ( auto &i : b )
                if ( !i.getMetadata( "lart.id" ) )
                    i.setMetadata( "lart.id", makeUniqueMDNode( m ) );
}

}
#endif
