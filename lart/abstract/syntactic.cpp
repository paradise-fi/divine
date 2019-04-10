// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/syntactic.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract
{

    void union_on_branches( llvm::Module & m )
    {
        auto vals = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::CmpInst > )
            .filter( query::notnull )
            .filter( [&] ( auto * cmp ) {
                return query::any( cmp->operands(), [&] ( const auto & op ) {
                    auto dom = Domain::get( op.get() );
                    if ( is_concrete( dom ) || !op->getType()->isPointerTy() )
                        return false;
                    return DomainMetadata::get( &m, dom ).content();
                } );
            } )
            .map( [] ( auto cmp ) { return cmp->getOperand( 0 ); } )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .filter( [] ( auto val ) { return meta::has_dual( val ); } )
            .freeze();

        std::set< llvm::Instruction * > uni{ vals.begin(), vals.end() };

        APlaceholderBuilder builder;
        for ( auto val : uni ) {
            auto ph = builder.construct< Placeholder::Type::Union >( val );

            ph.inst->moveAfter( llvm::cast< llvm::Instruction >( meta::get_dual( val ) ) );

            auto icmps = query::query( val->users() )
                .map( query::llvmdyncast< llvm::CmpInst > )
                .filter( query::notnull )
                .freeze();

            for ( auto icmp : icmps ) {
                icmp->setOperand( 0, ph.inst );
            }
        }
    }

    void Syntactic::run( llvm::Module &m ) {
        auto abstract = query::query( meta::enumerate( m ) )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .filter( is_transformable )
            // skip alredy processed instructions
            .filter( [] ( auto * inst ) {
                return !meta::has_dual( inst );
            } )
            .freeze();

        APlaceholderBuilder builder;
        for ( const auto &inst : abstract ) {
            builder.construct( inst );
        }

        union_on_branches( m );
    }

} // namespace lart::abstract
