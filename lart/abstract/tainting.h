// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/operation.h>

namespace lart::abstract {

    struct Taint : Operation
    {
        inline static std::string prefix = "__vm_test_taint";

        using Operation::Operation;

        inline llvm::Function * function() const
        {
            ASSERT( taint );
            return llvm::cast< llvm::Function >( this->inst->getOperand( 0 ) );
        }
    };

    struct Tainting {
        void run( llvm::Module & m );
        Taint dispatch( const Operation & ph );

        Matched matched;
    };


    static auto taints( llvm::Module & m )
    {
        return query::query( Operation::enumerate( m, true /* taint */ ) )
            .map( [] ( auto op ) { return Taint{ op.inst, op.type, true }; } )
            .freeze();
    }

    template< typename Filter >
    auto taints( llvm::Module & m, Filter filter )
    {
        return query::query( taints( m ) ).filter( filter ).freeze();
    }

    template< Operation::Type T >
    auto taints( llvm::Module & m )
    {
        return taints( m, [] ( const auto & taint ) { return taint.type == T; } );
    }

} // namespace lart::abstract
