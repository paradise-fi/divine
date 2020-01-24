// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/synthesize.h>

#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Transforms/Utils/Cloning.h>
DIVINE_UNRELAX_WARNINGS

namespace
{
    llvm::BasicBlock * basic_block( llvm::Function * fn, std::string name )
    {
        auto & ctx = fn->getContext();
        return llvm::BasicBlock::Create( ctx, name, fn );
    }

    auto inline_call = [] ( auto call )
    {
        llvm::InlineFunctionInfo ifi;
        llvm::InlineFunction( call, ifi );
    };
} // anonymous namespace

namespace lart::abstract
{
    template< Operation::Type T >
    struct Lifter : LLVMUtil< Lifter< T > >
    {
        using lifter_op_t = Operation::Type;

        using Self = Lifter< T >;
        using Util = LLVMUtil< Self >;

        using Util::i1Ty;
        using Util::i8Ty;
        using Util::i16Ty;
        using Util::i32Ty;
        using Util::i64Ty;

        using Util::i8PTy;

        using Util::i1;
        using Util::i8;
        using Util::i32;
        using Util::i64;

        using Util::ctx;

        using Util::undef;

        static constexpr const char * vtable = "__lart_domains_vtable";

        Lifter( const Taint & taint )
            : taint( taint ), module( taint.inst->getModule() )
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

        struct TaintArgument
        {
            explicit TaintArgument( llvm::Argument * arg )
                : concrete( arg ), abstract( std::next( arg, 2 ) )
            {}

            Argument concrete;
            Argument abstract;
        };

        auto get_lamp_op() const
        {
            if constexpr ( Taint::toBool( T ) )
                return module->getFunction( "__lamp_to_tristate" );
            if constexpr ( Taint::assume( T ) )
                return module->getFunction( "__lamp_assume" );

            auto meta = taint.inst->getMetadata( meta::tag::operation::impl );
            ASSERT( meta, *taint.inst );
            return llvm::mdconst::extract< llvm::Function >( meta->getOperand( 0 ) );
        }

        template< typename IRB >
        auto thaw( IRB & irb, llvm::Value * value ) const
        {
            auto thawFn = llvm::cast< llvm::Function >( module->getFunction( "__lamp_melt" ) );
            ASSERT( thawFn, "Missing implementation of 'thaw' function." );

            auto fty = thawFn->getFunctionType();
            auto addr = irb.CreateBitCast( value, fty->getParamType( 0 ) );

            return irb.CreateCall( thawFn, { addr } );
        }

        template< typename IRB >
        auto freeze( IRB & irb, llvm::Value * value, llvm::Value * addr ) const
        {
            auto freezeFn = llvm::cast< llvm::Function >( module->getFunction( "__lamp_freeze" ) );
            ASSERT( freezeFn, "Missing implementation of 'freeze' function." );

            auto fty = freezeFn->getFunctionType();
            auto v = irb.CreateBitCast( value, fty->getParamType( 0 ) );
            auto a = irb.CreateBitCast( addr, fty->getParamType( 1 ) );

            return irb.CreateCall( freezeFn, { v, a } );
        }

        #define ENABLE_IF( name ) \
            typename std::enable_if_t< Taint::name( op ), void >

        #define ENABLE_IF_NOT( name ) \
            typename std::enable_if_t< !Taint::name( op ), void >

        void synthesize()
        {
            entry = basic_block( lifter_function(), "entry" );
            inargs = arguments();

            auto irb = llvm::IRBuilder<>( entry );
            construct( irb );
        }

