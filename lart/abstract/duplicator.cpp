// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/duplicator.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract
{

void Duplicator::run( llvm::Module &m ) {
    auto abstract = query::query( meta::enumerate( m ) )
        .map( query::llvmdyncast< llvm::Instruction > )
        .filter( query::notnull )
        .filter( is_duplicable )
        .freeze();

    AbstractPlaceholderBuilder apb( m.getContext() );
    for ( const auto &inst : abstract )
        apb.construct( inst );
}

} // namespace lart::abstract
