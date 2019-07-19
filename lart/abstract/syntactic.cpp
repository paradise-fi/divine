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

    struct ThawPass {
        void run( llvm::Module &m )
        {
            thawFn = llvm::cast< llvm::Function >( m.getFunction( "__lart_thaw" ) );
            ASSERT( thawFn, "Missing implementation of 'thaw' function." );

            const auto tag = meta::tag::operation::thaw;
            for ( auto l : meta::enumerate< llvm::LoadInst >( m, tag ) )
                thaw( l );
        }

        void thaw( llvm::LoadInst *load ) const noexcept
        {
            llvm::IRBuilder<> irb( load->getNextNode() );

            auto fty = thawFn->getFunctionType();
            auto addr = irb.CreateBitCast( load->getPointerOperand(), fty->getParamType( 0 ) );
            irb.CreateCall( thawFn, { addr } );
        }

        llvm::Function * thawFn = nullptr;
    };


    struct FreezePass {
        void run( llvm::Module &m )
        {
            freezeFn = llvm::cast< llvm::Function >( m.getFunction( "__lart_freeze" ) );
            ASSERT( freezeFn, "Missing implementation of 'freeze' function." );

            _matched.init( m );

            const auto tag = meta::tag::operation::freeze;
            for ( auto s : meta::enumerate< llvm::StoreInst >( m, tag ) )
                freeze( s );
        }

        void freeze( llvm::StoreInst *store ) const noexcept
        {
            auto abs = _matched.abstract.at( store->getValueOperand() );

            auto fty = freezeFn->getFunctionType();

            llvm::IRBuilder<> irb( store->getNextNode() );
            auto addr = irb.CreateBitCast( store->getPointerOperand(), fty->getParamType( 1 ) );
            irb.CreateCall( freezeFn, { abs, addr } );
        }

        Matched _matched;

        llvm::Function * freezeFn = nullptr;
    };


    void Syntactic::run( llvm::Module &m ) {
        ThawPass().run( m );

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
