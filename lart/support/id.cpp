#include <lart/support/id.h>

namespace lart {

typedef llvm::ArrayRef< llvm::Value * > ValsRef;

llvm::MDNode *makeUniqueMDNode( llvm::Module &m ) {
    llvm::Value **v = new llvm::Value *[ 1 ];
    v[0] = nullptr;
    auto md = llvm::MDNode::get( m.getContext(), ValsRef( v, 1 ) );
    md->replaceOperandWith( 0, md );
    return md;
}

void updateIDs( llvm::Module &m ) {
    for ( auto &f : m )
        for ( auto &b: f )
            for ( auto &i : b )
                if ( !i.getMetadata( "lart.id" ) )
                    i.setMetadata( "lart.id", makeUniqueMDNode( m ) );
}

}
