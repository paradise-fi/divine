// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/branching.h>

#include <lart/support/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract
{
    void LowerToBool::run( llvm::Module & m )
    {
        auto match = []( auto br )
        {
            return br->isConditional() &&
                   ( meta::abstract::has( br->getCondition() ) ||
                     meta::has( br->getCondition(), meta::tag::operation::phi ) );
        };

        auto brs = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::BranchInst > )
            .filter( query::notnull )
            .filter( match ).freeze();

        OperationBuilder builder;
        for ( auto * br : brs )
        {
            auto cond = llvm::cast< llvm::Instruction >( br->getCondition() );
            auto abs = cond->getNextNonDebugInstruction();
            auto op = builder.construct< Operation::Type::ToBool >( abs );
            br->setCondition( op.inst );
        }
    }
}

