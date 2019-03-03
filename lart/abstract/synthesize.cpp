// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/synthesize.h>

namespace
{
    llvm::BasicBlock * basic_block( llvm::Function * fn, std::string name )
    {
        auto & ctx = fn->getContext();
        return llvm::BasicBlock::Create( ctx, name, fn );
    }
}

namespace lart::abstract
{
    template< Taint::Type T >
    struct Lifter
    {
        Lifter( const Taint & taint )
            : taint( taint )
        {
            assert( taint.type == T );
        }

        struct Argument
        {
            explicit Argument( llvm::Argument * arg )
                : taint( arg ), value( std::next( arg ) )
            {}

            llvm::Value * taint;
            llvm::Value * value;
        };

        struct TaintAgumment
        {
            explicit TaintAgumment( llvm::Argument * arg )
                : concrete( arg ), abstract( std::next( arg, 2 ) )
            {}

            Argument concrete;
            Argument abstract;
        };

        struct LiftBlock
        {
            llvm::BasicBlock * entry;
            llvm::BasicBlock * exit;
            llvm::Value * lifted;
        };

        llvm::Function * get_operation( const std::string & name ) const
        {
            auto operation = module()->getFunction( name );
            if ( !operation )
                throw std::runtime_error( "missing function in domain: " + name );
            return operation;
        }

        llvm::Function * get_operation() const
        {
            std::string prefix = "__" + domain().name() + "_";
            if constexpr ( Taint::toBool( T ) ) {
                return get_operation( prefix + "bool_to_tristate" ); // TODO cleanup?
            }

            if constexpr ( Taint::call( T ) ) {
                auto dual = meta::get_dual( taint.inst );
                auto fn = llvm::cast< llvm::CallInst >( dual )->getCalledFunction();
                return get_operation( prefix + fn->getName().str() );
            }

            return get_operation( prefix + taint.name() );
        }

