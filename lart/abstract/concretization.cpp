// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/concretization.h>

#include <lart/abstract/placeholder.h>

namespace lart::abstract
{

    auto concretized_type( llvm::Argument * arg ) -> llvm::Type *
    {
        auto m = arg->getParent()->getParent();
        auto dom = Domain::get( arg );

        if ( dom == Domain::Concrete() )
            return arg->getType();

        if ( !is_base_type_in_domain( m, arg, dom ) )
            return arg->getType();

        return DomainMetadata::get( m, dom ).base_type();
    }

    auto concretized_types( llvm::Function * fn ) -> std::vector< llvm::Type * >
    {
        return query::query( fn->args() )
            .map( query::refToPtr )
            .map( concretized_type )
            .freeze();
    }

    void stash( llvm::CallInst * call, llvm::Value * mem )
    {
        llvm::IRBuilder<> irb( call );

        auto i64 = llvm::Type::getInt64Ty( call->getContext() );
        auto val = irb.CreatePtrToInt( mem, i64 );

        auto _stash = call->getModule()->getFunction( "__lart_stash" );
        ASSERT( _stash && "Missing __lart_stash function" );

        irb.CreateCall( _stash, { val } );
    }

    auto unstash( llvm::Function * fn ) -> llvm::Instruction *
    {
        llvm::IRBuilder<> irb( fn->getEntryBlock().getFirstNonPHI() );

        auto _unstash = fn->getParent()->getFunction( "__lart_unstash" );
        ASSERT( _unstash && "Missing __lart_unstash function" );

        return irb.CreateCall( _unstash );
    }

    struct Bundle
    {
        Bundle( llvm::CallInst * call )
            : _call( call )
        {
            _type = llvm::StructType::create( concretized_types( function() ) );
        }

        llvm::StructType * type() const noexcept { return _type; }

        llvm::Function * function() const noexcept
        {
            return get_some_called_function( _call );
        }

        llvm::Value * element( const llvm::Use & op ) const noexcept
        {
            auto idx = op.getOperandNo();
            if ( meta::has_dual( op ) )
                return meta::get_dual( op );

            auto fn = function();
            auto dom = Domain::get( std::next( fn->arg_begin(), idx ) );

            if ( dom == Domain::Concrete() )
                return op.get();

            if ( !is_base_type_in_domain( fn->getParent(), op.get(), dom ) )
                return op.get();

            return DomainMetadata::get( fn->getParent(), dom ).default_value();
        }

        llvm::Value * pack() const noexcept
        {
            llvm::IRBuilder<> irb( _call );
            auto ty = type();
            auto addr = irb.CreateAlloca( ty );

            llvm::Value * pack = llvm::UndefValue::get( ty );
            for ( const auto & op : _call->arg_operands() ) {
                pack = irb.CreateInsertValue( pack, element( op ), { op.getOperandNo() } );
            }

            irb.CreateStore( pack, addr );
            return addr;
        }

        void unpack( llvm::Function * fn )
        {
            auto addr = unstash( fn );

            llvm::IRBuilder<> irb( addr );
            auto ptr = irb.CreateIntToPtr( addr, _type->getPointerTo() );
            auto pack = irb.CreateLoad( _type, ptr );

            for ( auto & arg : fn->args() ) {
                if ( meta::has_dual( &arg ) ) {
                    auto dual = llvm::cast< llvm::Instruction >( meta::get_dual( &arg ) );
                    auto unstashed = irb.CreateExtractValue( pack, { arg.getArgNo() } );
                    dual->replaceAllUsesWith( unstashed );
                    dual->eraseFromParent();
                    meta::make_duals( &arg, llvm::cast< llvm::Instruction >( unstashed ) );
                }
            }

            addr->moveBefore( llvm::cast< llvm::Instruction >( ptr ) );
        }

        llvm::StructType * _type;
        llvm::CallInst * _call;
    };

    template< Placeholder::Type T, typename Builder >
    void concretize( llvm::Module & m, Builder & builder )
    {
        for ( const auto & ph : placeholders< T >( m ) )
            builder.concretize( ph );
    }

    void Concretization::run( llvm::Module & m )
    {
        CPlaceholderBuilder builder;

        auto filter = [] ( const auto & ph ) {
            return ph.type != Placeholder::Type::Assume &&
                   ph.type != Placeholder::Type::ToBool &&
                   ph.type != Placeholder::Type::Stash;
        };

        for ( const auto & ph : placeholders( m, filter ) )
            builder.concretize( ph );

        // process the rest of placeholders after their arguments were generated
        concretize< Placeholder::Type::Stash >( m, builder );
        concretize< Placeholder::Type::ToBool >( m, builder );
        concretize< Placeholder::Type::Assume >( m, builder );

        for ( const auto & ph : placeholders< Placeholder::Level::Abstract >( m ) ) {
            auto inst = ph.inst;
            inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
            inst->eraseFromParent();
        }

        std::set< llvm::Function * > seen;
        run_on_abstract_calls( [&] ( auto * call ) {
            if ( call->getNumArgOperands() != 0 ) {
                Bundle bundle( call );
                stash( call, bundle.pack() );

                run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                    if ( !seen.count( fn ) ) {
                        bundle.unpack( fn );
                        seen.insert( fn );
                    }
                } );
            }
        }, m );

    }

} // namespace lart::abstract
