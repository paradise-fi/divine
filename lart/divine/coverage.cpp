// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/divine/coverage.h>

namespace lart::divine
{
    void Coverage::run( llvm::Module &m )
    {
        _choose = m.getFunction( "__vm_choose" );
        for ( auto cs : _choose->users() )
            _chooses.emplace_back( cs );

        IndicesBuilder ib( &m );
        assign_indices( ib );
    }

    void Coverage::assign_indices( IndicesBuilder &indices )
    {
        int index = 0;
        for ( auto & ch : _chooses ) {
            auto call = ch.getInstruction();
            llvm::IRBuilder<> irb( call );

            ASSERT( llvm::isa< llvm::ConstantInt >( ch.getArgument( 0 ) ) );
            auto total = llvm::cast< llvm::ConstantInt >( ch.getArgument( 0 ) )
                ->getValue().getLimitedValue();

            ASSERT( ch.getNumArgOperands() == 1 );

            auto args = indices.create( total, index++ );
            auto indexed = irb.CreateCall( _choose, args );
            indexed->copyMetadata( *call );
            call->replaceAllUsesWith( indexed );
        }

        for ( auto & ch : _chooses )
            ch.getInstruction()->eraseFromParent();
    }

    PassMeta coverage() {
        return compositePassMeta< Coverage >( "coverage",
            "Instrument choose callsites with indices for coverage huristics." );
    }

} // namespace lart::divine
