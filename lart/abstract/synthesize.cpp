// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/synthesize.h>

#include <lart/abstract/stash.h>

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
            ERROR( "Missing domain index metadata." )
        auto c = llvm::cast< llvm::ConstantAsMetadata >( i->getOperand( 0 ) );
        return llvm::cast< llvm::ConstantInt >( c->getValue() );
    }

    template< typename Arg, typename IRB >
    llvm::Instruction * domain_index( const Arg& arg, IRB &irb )
    {
        return irb.CreateLoad( irb.getInt8Ty(), arg, "domain" );
    }
} // anonymous namespace

namespace lart::abstract
{
    template< Operation::Type T >
    struct Lifter
    {
        static constexpr const char * vtable = "__lart_domains_vtable";

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
            auto table = module()->getNamedGlobal( vtable );
            auto z = llvm::ConstantInt::get( i64(), 0 ); // dummy zero
            auto domain = irb.CreateZExt( dom, i32() );
            auto o = irb.CreateGEP( table, { z, domain, op } );
            return  irb.CreateLoad( o );
        }

        template< typename IRB >
        auto get_function_from_domain( IRB &irb ) const
        {
            auto index = [&] {
                if constexpr ( Taint::toBool( T ) )
                    return llvm_index( module()->getFunction( "lart.abstract.to_tristate" ) );
                else if constexpr ( Taint::assume( T ) )
                    return llvm_index( module()->getFunction( "lart.abstract.assume" ) );
                else
                    return llvm_index( function() );
            } ();
            return get_function_from_domain( irb, index, _domain );
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
                    auto arg = irb.CreatePHI( irb.getInt8PtrTy(), 3 );
                    auto phi = llvm::cast< llvm::PHINode >( arg );
                    for ( const auto &[bb, d] : args )
                        phi->addIncoming( d, bb );
                    return phi;
                };

                _domain = irb.CreatePHI( irb.getInt8Ty(), 3 );

                auto phi = llvm::cast< llvm::PHINode >( _domain );
                for ( const auto &[bb, d] : dom )
                    phi->addIncoming( d, bb );

                vals.push_back( merge( val1 ) );
                vals.push_back( merge( val2 ) );
            }

            /*if constexpr ( T == Operation::Type::Union ) {
                auto paired = TaintArgument{ function()->arg_begin() };
                auto val = paired.abstract.value;

                ASSERT( val->getType()->isPointerTy() );
                auto cast = irb.CreateBitCast( val, function()->getReturnType() );
                irb.CreateRet( cast );
                return;
            }*/

            if constexpr ( Taint::gep( T ) )
            {
                UNREACHABLE( "not implemented" );
                /*auto paired = TaintArgument{ function()->arg_begin() };
                vals.push_back( paired.abstract.value ); // address
                auto idx = irb.CreateZExt( args[ 2 ].value, i64() );
                vals.push_back( idx ); // index*/
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
                //vals.push_back( args[ 0 ].value ); // value
                //vals.push_back( args[ 2 ].value ); // addr
                UNREACHABLE( "not implemented" );
            }

            if constexpr ( Taint::load( T ) )
            {
                //vals.push_back( args[ 1 ].value ); // addr
                UNREACHABLE( "not implemented" );
            }

            if constexpr ( Taint::thaw( T ) )
            {
                //auto addr = irb.CreateBitCast( args[ 1 ].value, i8ptr() );
                //vals.push_back( addr ); // thawed address

                /*if ( meta.scalar() ) {
                    auto bw = args[ 0 ].value->getType()->getPrimitiveSizeInBits();
                    vals.push_back( i32cv( bw ) ); // bitwidth of thawed value
                }*/
                UNREACHABLE( "not implemented" );
            }

            if constexpr ( Taint::freeze( T ) )
            {
                //vals.push_back( args[ 1 ].value ); // abstract value
                //auto cst = irb.CreateBitCast( args[ 2 ].value, i8ptr() );
                //vals.push_back( cst );             // concrete address
                UNREACHABLE( "not implemented" );
            }

            if constexpr ( Taint::cast( T ) )
            {
                auto paired = paired_arguments().front();
                vals.push_back( paired.abstract.value );

                auto cast = llvm::cast< llvm::CastInst >( taint.inst->getPrevNode() );
                auto bw = cast->getDestTy()->getPrimitiveSizeInBits();
                vals.push_back( i8cv( bw ) );

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
            auto rty = Taint::faultable( T ) ? i8ptr() : function()->getReturnType();
            auto fty = llvm::FunctionType::get( rty, types_of( vals ), false );
            auto op = irb.CreateBitCast( ptr, fty->getPointerTo() );
            auto call = irb.CreateCall( op, vals );

            if constexpr ( Taint::freeze( T ) || Taint::store( T ) ) {
                irb.CreateRet( llvm::UndefValue::get( function()->getReturnType() ) );
            } else if constexpr ( Taint::faultable( T ) ) {
                auto crty = function()->getReturnType();
                auto t = module()->getNamedGlobal( "__tainted" );
                auto load = irb.CreateLoad( t );
                auto trunc = irb.CreateTruncOrBitCast( load, crty );
                auto ret = irb.CreateRet( trunc );
                stash( ret, call );
            } else {
                irb.CreateRet( call );
            }
        }

    private:
        using IntegerType = llvm::IntegerType;
        using Constant = llvm::Constant;
        using ConstantInt = llvm::ConstantInt;

        llvm::Type * i8ptr() const { return llvm::Type::getInt8PtrTy( ctx() ); }

        llvm::Type * i8() const { return IntegerType::get( ctx(), 8 ); }
        llvm::Type * i16() const { return IntegerType::get( ctx(), 16 ); }
        llvm::Type * i32() const { return IntegerType::get( ctx(), 32 ); }
        llvm::Type * i64() const { return IntegerType::get( ctx(), 64 ); }

        Constant * i8cv( uint32_t v ) const { return ConstantInt::get( i8(), v ); }
        Constant * i16cv( uint32_t v ) const { return ConstantInt::get( i16(), v ); }
        Constant * i32cv( uint32_t v ) const { return ConstantInt::get( i32(), v ); }
        Constant * i64cv( uint32_t v ) const { return ConstantInt::get( i64(), v ); }

        llvm::LLVMContext & ctx() const { return taint.inst->getContext(); }

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

        llvm::Module * module() const { return taint.inst->getModule(); }
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
            auto m = module();

            auto fty = llvm::FunctionType::get( i8ptr(), { val->getType() }, false );
            auto name = "lart.abstract.lift_one_" + type_name( val );
            auto op = llvm_index( m->getFunction( name ) );
            auto ptr = get_function_from_domain( irb, op, dom );
            auto fn = irb.CreateBitCast( ptr, fty->getPointerTo() );

            return irb.CreateCall( fn, { val } );
        }

        llvm::Value * _domain = nullptr;

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

        #define DISPATCH( type ) \
            case Type::type: \
                Lifter< Type::type >( taint ).construct(); break;

        switch ( taint.type )
        {
            DISPATCH( PHI )
            DISPATCH( GEP )
            DISPATCH( Thaw )
            DISPATCH( Freeze )
            DISPATCH( ToBool )
            DISPATCH( Assume )
            DISPATCH( Store )
            DISPATCH( Load )
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
        }
    }

} // namespace lart::abstract
