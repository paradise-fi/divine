// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/concretization.h>

#include <lart/abstract/placeholder.h>

namespace lart::abstract
{
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

            unsigned int idx = 0;
            for ( auto & arg : fn->args() ) {
                if ( !meta::has_dual( &arg ) )
                    continue;
                auto dual = llvm::cast< llvm::Instruction >( meta::get_dual( &arg ) );
                auto ph = Placeholder( dual );
                ASSERT( ph.type == Placeholder::Type::Unstash );

                auto unstashed = irb.CreateExtractValue( pack, { idx++ } );
                meta::make_duals( &arg, llvm::cast< llvm::Instruction >( unstashed ) );
                ph.inst->replaceAllUsesWith( unstashed );
                ph.inst->eraseFromParent();
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

    void concretize_phi_placeholder( Placeholder ph )
    {
        ASSERT( ph.type == Placeholder::Type::PHI );
        auto inst = ph.inst;
        auto concrete = llvm::cast< llvm::PHINode >( meta::get_dual( inst ) );

        llvm::IRBuilder<> irb( inst );
        auto abstract = irb.CreatePHI( inst->getType(), concrete->getNumIncomingValues() );

        auto dom = Domain::get( inst );
        auto meta = DomainMetadata::get( inst->getModule(), dom );
        for ( unsigned int i = 0; i < concrete->getNumIncomingValues(); ++i ) {
            auto in = concrete->getIncomingValue( i );
            auto bb = concrete->getIncomingBlock( i );
            auto val = meta::has_dual( in ) ? meta::get_dual( in ) : meta.default_value();
            abstract->addIncoming( val, bb );
        }

        meta::abstract::inherit( abstract, inst );
        meta::make_duals( concrete, abstract );
        inst->replaceAllUsesWith( abstract );
        inst->eraseFromParent();
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
            if ( is_transformable( call ) )
                return; // call of domain function

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

        auto phis = [] ( const auto & ph ) { return  ph.type == Placeholder::Type::PHI; };
        for ( const auto & ph : placeholders( m, phis ) ) {
            concretize_phi_placeholder( ph );
        }
    }

} // namespace lart::abstract
