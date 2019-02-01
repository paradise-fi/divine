// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/support/util.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace divine {

template< typename Mapper >
void rewriteFileDebugPath( llvm::DIFile *file, Mapper &map, llvm::LLVMContext &ctx )
{
    std::string path = file->getFilename();
    std::string npath = map( path );
    if ( path != npath )
        for ( int i = 0, end = file->getNumOperands(); i < end; ++i )
            if ( file->getOperand( i ) == file->getRawFilename() )
                file->replaceOperandWith( i, llvm::MDString::get( ctx, npath ) );
}

template< typename Mapper >
void rewriteMDDebugPaths( llvm::MDNode *node, util::Set< llvm::MDNode * > &seen,
                           llvm::LLVMContext &ctx, Mapper &map )
{
    if ( !node || !seen.insert( node ).second )
        return;
    if ( auto *file = llvm::dyn_cast< llvm::DIFile >( node ) )
        return rewriteFileDebugPath( file, map, ctx );
    for ( auto &op : node->operands() )
        rewriteMDDebugPaths( llvm::dyn_cast_or_null< llvm::MDNode >( op.get() ), seen, ctx, map );
}

template< typename Mapper >
void rewriteDebugPaths( llvm::Module &m, Mapper map ) {
    auto &ctx = m.getContext();

    util::Set< llvm::MDNode * > seen;
    llvm::DebugInfoFinder dif;
    dif.processModule( m );

    for ( auto *cu : dif.compile_units() )
        rewriteMDDebugPaths( cu, seen, ctx, map );
    for ( auto *subr : dif.subprograms() )
        rewriteMDDebugPaths( subr, seen, ctx, map );
}

}
}

