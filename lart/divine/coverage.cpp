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

        if ( !_choose )
            return;

        for ( auto cs : _choose->users() )
            _chooses.emplace_back( cs );

        assign_indices();
    }

    void Coverage::assign_indices()
    {
        auto i32Ty = llvm::Type::getInt32Ty( _choose->getContext() );

        int index = 0;
        for ( auto & ch : _chooses ) {
            auto call = ch.getInstruction();
            llvm::IRBuilder<> irb( call );

            ASSERT( ch.getNumArgOperands() == 1 );

            std::vector< llvm::Value * > args;
            args.push_back( ch.getArgument( 0 ) );
            args.push_back( llvm::ConstantInt::get( i32Ty, index ) );

            index += 65536; // 2^16 is upper bound on number of choices

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
