// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/undef.h>

DIVINE_RELAX_WARNINGS
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>

namespace lart::abstract {

    auto is_abstract = [] ( auto i ) { return meta::abstract::has( i ); };

    void UndefLifting::run( llvm::Module & m )
    {
        _types = type_map::get( m );
        _lift = lift_function::get( "__unit_aggregate", m );

        auto inserts = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::InsertValueInst > )
            .filter( query::notnull )
            .filter( is_abstract )
            .freeze();

        for ( auto insert : inserts )
            process( insert );
    }

    void UndefLifting::process( llvm::InsertValueInst * insert )
    {
        auto agg = insert->getAggregateOperand();
        if ( !llvm::isa< llvm::UndefValue >( agg ) )
            return;

        auto irb = llvm::IRBuilder<>( insert );
        auto lifted_undef = _lift.call( irb );

        insert->setOperand( 0, lifted_undef );
    }

} // namespace lart::abstract