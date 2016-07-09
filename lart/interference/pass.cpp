#if LLVM_MAJOR >= 3 && LLVM_MINOR >= 7

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/interference/pass.h>
#include <lart/support/util.h>
#include <brick-assert>

using namespace lart::interference;

using MDsRef = llvm::ArrayRef< llvm::Metadata * >;

void Pass::propagate( llvm::Instruction *def, llvm::Instruction *use )
{
    if ( !_interference[ def ].insert( use ).second )
        return; /* already covered */
    if ( def == use )
        return;
    llvm::BasicBlock::iterator useit{ use };
    _interference[ use ].insert( def );
    auto bb = useit->getParent();
    if ( useit != bb->begin() )
        return propagate( def, --useit );
    else
        for ( auto &j : util::preds( bb ) )
            propagate( def, j.getTerminator() );
}

void Pass::annotate( llvm::Function &f )
{
    for ( auto &b: f )
        for ( auto &i : b )
            // alloca must live as long as any pointer to it might live, so if use is
            // such a pointer (or might contain pointer) we must propagate further
            for ( auto *u : pointerTransitiveUsers( i, TrackPointers::Alloca ) )
                propagate( &i, llvm::cast< llvm::Instruction >( u ) );

    for ( auto i : _interference ) {
        auto &vals = i.second;
        std::vector< llvm::Metadata * > v( vals.size() );
        std::transform( vals.begin(), vals.end(), v.begin(),
                        []( llvm::Instruction *i ) { return i->getMetadata( "lart.id" ); } );
        i.first->setMetadata( "lart.interference",
                              llvm::MDNode::get( f.getParent()->getContext(),
                                                 MDsRef( v ) ) );
    }
}

#endif
