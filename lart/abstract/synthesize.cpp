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
                    return llvm_index( function() );
            } ();
            return get_function_from_domain( irb, index, _domain );
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

        void construct()
        {
            auto entry =  basic_block( function(), "entry" );
            llvm::IRBuilder<> irb( entry );

            std::vector< llvm::Value * > vals = {};
            auto args = arguments();

            if constexpr ( Taint::binary( T ) )
            {
                auto fn = function();
                auto dbb = basic_block( fn,  "load.domain" );
                auto cbb = basic_block( fn,  "check.abstract" );
                auto l1bb = basic_block( fn, "arg.1.lift" );
                auto l2bb = basic_block( fn, "arg.2.lift" );
                auto exit = basic_block( fn, "exit" );

                /* entry */
                auto args = paired_arguments();
                const auto &lhs = args[ 0 ];
                const auto &rhs = args[ 1 ];

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

                _domain = irb.CreatePHI( i8Ty(), 3 );

                auto phi = llvm::cast< llvm::PHINode >( _domain );
                for ( const auto &[bb, d] : dom )
                    phi->addIncoming( d, bb );

                vals.push_back( merge( val1 ) );
                vals.push_back( merge( val2 ) );
            }

            if constexpr ( Taint::gep( T ) )
            {

                auto paired = paired_arguments();
                auto ptr = paired[ 0 ];
                auto off = paired[ 1 ];

                std::vector< llvm::Value * > args;

                args.push_back( ptr.concrete.taint );
                args.push_back( ptr.concrete.value );
                args.push_back( ptr.abstract.value );

                args.push_back( off.concrete.taint );
                args.push_back( irb.CreateSExt( off.concrete.value, i64Ty() ) );
                args.push_back( off.abstract.value );

                args.push_back( llvm_index( function() ) );

                auto fn = module->getFunction( "__lart_gep_lifter" );
                auto call = irb.CreateCall( fn, args );
                irb.CreateRet( call );

                inline_call( call );
                return;
            }

            if constexpr ( Taint::toBool( T ) )
            {
                auto arg = TaintArgument{ function()->arg_begin() };
                auto val = arg.abstract.value;
                auto lower = function()->getParent()->getFunction( "__lower_tristate" );

                _domain = domain_index( val, irb );

                auto ptr = get_function_from_domain( irb );
                auto trty = lower->getFunctionType()->getParamType( 0 );
                auto fty = llvm::FunctionType::get( trty, { val->getType() }, false );
                auto to_tristate = irb.CreateBitCast( ptr, fty->getPointerTo() );

                auto tristate = irb.CreateCall( to_tristate, { val } );
                auto _bool = irb.CreateCall( lower, { tristate } );
                irb.CreateRet( _bool );
                return;
            }

            if constexpr ( Taint::assume( T ) )
            {
                auto constrained = TaintArgument{ function()->arg_begin() };
                vals.push_back( constrained.abstract.value ); // constrained abstract value
                vals.push_back( args[ 3 ].value ); // constraint condition
                vals.push_back( args[ 4 ].value ); // assumed constraint value

                _domain = domain_index( constrained.abstract.value, irb );
            }

            if constexpr ( Taint::store( T ) )
            {
                auto paired = paired_arguments();
                auto value = paired[ 0 ];
                auto addr = paired[ 1 ];

                std::vector< llvm::Value * > args;

                args.push_back( value.concrete.taint );
                args.push_back( value.concrete.value );
                args.push_back( value.abstract.value );
                args.push_back( addr.concrete.taint );
                args.push_back( addr.concrete.value );
                args.push_back( addr.abstract.value );
                args.push_back( llvm_index( function() ) );


                auto type = args[ 1 ]->getType();
                std::string name = "__lart_store_lifter_" + llvm_name( type );
                auto fn = module->getFunction( name );

                auto call = irb.CreateCall( fn, args );
                irb.CreateRet( undef( function()->getReturnType() ) );

                inline_call( call );
                return;
            }

            if constexpr ( Taint::load( T ) )
            {
                auto addr = args[ 1 ].value;
                vals.push_back( addr ); // addr

                _domain = domain_index( addr, irb );
            }

            if constexpr ( Taint::freeze( T ) )
            {
                auto val = args[ 1 ].value;
                auto addr = args[ 2 ].value;
                freeze( irb, val, addr );
                irb.CreateRet( undef( function()->getReturnType() ) );
                return;
            }

            if constexpr ( Taint::thaw( T ) )
            {
                auto thawed = thaw( irb, args[ 1 ].value );
                vals.push_back( thawed ); // thawed address

                // TODO if scalar
                auto bw = args[ 0 ].value->getType()->getPrimitiveSizeInBits();
                vals.push_back( i8( bw ) ); // bitwidth of thawed value

                _domain = domain_index( thawed, irb );
            }

            if constexpr ( Taint::cast( T ) )
            {
                auto paired = paired_arguments().front();
                vals.push_back( paired.abstract.value );

                auto cast = llvm::cast< llvm::CastInst >( taint.inst->getPrevNode() );
                auto bw = cast->getDestTy()->getPrimitiveSizeInBits();
                vals.push_back( i8( bw ) );

                _domain = domain_index( paired.abstract.value, irb );
            }

            if constexpr ( Taint::call( T ) || Taint::mem( T ) )
            {
                /*auto argument = [&] ( const auto & arg, unsigned & idx ) {
                    auto abstract = [] ( unsigned i ) { return i + 1; };

                    llvm::Value * res = nullptr;
                    if ( meta::has_dual( arg ) ) {
                        res = args[ abstract( idx ) ].value;
                        idx += 2;
                    } else {
                        FIXME ASSERT( is_concrete( arg ) );
                        res = args[ idx ].value;
                        idx += 1;
                    }

                    return res;
                };

                auto call = llvm::cast< llvm::CallInst >( meta::get_dual( taint.inst ) );
                // TODO lifting

                unsigned bound = call->getNumArgOperands();
                if constexpr ( Taint::mem( T ) ) {
                    bound = 3; // do not consider volatilnes argument
                }

                unsigned idx = 0;
                for ( unsigned i = 0; i < bound; ++i ) {
                    vals.push_back( argument( call->getArgOperand( i ), idx ) );
                }*/
                UNREACHABLE( "not implemented" );
            }

            auto ptr = get_function_from_domain( irb );
            auto rty = Taint::faultable( T ) ? i8PTy() : function()->getReturnType();
            auto fty = llvm::FunctionType::get( rty, types_of( vals ), false );
            auto op = irb.CreateBitCast( ptr, fty->getPointerTo() );
            auto call = irb.CreateCall( op, vals );

            if constexpr ( Taint::freeze( T ) || Taint::store( T ) ) {
                irb.CreateRet( undef( function()->getReturnType() ) );
            } else if constexpr ( Taint::faultable( T ) ) {
                auto crty = function()->getReturnType();
                auto t = module->getNamedGlobal( "__tainted" );
                auto load = irb.CreateLoad( t );
                auto trunc = irb.CreateTruncOrBitCast( load, crty );
                auto ret = irb.CreateRet( trunc );
                stash( ret, call );
            } else {
                irb.CreateRet( call );
            }
        }

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

        std::vector< TaintArgument > paired_arguments() const
        {
            std::vector< TaintArgument > args;
            auto fn = function();

            ASSERT( fn->arg_size() % 4 == 0 );
            for ( auto it = fn->arg_begin(); it != fn->arg_end(); std::advance( it, 4 ) ) {
                args.push_back( TaintArgument{ it } );
            }

            return args;
        }

        llvm::Function * function() const { return taint.function(); }

        std::string type_name( llvm::Type * ty ) const noexcept
        {
            if ( ty->isPointerTy() )
                return "ptr";
            if ( ty->isIntegerTy() || ty->isFloatingPointTy() )
                return llvm_name( ty );
            UNREACHABLE( "unsupported type" );
        }

        std::string type_name( llvm::Value * val ) const noexcept
        {
            return type_name( val->getType() );
        }

        template< typename V, typename I, typename IRB >
        llvm::CallInst * lift( V val, I dom, IRB &irb ) const
        {
            auto fty = llvm::FunctionType::get( i8PTy(), { val->getType() }, false );
            auto name = "lart.abstract.lift_one_" + type_name( val );
            auto impl = module->getFunction( name );
            if ( !impl )
                UNREACHABLE( "missing domain function", name );
            auto op = llvm_index( impl );
            auto ptr = get_function_from_domain( irb, op, dom );
            auto fn = irb.CreateBitCast( ptr, fty->getPointerTo() );

            return irb.CreateCall( fn, { val } );
        }

        llvm::Value * _domain = nullptr;

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
                Lifter< Type::type >( taint ).construct(); break;

        switch ( taint.type )
        {
            DISPATCH( PHI )
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
            DISPATCH( Lift )
            DISPATCH( Lower )
            DISPATCH( Call )
            DISPATCH( Memcpy )
            DISPATCH( Memmove )
            DISPATCH( Memset )
            default:
                UNREACHABLE( "unsupported taint type" );
        }
    }

} // namespace lart::abstract
