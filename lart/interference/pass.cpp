#include <lart/interference/pass.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/CFG.h>
#include <brick-assert.h>

char lart::interference::Pass::ID = 0;

using namespace lart::interference;

using llvm::cast;
using ValsRef = llvm::ArrayRef< llvm::Value * >;

void Pass::propagate( llvm::Instruction *def, llvm::Value *use )
{
    if ( _interference[ def ].count( use ) )
        return; /* already covered */
    _interference[ def ].insert( use );
    if ( def == use )
        return;
    llvm::BasicBlock::iterator I = cast< llvm::Instruction >( use );
    _interference[ &*I ].insert( def );
    auto BB = I->getParent();
    if ( I != BB->begin() )
        return propagate( def, --I );
    else
        for ( auto J = llvm::pred_begin( BB ); J != llvm::pred_end( BB ); ++J )
            propagate( def, (*J)->getTerminator() );
}

void Pass::annotate( llvm::Function *f )
{
    for ( auto &b: *f )
        for ( auto &i : b )
            for ( auto u = i.use_begin(); u != i.use_end(); ++u )
                propagate( &i, *u );

    for ( auto i : _interference ) {
        auto &vals = i.second;
        llvm::Value **v = new llvm::Value *[vals.size()];
        std::transform( vals.begin(), vals.end(), v,
                        []( llvm::Value *i ) { return cast< llvm::Instruction >( i )->getMetadata( "lart.id" ); } );
        i.first->setMetadata( "lart.interference",
                              llvm::MDNode::get( f->getParent()->getContext(),
                                                 ValsRef( v, vals.size() ) ) );
    }
}
