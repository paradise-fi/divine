// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/branching.h>

#include <lart/support/util.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract {

void LowerToBool::run( llvm::Module & m ) {
    auto conditional = [] ( llvm::Value * val ) {
        return query::query( val->users() ).any( [] ( auto * u ) {
            return llvm::isa< llvm::BranchInst >( u );
        } );
    };

    APlaceholderBuilder builder{ m.getContext() };
    for ( const auto & ph : placeholders( m ) ) {
        auto op = ph.inst->getOperand( 0 );
        if ( conditional( op ) ) {
            auto cond = builder.construct< Placeholder::Type::ToBool >( ph.inst );

            for ( auto * u : op->users() )
                if ( auto br = llvm::dyn_cast< llvm::BranchInst >( u ) )
                    br->setCondition( cond.inst );
        }
    }
}

} // namespace lart::abstract

