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


    void Syntactic::run( llvm::Module &m ) {
        auto abstract = query::query( meta::enumerate( m ) )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .freeze();

        OperationBuilder builder;
        for ( auto * inst : abstract ) {
            // skip abstract constructors
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
                if ( call->getCalledFunction()->getMetadata( meta::tag::abstract ) )
                    continue;
            }

            // return instructions will be stashed in stashing pass
            if ( llvm::isa< llvm::ReturnInst >( inst ) )
                continue;

            // TODO fix call lifting
            if ( llvm::isa< llvm::CallInst >( inst ) )
                continue;

            if ( is_faultable( inst ) ) {
                // replace faultable operations by lifter handler
                auto op = builder.construct( inst );
                // annotate lifter as abstract return function to unstash its value
                meta::abstract::inherit( op.inst, inst );
                unstash( llvm::cast< llvm::CallInst >( op.inst ) );
            } else {
                builder.construct( inst );
            }
        }

        FreezePass().run( m );
    }

} // namespace lart::abstract
