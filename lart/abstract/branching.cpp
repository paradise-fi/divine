// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/branching.h>

#include <lart/support/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract {

    void LowerToBool::run( llvm::Module & m )
    {
        auto brs = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::BranchInst > )
            .filter( query::notnull )
            .filter( [] ( auto br ) {
                if ( br->isConditional() )
                    return meta::abstract::has( br->getCondition() );
                return false;
            } ).freeze();

        auto tag = meta::tag::operation::index;
        auto index = m.getFunction( "lart.abstract.to_tristate" )->getMetadata( tag );

        OperationBuilder builder;
        for ( auto * br : brs ) {
            auto abs = llvm::cast< llvm::Instruction >( meta::get_dual( br->getCondition() ) );
            auto cond = builder.construct< Operation::Type::ToBool >( abs );

            cond.inst->setMetadata( tag, index );

            br->setCondition( cond.inst );
        };
    }

} // namespace lart::abstract