        template< typename builder_t >
        auto construct_binary_lifter( builder_t &irb )
        {
            auto fn = lifter_function();
            auto dbb = basic_block( fn,  "load.domain" );
            auto cbb = basic_block( fn,  "check.abstract" );
            auto l1bb = basic_block( fn, "arg.1.lift" );
            auto l2bb = basic_block( fn, "arg.2.lift" );
            auto exit = basic_block( fn, "exit" );

            /* entry */
            auto paired = paired_arguments( 2 );
            const auto &lhs = paired[ 0 ];
            const auto &rhs = paired[ 1 ];

            auto abs = irb.CreateAnd( lhs.concrete.taint, rhs.concrete.taint );
            irb.CreateCondBr( abs, dbb, cbb );

            std::map< llvm::BasicBlock *, llvm::Value * > val1;
            std::map< llvm::BasicBlock *, llvm::Value * > val2;

            /* domain block */
            irb.SetInsertPoint( dbb );
            val1[ dbb ] = lhs.abstract.value;
            val2[ dbb ] = rhs.abstract.value;
            irb.CreateBr( exit );

            /* find out which argument to lift */
            irb.SetInsertPoint( cbb );
            irb.CreateCondBr( lhs.concrete.taint, l2bb, l1bb );

            /* lift 1. argument */
            irb.SetInsertPoint( l1bb );

            if constexpr ( Taint::insertvalue( T ) )
            {
                auto ptr_concrete = irb.CreateAlloca( lhs.concrete.value->getType() );
                irb.CreateStore( lhs.concrete.value, ptr_concrete );
                val1[ l1bb ] = lift_aggr( ptr_concrete, irb );
            }
            else
                val1[ l1bb ] = lift( lhs.concrete.value, irb );

            val2[ l1bb ] = rhs.abstract.value;
            irb.CreateBr( exit );

            /* lift 2. argument */
            irb.SetInsertPoint( l2bb );
            val1[ l2bb ] = lhs.abstract.value;
            val2[ l2bb ] = lift( rhs.concrete.value, irb );
            irb.CreateBr( exit );

            /* pick correct arguments */
            irb.SetInsertPoint( exit );

            auto merge = [&] ( auto args )
            {
                auto arg = irb.CreatePHI( i8PTy(), 3 );
                auto phi = llvm::cast< llvm::PHINode >( arg );
                for ( const auto &[bb, d] : args )
                    phi->addIncoming( d, bb );
                return phi;
            };

            args.push_back( merge( val1 ) );
            args.push_back( merge( val2 ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( binary )
        {
            construct_binary_lifter( irb );
            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( insertvalue )
        {
            construct_binary_lifter( irb );

            ASSERT_EQ( inargs.size(), 5 );
            const auto &idx = inargs[ 4 ];
            args.push_back( idx.value );

            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( extractvalue )
        {
            auto agg = paired_arguments( 1 ).front();
            ASSERT_EQ( inargs.size(), 3 );
            const auto &idx = inargs[ 2 ];
            args.push_back( agg.abstract.value );
            args.push_back( idx.value );

            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( gep )
        {
            construct_binary_lifter( irb );
            args.push_back( inargs[ 4 ].value );
            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( toBool )
        {
            auto arg = TaintArgument{ lifter_function()->arg_begin() };
            auto val = arg.abstract.value;
            auto lower = lifter_function()->getParent()->getFunction( "__lamp_decide" );

            auto ptr = get_lamp_op();
            auto trty = lower->getFunctionType()->getParamType( 0 );
            auto fty = llvm::FunctionType::get( trty, { val->getType() }, false );
            auto to_tristate = irb.CreateBitCast( ptr, fty->getPointerTo() );

            auto tristate = irb.CreateCall( to_tristate, { val } );
            auto _bool = irb.CreateCall( lower, { tristate } );
            irb.CreateRet( _bool );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( assume )
        {
            auto constrained = TaintArgument{ lifter_function()->arg_begin() };
            args.push_back( constrained.abstract.value ); // constrained abstract value
            args.push_back( inargs[ 3 ].value ); // constraint condition
            args.push_back( inargs[ 4 ].value ); // assumed constraint value

            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( store )
        {
            auto paired = paired_arguments();
            auto value = paired[ 0 ];
            auto addr = paired[ 1 ];

            args.push_back( value.concrete.taint );
            args.push_back( value.concrete.value );
            args.push_back( value.abstract.value );
            args.push_back( addr.concrete.taint );
            args.push_back( addr.concrete.value );
            args.push_back( addr.abstract.value );

            auto type = args[ 1 ]->getType();
            std::string name = "__lart_store_lifter_" + llvm_name( type );
            auto fn = module->getFunction( name );

            auto call = irb.CreateCall( fn, args );
            irb.CreateRet( undef( lifter_function()->getReturnType() ) );
            inline_call( call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( load )
        {
            auto addr = inargs[ 1 ].value;
            args.push_back( addr ); // addr

            auto rty = lifter_function()->getReturnType();
            std::string name = "__lart_load_lifter_" + llvm_name( rty );
            auto lifter_template = module->getFunction( name );

            auto call = irb.CreateCall( lifter_template, args );
            irb.CreateRet( call );
            inline_call( call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( freeze )
        {
            auto val = inargs[ 1 ].value;
            auto addr = inargs[ 2 ].value;
            freeze( irb, val, addr );
            irb.CreateRet( undef( lifter_function()->getReturnType() ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( thaw )
        {
            auto thawed = thaw( irb, inargs[ 1 ].value );
            args.push_back( thawed ); // thawed address

            // TODO if scalar
            auto bw = inargs[ 0 ].value->getType()->getPrimitiveSizeInBits();
            args.push_back( i8( bw ) ); // bitwidth of thawed value

            auto call = call_lamp_op( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( cast )
        {
            auto paired = paired_arguments().front();
            args.push_back( paired.abstract.value );

            auto cast = llvm::cast< llvm::CastInst >( taint.inst->getPrevNode() );
            auto bw = cast->getDestTy()->getPrimitiveSizeInBits();
            args.push_back( i8( bw ) );

            return_from_lifter( irb, call_lamp_op( irb ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( call )
        {
            auto paired = paired_arguments();

            std::map< llvm::BasicBlock *, llvm::Value * > lifted;

            auto merge = [&] ( auto cbb, auto lbb )
            {
                auto arg = irb.CreatePHI( i8PTy(), 2 );
                auto phi = llvm::cast< llvm::PHINode >( arg );
                phi->addIncoming( lifted[ cbb ], cbb ); // concrete path
                phi->addIncoming( lifted[ lbb ], lbb ); // lifted (abstract) path
                return phi;
            };

            auto lifter = lifter_function();

            auto lift_arg = [&] ( auto arg )
                -> std::tuple< llvm::BasicBlock *, llvm::BasicBlock * > // (start, end)
            {
                auto fork = basic_block( lifter, "fork" );
                auto concrete_path = basic_block( lifter, "concrete" );
                auto join = basic_block( lifter, "join" );

                irb.SetInsertPoint( fork );
                irb.CreateCondBr( arg.concrete.taint, join, concrete_path );

                lifted[ fork ] = arg.abstract.value;

                irb.SetInsertPoint( concrete_path );
                lifted[ concrete_path ] = lift_constant( arg.concrete.value, irb );
                irb.CreateBr( join );

                irb.SetInsertPoint( join );
                args.push_back( merge( fork, concrete_path ) );
                return { fork, join };
            };

            if ( paired.size() > 1 )
            {
                auto last = &lifter->getEntryBlock();
                auto exit = basic_block( lifter, "exit" );

                for ( const auto &arg : paired )
                {
                    auto [brbb, mbb] = lift_arg( arg );
                    irb.SetInsertPoint( last );
                    irb.CreateBr( brbb );
                    last = mbb;
                }

                exit->moveAfter( last );
                irb.SetInsertPoint( last );
                irb.CreateBr( exit );
                irb.SetInsertPoint( exit );
            }
            else
                args.push_back( paired.front().abstract.value );

            auto call = call_lamp_op( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t >
        auto call_lamp_op( builder_t &irb )
        {
            auto ptr = get_lamp_op();
            auto rty = Taint::faultable( T ) ? i8PTy() : lifter_function()->getReturnType();
            auto fty = llvm::FunctionType::get( rty, types_of( args ), false );
            auto op = irb.CreateBitCast( ptr, fty->getPointerTo() );
            return irb.CreateCall( op, args );
        }

        template< typename builder_t >
        llvm::Value * tainted_value( llvm::Type * type, builder_t &irb )
        {
            auto tainted = irb.CreateLoad( module->getNamedGlobal( "__tainted" ) );

            if ( type->isIntegerTy() )
                return irb.CreateZExtOrTrunc( tainted, type );
            if ( type->isFloatingPointTy() )
                return irb.CreateUIToFP( tainted, type );
            if ( type->isPointerTy() )
                return irb.CreateIntToPtr( tainted, type );

            UNREACHABLE( "unsupported taint type" );
        }

        template< typename builder_t, typename lifter_t, lifter_op_t op = T >
        auto return_from_lifter( builder_t &irb, lifter_t call ) -> ENABLE_IF( faultable )
        {
            auto crty = lifter_function()->getReturnType();
            auto tainted = tainted_value( crty, irb );
            auto ret = irb.CreateRet( tainted );
            stash( ret, call );
        }

        template< typename builder_t, typename lifter_t, lifter_op_t op = T >
        auto return_from_lifter( builder_t &irb, lifter_t call ) -> ENABLE_IF_NOT( faultable )
        {
            irb.CreateRet( call );
        }

        // synthesization state
        std::vector< Argument > inargs;
        std::vector< llvm::Value * > args;
        llvm::BasicBlock * entry;

        auto arguments() const
        {
            std::vector< Argument > pargs;

            auto begin = lifter_function()->arg_begin();
            auto end = lifter_function()->arg_end();

            for ( auto it = begin; it != end; std::advance( it, 2 ) )
               pargs.push_back( Argument{ it } );

            return pargs;
        }

        std::vector< TaintArgument > paired_arguments( unsigned pairs = 0 ) const
        {
            std::vector< TaintArgument > args;
            auto fn = lifter_function();

            if ( pairs )
                ASSERT( fn->arg_size() >= 4 * pairs );
            else
                ASSERT( fn->arg_size() % 4 == 0 );

            auto begin = fn->arg_begin();
            auto end = pairs ? std::next( begin, pairs * 4 ) : fn->arg_end();
            for ( auto it = begin; it != end; std::advance( it, 4 ) ) {
                args.push_back( TaintArgument{ it } );
            }

            return args;
        }

        llvm::Function * lifter_function() const { return taint.function(); }

        std::string type_name( llvm::Type * ty ) const noexcept
        {
            if ( ty->isPointerTy() )
                return "ptr";
            if ( ty->isIntegerTy() || ty->isFloatingPointTy() )
                return llvm_name( ty );
            if ( ty->isStructTy() )
                return "aggr";
            UNREACHABLE( "unsupported type" );
        }

        std::string type_name( llvm::Value * val ) const noexcept
        {
            return type_name( val->getType() );
        }

        template< typename V, typename IRB >
        llvm::CallInst * lift_aggr( V val, IRB &irb ) const
        {
            auto fty = llvm::FunctionType::get( i8PTy(), { i8PTy(), i64Ty() }, false );
            auto fn = lift( irb, fty, "aggr" );
            auto size = module->getDataLayout().getTypeAllocSize( val->getType() );
            auto i8ptr = irb.CreateBitCast( val, i8PTy() );
            return irb.CreateCall( fn, { i8ptr, i64( size ) } );
        }

        template< typename V, typename IRB >
        llvm::CallInst * lift( V val, IRB &irb ) const
        {
            auto fty = llvm::FunctionType::get( i8PTy(), { val->getType() }, false );
            auto fn = lift( irb, fty, type_name( val ) );
            return irb.CreateCall( fn, { val } );
        }

        template< typename IRB >
        llvm::Value * lift( IRB &irb, llvm::FunctionType *fty, const std::string & tname ) const
        {
            auto name = "__lamp_wrap_" + tname;
            auto impl = module->getFunction( name );
            if ( !impl )
                brq::raise() << "Missing domain function " << name;
            return irb.CreateBitCast( impl, fty->getPointerTo() );
        }

        template< typename IRB >
        llvm::CallInst * lift_constant( llvm::Value * con, IRB &irb ) const
        {
            auto name = "__lart_lift_constant_" + type_name( con );
            auto const_lift = module->getFunction( name );
            ASSERT( const_lift );

            return irb.CreateCall( const_lift, { con } );
        }

        Taint taint;
        llvm::Module * module;
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

        #define DISPATCH( type ) \
            case Type::type: \
                Lifter< Type::type >( taint ).synthesize(); break;

        switch ( taint.type )
        {
            // DISPATCH( PHI )
            DISPATCH( GEP )
            DISPATCH( ToBool )
            DISPATCH( Assume )
            DISPATCH( Store )
            DISPATCH( Load )
            DISPATCH( Freeze )
            DISPATCH( Thaw )
            DISPATCH( Cmp )
            DISPATCH( Cast )
            DISPATCH( Binary )
            DISPATCH( BinaryFaultable )
            // DISPATCH( Lift )
            // DISPATCH( Lower )
            DISPATCH( Call )
            DISPATCH( ExtractValue )
            DISPATCH( InsertValue )
            default:
                UNREACHABLE( "unsupported taint type" );
        }
    }

} // namespace lart::abstract