        void construct()
        {
            auto entry =  basic_block( function(), "entry" );
            llvm::IRBuilder<> irb( entry );

            auto meta = DomainMetadata::get( module(), domain() );
            std::vector< llvm::Value * > vals = {};
            auto operation = get_operation();

            auto args = arguments();

            if constexpr ( Taint::gep( T ) )
            {
                auto paired = TaintAgumment{ function()->arg_begin() };
                vals.push_back( paired.abstract.value ); // address
                auto idx = irb.CreateZExt( args[ 2 ].value, i64() );
                vals.push_back( idx ); // index
            }

            if constexpr ( Taint::toBool( T ) )
            {
                vals.push_back( irb.CreateCall( operation, args[ 0 ].value ) );
                operation = get_operation( "__tristate_lower" );
            }

            if constexpr ( Taint::assume( T ) )
            {
                auto constrained = TaintAgumment{ function()->arg_begin() };
                vals.push_back( constrained.abstract.value ); // constrained abstract value
                vals.push_back( args[ 2 ].value ); // constraint condition
                vals.push_back( args[ 3 ].value ); // assumed constraint value
            }

            if constexpr ( Taint::store( T ) )
            {
                vals.push_back( args[ 0 ].value ); // value
                vals.push_back( args[ 1 ].value ); // addr
            }

            if constexpr ( Taint::load( T ) )
            {
                vals.push_back( args[ 0 ].value ); // addr
                // TODO use abstract addr
            }

            if constexpr ( Taint::thaw( T ) )
            {
                auto addr = irb.CreateBitCast( args[ 1 ].value, i8ptr() );
                vals.push_back( addr ); // thawed address

                if ( meta.scalar() ) {
                    auto bw = args[ 0 ].value->getType()->getPrimitiveSizeInBits();
                    vals.push_back( i32cv( bw ) ); // bitwidth of thawed value
                }
            }

            if constexpr ( Taint::freeze( T ) )
            {
                vals.push_back( args[ 1 ].value ); // abstract value
                auto cst = irb.CreateBitCast( args[ 2 ].value, i8ptr() );
                vals.push_back( cst );             // concrete address
            }

            if constexpr ( Taint::stash( T ) )
            {
                auto paired = paired_arguments().front();
                vals.push_back( paired.abstract.value );
            }

            if constexpr ( Taint::cast( T ) )
            {
                auto paired = paired_arguments().front();
                vals.push_back( paired.abstract.value );

                auto dual = taint.dual();
                if ( !llvm::isa< llvm::PtrToIntInst >( dual ) ) {
                    auto bw = dual->getType()->getPrimitiveSizeInBits();
                    vals.push_back( i32cv( bw ) );
                }
                // TODO floats
            }

            if constexpr ( Taint::binary( T ) )
            {
                auto exit = basic_block( function(), "exit" );

                std::vector< LiftBlock > blocks;
                for ( const auto & arg : paired_arguments() ) {
                    if ( is_base_type_in_domain( module(), arg.concrete.value, domain() ) ) {
                        auto block = lifter( arg, blocks.size() );
                        if ( !blocks.empty() ) {
                            auto prev = blocks.back();
                            prev.exit->getTerminator()->setSuccessor( 0, block.entry );
                        } else {
                            entry->eraseFromParent();
                        }

                        exit->moveAfter( block.exit );
                        irb.SetInsertPoint( block.exit );
                        irb.CreateBr( exit );

                        vals.push_back( block.lifted );
                        blocks.push_back( std::move( block ) );
                    } else {
                        vals.push_back( arg.concrete.value );
                    }
                }

                irb.SetInsertPoint( exit );
            }

            if constexpr ( Taint::call( T ) )
            {
                auto call = llvm::cast< llvm::CallInst >( meta::get_dual( taint.inst ) );
                // TODO lifting

                unsigned idx = 0;
                auto abstract = [] ( unsigned i ) { return i + 1; };

                for ( const auto & arg : call->arg_operands() ) {
                    if ( meta::has_dual( arg.get() ) ) {
                        vals.push_back( args[ abstract( idx ) ].value );
                        idx += 2;
                    } else {
                        ASSERT( is_concrete( arg.get() ) );
                        vals.push_back( args[ idx ].value );
                        idx += 1;
                    }
                }
            }

            ASSERT( operation );
            ASSERT( operation->arg_size() == vals.size() );
            auto call = irb.CreateCall( operation, vals );

            if constexpr ( Taint::freeze( T ) || Taint::store( T ) || Taint::stash( T ) ) {
                irb.CreateRet( llvm::UndefValue::get( function()->getReturnType() ) );
            } else {
                irb.CreateRet( call );
            }
        }
    private:
        using IntegerType = llvm::IntegerType;
        using Constant = llvm::Constant;
        using ConstantInt = llvm::ConstantInt;

        llvm::Type * i8ptr() { return llvm::Type::getInt8PtrTy( ctx() ); }

        llvm::Type * i32() const { return IntegerType::get( ctx(), 32 ); }
        llvm::Type * i64() const { return IntegerType::get( ctx(), 64 ); }
        Constant * i32cv( uint32_t v ) const { return ConstantInt::get( i32(), v ); }
        Constant * i64cv( uint32_t v ) const { return ConstantInt::get( i64(), v ); }

        llvm::LLVMContext & ctx() const { return taint.inst->getContext(); }

        Domain domain() const { return Domain::get( taint.inst ); }

        auto arguments() const
        {
            std::vector< Argument > args;

            auto begin = function()->arg_begin();
            auto end = function()->arg_end();

            for ( auto it = begin; it != end; std::advance( it, 2 ) ) {
                args.push_back( Argument{ it } );
            }

            return args;
        }

        std::vector< TaintAgumment > paired_arguments() const
        {
            std::vector< TaintAgumment > args;
            auto fn = function();

            ASSERT( fn->arg_size() % 4 == 0 );
            for ( auto it = fn->arg_begin(); it != fn->arg_end(); std::advance( it, 4 ) ) {
                args.push_back( TaintAgumment{ it } );
            }

            return args;
        }

        llvm::Module * module() const { return taint.inst->getModule(); }
        llvm::Function * function() const { return taint.function(); }

        llvm::Constant * bitwidth( llvm::Value * val ) const
        {
            return i32cv( val->getType()->getPrimitiveSizeInBits() );
        }

