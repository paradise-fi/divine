// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/syntactic.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/stash.h>

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract
{

    struct FreezePass
    {
        void run( llvm::Module &m )
        {
            OperationBuilder builder;

            const auto tag = meta::tag::operation::freeze;
            constexpr auto Freeze = Operation::Type::Freeze;
            for ( auto store : meta::enumerate< llvm::StoreInst >( m, tag ) )
                builder.construct< Freeze >( store );
        }
    };


    struct PHINodePass
    {
        auto phis( llvm::Module &m )
        {
            const auto tag = meta::tag::operation::phi;
            return meta::enumerate< llvm::PHINode >( m, tag );
        }

        void init( llvm::Module &m )
        {
            auto i8 = llvm::Type::getInt8PtrTy( m.getContext() );

            for ( auto phi : phis( m ) ) {
                llvm::IRBuilder<> irb( phi );
                auto a = irb.CreatePHI( i8, phi->getNumIncomingValues() );
                a->moveAfter( phi );
            }
        }

        void fill( llvm::Module &m )
        {
            Matched matched;
            matched.init( m );

            auto i8 = llvm::Type::getInt8PtrTy( m.getContext() );
            auto null = llvm::ConstantPointerNull::get( i8 );

            for ( auto phi : phis( m ) ) {
                auto aphi = llvm::cast< llvm::PHINode >( matched.abstract[ phi ] );

                for ( unsigned i = 0; i < phi->getNumIncomingValues(); ++i ) {
                    auto ci = phi->getIncomingValue( i );
                    auto ai = matched.abstract.count( ci )
                            ? matched.abstract[ ci ]
                            : null;

                    aphi->addIncoming( ai, phi->getIncomingBlock( i ) );
                }
            }
        }
    };

    void Syntactic::run( llvm::Module &m )
    {
        PHINodePass pnp;

        pnp.init( m );

        auto abstract = query::query( meta::enumerate( m ) )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .freeze();

        OperationBuilder builder;

        for ( auto * inst : abstract )
        {
            // skip abstract constructors
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) )
                if ( call->getCalledFunction() &&
                     call->getCalledFunction()->getMetadata( meta::tag::abstract ) )
                    continue;

            // return instructions will be stashed in stashing pass
            if ( llvm::isa< llvm::ReturnInst >( inst ) )
                continue;

            if ( is_faultable( inst ) )
            {
                // replace faultable operations by lifter handler
                auto op = builder.construct( inst );
                // annotate lifter as abstract return function to unstash its value
                meta::abstract::inherit( op.inst, inst );
                unstash( llvm::cast< llvm::CallInst >( op.inst ) );
            } else {
                if ( llvm::isa< llvm::CallInst >( inst ) )
                    continue;
                builder.construct( inst );
            }
        }

        pnp.fill( m );

        FreezePass().run( m );
    }

} // namespace lart::abstract
