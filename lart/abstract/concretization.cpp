// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/concretization.h>

#include <lart/abstract/placeholder.h>

namespace lart::abstract
{

    auto unstash_placeholder( llvm::Argument * arg ) -> std::optional< Placeholder >
    {
        auto is_placeholder = [] ( llvm::Instruction * inst ) {
            return Placeholder::is( inst );
        };
        auto place = [] ( llvm::Instruction * inst ) {
            return Placeholder( inst );
        };
        auto is_unstash = [] ( const Placeholder & ph ) {
            return ph.type == Placeholder::Type::Unstash;
        };

        auto ph = query::query( arg->users() )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .filter( is_placeholder )
            .map( place )
            .filter( is_unstash )
            .freeze();

        if ( ph.empty() )
            return std::nullopt;

        ASSERT( ph.size() == 1 );
        return ph.front();
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
            :  _type( nullptr ), _call( call )
        {
            if ( auto tys = types(); !tys.empty() )
                _type = llvm::StructType::create( tys );
        }

        llvm::StructType * type() const noexcept { return _type; }

        auto types() const noexcept -> std::vector< llvm::Type * >
        {
            return types_of( arguments() );
        }

        auto arguments() const noexcept -> std::vector< llvm::Value * >
        {
            return query::query( _call->arg_operands() )
                .map( [&] ( const auto & op ) {
                    auto elem = element( op );
                    return elem == op.get() ? nullptr : elem;
                } )
                .filter( query::notnull )
                .freeze();
        }

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
            ASSERT ( !empty() );

            llvm::IRBuilder<> irb( _call );
            auto ty = type();
            auto addr = irb.CreateAlloca( ty );

            llvm::Value * pack = llvm::UndefValue::get( ty );

            unsigned int idx = 0;
            for ( const auto & arg : arguments() ) {
                pack = irb.CreateInsertValue( pack, arg, { idx++ } );
            }

            irb.CreateStore( pack, addr );
            return addr;
        }

        void unpack( llvm::Function * fn )
        {
            ASSERT ( !empty() );
            auto addr = unstash( fn );

            llvm::IRBuilder<> irb( addr );
            auto ptr = irb.CreateIntToPtr( addr, _type->getPointerTo() );
            auto pack = irb.CreateLoad( _type, ptr );

            auto unstashes = query::query( fn->args() )
                .map( query::refToPtr )
                .map( unstash_placeholder )
                .filter( [] ( const auto & ph ) { return ph.has_value(); } )
                .map( [] ( const auto & ph ) { return ph.value(); } )
                .freeze();

            unsigned int idx = 0;
            for ( auto & un : unstashes ) {
                auto arg = un.inst->getOperand( 0 );
                auto unstashed = irb.CreateExtractValue( pack, { idx++ } );
                meta::make_duals( arg, llvm::cast< llvm::Instruction >( unstashed ) );
                un.inst->replaceAllUsesWith( unstashed );
                un.inst->eraseFromParent();
            }

            addr->moveBefore( llvm::cast< llvm::Instruction >( ptr ) );
        }

        bool empty() const noexcept { return _type == nullptr; }

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
                if ( bundle.empty() )
                    return;
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