        llvm::CallInst * lift( llvm::Value * val ) const
        {
            llvm::IRBuilder<> irb( ctx() );

            auto argc = i32cv( 1 );
            auto m = module();

            auto meta = DomainMetadata::get( module(), domain() );

            if ( meta.scalar() )
            {
                if ( val->getType()->isIntegerTy() ) {
                    // TODO rename lift to lift_int
                    auto name = "__" + domain().name() + "_lift";
                    auto fn = m->getFunction( name );
                    auto val64bit = irb.CreateSExt( val, i64() );
                    return irb.CreateCall( fn, { bitwidth( val ), argc, val64bit } );
                }

                if ( val->getType()->isFloatingPointTy() ) {
                    auto name = "__" + domain().name() + "_lift_float";
                    auto fn = m->getFunction( name );
                    return irb.CreateCall( fn, { bitwidth( val ), argc, val } );
                }
            }

            if ( meta.content() )
            {
                auto fault = m->getFunction( "__" + domain().name() + "_undef_value" );
                return irb.CreateCall( fault );
            }

            UNREACHABLE( "Unknown type to be lifted.\n" );
        }

        LiftBlock lifter( TaintAgumment arg, size_t idx ) const
        {
            auto fn = function();

            std::string prefix = "arg." + std::to_string( idx );

            auto ebb = basic_block( fn, prefix + ".entry" );
            auto lbb = basic_block( fn, prefix + ".lifter" );

            llvm::IRBuilder<> irb( lbb );
            auto lifted = lift( arg.concrete.value );

            auto ops = query::query( lifted->operands() )
                .map( query::llvmdyncast< llvm::Instruction > )
                .filter( query::notnull )
                .filter( [] ( auto inst ) { return !inst->getParent(); } )
                .freeze();

            for ( const auto & op : ops )
                irb.Insert( op );
            irb.Insert( lifted );

            auto mbb = basic_block( fn, prefix + ".merge" );
            irb.SetInsertPoint( mbb );

            auto type = arg.abstract.value->getType();
            auto phi = irb.CreatePHI( type, 2 );
            phi->addIncoming( arg.abstract.value, ebb );
            phi->addIncoming( lifted, lbb );

            irb.SetInsertPoint( ebb );
            irb.CreateCondBr( arg.concrete.taint, mbb, lbb );
            irb.SetInsertPoint( lbb );
            irb.CreateBr( mbb );
            return { ebb, mbb, phi };
        }

        Taint taint;
    };


    void Synthesize::run( llvm::Module & m )
    {
        for ( const auto & taint : taints( m ) )
            dispach( taint );
    }

    void Synthesize::dispach( const Taint & taint )
    {
        if ( !taint.function()->empty() )
            return;

        using Type = Taint::Type;

        switch ( taint.type )
        {
            case Type::PHI:
                Lifter< Type::PHI >( taint ).construct(); break;
            case Type::GEP:
                Lifter< Type::GEP >( taint ).construct(); break;
            case Type::Thaw:
                Lifter< Type::Thaw >( taint ).construct(); break;
            case Type::Freeze:
                Lifter< Type::Freeze >( taint ).construct(); break;
            case Type::Stash:
                Lifter< Type::Stash >( taint ).construct(); break;
            case Type::Unstash:
                Lifter< Type::Unstash >( taint ).construct(); break;
            case Type::ToBool:
                Lifter< Type::ToBool >( taint ).construct(); break;
            case Type::Assume:
                Lifter< Type::Assume >( taint ).construct(); break;
            case Type::Store:
                Lifter< Type::Store >( taint ).construct(); break;
            case Type::Load:
                Lifter< Type::Load >( taint ).construct(); break;
            case Type::Cmp:
                Lifter< Type::Cmp >( taint ).construct(); break;
            case Type::Cast:
                Lifter< Type::Cast >( taint ).construct(); break;
            case Type::Binary:
                Lifter< Type::Binary >( taint ).construct(); break;
            case Type::Lift:
                Lifter< Type::Lift >( taint ).construct(); break;
            case Type::Lower:
                Lifter< Type::Lower >( taint ).construct(); break;
            case Type::Call:
                Lifter< Type::Call >( taint ).construct(); break;
        }
    }

} // namespace lart::abstract
