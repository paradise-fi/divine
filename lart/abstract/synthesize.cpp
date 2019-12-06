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

    llvm::ConstantInt * llvm_index( llvm::Function * fn )
    {
        using namespace lart::abstract;

        auto i = fn->getMetadata( meta::tag::operation::index );
        if ( !i )
            brq::raise() << "Missing domain index metadata.";
        auto c = llvm::cast< llvm::ConstantAsMetadata >( i->getOperand( 0 ) );
        return llvm::cast< llvm::ConstantInt >( c->getValue() );
    }

    template< typename Arg, typename IRB >
    llvm::Instruction * domain_index( const Arg& arg, IRB &irb )
    {
        return irb.CreateLoad( irb.getInt8Ty(), arg, "domain" );
    }

    auto inline_call = [] ( auto call ) {
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

        template<typename IRB, typename Index, typename Domain >
        auto get_function_from_domain( IRB &irb, Index op, Domain dom ) const
        {
            auto table = module->getNamedGlobal( vtable );
            auto domain = irb.CreateZExt( dom, i32Ty() );
            auto o = irb.CreateGEP( table, { i64( 0 ), domain, op } );
            return  irb.CreateLoad( o );
        }

        template< typename IRB >
        auto get_function_from_domain( IRB &irb ) const
        {
            auto index = [&] {
                if constexpr ( Taint::toBool( T ) )
                    return llvm_index( module->getFunction( "lart.abstract.to_tristate" ) );
                else if constexpr ( Taint::assume( T ) )
                    return llvm_index( module->getFunction( "lart.abstract.assume" ) );
                else
                    return llvm_index( lifter_function() );
            } ();
            return get_function_from_domain( irb, index, domain );
        }

        template< typename IRB >
        auto thaw( IRB & irb, llvm::Value * value ) const
        {
            auto thawFn = llvm::cast< llvm::Function >( module->getFunction( "__lart_thaw" ) );
            ASSERT( thawFn, "Missing implementation of 'thaw' function." );

            auto fty = thawFn->getFunctionType();
            auto addr = irb.CreateBitCast( value, fty->getParamType( 0 ) );

            return irb.CreateCall( thawFn, { addr } );
        }

        template< typename IRB >
        auto freeze( IRB & irb, llvm::Value * value, llvm::Value * addr ) const
        {
            auto freezeFn = llvm::cast< llvm::Function >( module->getFunction( "__lart_freeze" ) );
            ASSERT( freezeFn, "Missing implementation of 'freeze' function." );

            auto fty = freezeFn->getFunctionType();
            auto v = irb.CreateBitCast( value, fty->getParamType( 0 ) );
            auto a = irb.CreateBitCast( addr, fty->getParamType( 1 ) );

            return irb.CreateCall( freezeFn, { v, a } );
        }

        #define ENABLE_IF( name ) \
            typename std::enable_if_t< Taint::name( op ), void >

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

            std::map< llvm::BasicBlock *, llvm::Value * > dom;
            std::map< llvm::BasicBlock *, llvm::Value * > val1;
            std::map< llvm::BasicBlock *, llvm::Value * > val2;

            /* domain block */
            irb.SetInsertPoint( dbb );
            dom[ dbb ] = domain_index( lhs.abstract.value, irb );
            val1[ dbb ] = lhs.abstract.value;
            val2[ dbb ] = rhs.abstract.value;
            irb.CreateBr( exit );

            /* find out which argument to lift */
            irb.SetInsertPoint( cbb );
            irb.CreateCondBr( lhs.concrete.taint, l2bb, l1bb );

            /* lift 1. argument */
            irb.SetInsertPoint( l1bb );
            dom[ l1bb ] = domain_index( rhs.abstract.value, irb );

            if constexpr ( Taint::insertvalue( T ) )
            {
                auto ptr_concrete = irb.CreateAlloca( lhs.concrete.value->getType() );
                irb.CreateStore( lhs.concrete.value, ptr_concrete );
                val1[ l1bb ] = lift_aggr( ptr_concrete, dom[ l1bb ], irb );
            }
            else
                val1[ l1bb ] = lift( lhs.concrete.value, dom[ l1bb ], irb );

            val2[ l1bb ] = rhs.abstract.value;
            irb.CreateBr( exit );

            /* lift 2. argument */
            irb.SetInsertPoint( l2bb );
            dom[ l2bb ] = domain_index( lhs.abstract.value, irb );
            val1[ l2bb ] = lhs.abstract.value;
            val2[ l2bb ] = lift( rhs.concrete.value, dom[ l2bb ], irb );
            irb.CreateBr( exit );

            /* pick correct arguments */
            irb.SetInsertPoint( exit );

            auto merge = [&] ( auto args ) {
                auto arg = irb.CreatePHI( i8PTy(), 3 );
                auto phi = llvm::cast< llvm::PHINode >( arg );
                for ( const auto &[bb, d] : args )
                    phi->addIncoming( d, bb );
                return phi;
            };

            domain = irb.CreatePHI( i8Ty(), 3 );

            auto phi = llvm::cast< llvm::PHINode >( domain );
            for ( const auto &[bb, d] : dom )
                phi->addIncoming( d, bb );

            args.push_back( merge( val1 ) );
            args.push_back( merge( val2 ) );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( binary )
        {
            construct_binary_lifter( irb );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( insertvalue )
        {
            construct_binary_lifter( irb );

            ASSERT_EQ( inargs.size(), 5 );
            const auto &idx = inargs[ 4 ];
            args.push_back( idx.value );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( extractvalue )
        {
            auto agg = paired_arguments( 1 ).front();
            ASSERT_EQ( args.size(), 3 );
            const auto &idx = inargs[ 2 ];
            args.push_back( agg.concrete.value );
            args.push_back( idx.value );

            domain = domain_index( agg.abstract.value, irb );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( gep )
        {
            args.push_back( inargs[ 0 ].taint ); // ptr taint
            args.push_back( inargs[ 0 ].value ); // concrete ptr
            args.push_back( inargs[ 1 ].value ); // abstract ptr

            args.push_back( inargs[ 2 ].taint ); // offset taint
            args.push_back( irb.CreateSExt( inargs[ 2 ].value, i64Ty() ) ); // concrete offset
            args.push_back( inargs[ 3 ].value ); // abstract offset

            args.push_back( inargs[ 4 ].value ); // base bitwidth

            args.push_back( llvm_index( lifter_function() ) );

            auto lifter_template = module->getFunction( "__lart_gep_lifter" );

            auto call = irb.CreateCall( lifter_template, args );
            irb.CreateRet( call );
            inline_call( call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &irb ) -> ENABLE_IF( toBool )
        {
            auto arg = TaintArgument{ lifter_function()->arg_begin() };
            auto val = arg.abstract.value;
            auto lower = lifter_function()->getParent()->getFunction( "__lower_tristate" );

            domain = domain_index( val, irb );

            auto ptr = get_function_from_domain( irb );
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

            domain = domain_index( constrained.abstract.value, irb );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
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
            args.push_back( llvm_index( lifter_function() ) );

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

            domain = domain_index( addr, irb );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
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

            domain = domain_index( thawed, irb );

            auto call = call_lifter( irb );
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

            domain = domain_index( paired.abstract.value, irb );

            auto call = call_lifter( irb );
            return_from_lifter( irb, call );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &/* irb */ ) -> ENABLE_IF( call )
        {
            UNREACHABLE( "not implemented" );
        }

        template< typename builder_t, lifter_op_t op = T >
        auto construct( builder_t &/* irb */ ) -> ENABLE_IF( mem )
        {
            UNREACHABLE( "not implemented" );
        }

        template< typename builder_t >
        auto call_lifter( builder_t &irb )
        {
            auto ptr = get_function_from_domain( irb );
            auto rty = Taint::faultable( T ) ? i8PTy() : lifter_function()->getReturnType();
            auto fty = llvm::FunctionType::get( rty, types_of( args ), false );
            auto op = irb.CreateBitCast( ptr, fty->getPointerTo() );
            return irb.CreateCall( op, args );
        }

        template< typename builder_t, typename lifter_t, lifter_op_t op >
        auto return_from_lifter( builder_t &irb, lifter_t call ) -> ENABLE_IF( faultable )
        {
            auto crty = lifter_function()->getReturnType();
            auto t = module->getNamedGlobal( "__tainted" );
            auto load = irb.CreateLoad( t );

            auto casted = [&] {
                if ( crty->isIntegerTy() )
                    return irb.CreateTruncOrBitCast( load, crty );
                if ( crty->isFloatingPointTy() )
                    return irb.CreateUIToFP( load, crty );
                UNREACHABLE( "unsupported lifter return type" );
            } ();

            auto ret = irb.CreateRet( casted );
            stash( ret, call );
        }

        template< typename builder_t, typename lifter_t >
        auto return_from_lifter( builder_t &irb, lifter_t call )
        {
            irb.CreateRet( call );
        }

        // synthesization state
        std::vector< Argument > inargs;
        std::vector< llvm::Value * > args;
        llvm::BasicBlock * entry;
        llvm::Value * domain = nullptr;

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

        template< typename V, typename I, typename IRB >
        llvm::CallInst * lift_aggr( V val, I dom, IRB &irb ) const
        {
            auto fty = llvm::FunctionType::get( i8PTy(), { i8PTy(), i64Ty() }, false );
            auto fn = lift( dom, irb, fty, "aggr" );
            auto size = module->getDataLayout().getTypeAllocSize( val->getType() );
            auto i8ptr = irb.CreateBitCast( val, i8PTy() );
            return irb.CreateCall( fn, { i8ptr, i64( size ) } );
        }

        template< typename V, typename I, typename IRB >
        llvm::CallInst * lift( V val, I dom, IRB &irb ) const
        {
            auto fty = llvm::FunctionType::get( i8PTy(), { val->getType() }, false );
            auto fn = lift( dom, irb, fty, type_name( val ) );
            return irb.CreateCall( fn, { val } );
        }

        template< typename I, typename IRB >
        llvm::Value * lift( I dom, IRB &irb, llvm::FunctionType *fty, const std::string & tname ) const
        {
            auto name = "lart.abstract.lift_one_" + tname;
            auto impl = module->getFunction( name );
            if ( !impl )
                UNREACHABLE( "missing domain function", name );
            auto op = llvm_index( impl );
            auto ptr = get_function_from_domain( irb, op, dom );
            return irb.CreateBitCast( ptr, fty->getPointerTo() );
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
            // DISPATCH( Memcpy )
            // DISPATCH( Memmove )
            // DISPATCH( Memset )
            default:
                UNREACHABLE( "unsupported taint type" );
        }
    }

} // namespace lart::abstract
