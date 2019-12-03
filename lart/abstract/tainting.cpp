// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/tainting.h>

#include <lart/support/util.h>
#include <lart/support/annotate.h>

namespace lart::abstract {

    template< Operation::Type T >
    struct NameBuilder
    {
        using Type = Operation::Type;

        inline static const std::string c_prefix = "lart.concrete";
        inline static const std::string a_prefix = "lart.abstract";

        static auto concrete_name( llvm::Value *val ) noexcept -> std::string
        {
            return c_prefix + infix( val ) + "." + suffix( val );
        }

        static auto abstract_name( llvm::Value *val ) noexcept -> std::string
        {
            return a_prefix + infix( val ) + "." + suffix( val );
        }

        static auto result() noexcept -> std::string
        {
            return Operation::TypeTable[ T ];
        }

        #define ENABLE_IF(name) \
            typename std::enable_if_t< Taint::name( T_ ), std::string >

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( call )
        {
            auto fn = llvm::cast< llvm::CallInst >( val )->getCalledFunction();
            return fn->getName().str();
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( cmp )
        {
            auto cmp = llvm::cast< llvm::CmpInst >( val );
            return result() + "." + llvm_name( cmp->getOperand( 0 )->getType() );
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( freeze )
        {
            auto store = llvm::cast< llvm::StoreInst >( val );
            return result() + "." + llvm_name( store->getValueOperand()->getType() );
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( arith )
        {
            std::string op = llvm::cast< llvm::Instruction >( val )->getOpcodeName();
            return op + "." + llvm_name( val->getType() );
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( cast )
        {
            auto ci = llvm::cast< llvm::CastInst >( val );
            std::string op = llvm::cast< llvm::Instruction >( val )->getOpcodeName();
            auto src = llvm_name( ci->getSrcTy() );
            auto dest = llvm_name( ci->getDestTy() );
            if ( llvm::isa< llvm::PtrToIntInst >( val ) )
                return op + "." + dest;
            if ( llvm::isa< llvm::IntToPtrInst >( val ) )
                return op + "." + src;
            return op + "." + src + "." + dest;
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( insertvalue )
        {
            auto inst = llvm::cast< llvm::InsertValueInst >( val );
            return result() + "." + llvm_name( inst->getAggregateOperand()->getType() )
                            + "." + llvm_name( inst->getInsertedValueOperand()->getType() );
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept -> ENABLE_IF( extractvalue )
        {
            auto inst = llvm::cast< llvm::ExtractValueInst >( val );
            return result() + "." + llvm_name( inst->getAggregateOperand()->getType() )
                            + "." + llvm_name( inst->getType() );
        }

        template< Type T_ = T >
        static auto suffix( llvm::Value * val ) noexcept
            -> typename std::enable_if_t<
                Taint::toBool( T_ ) ||
                Taint::assume( T_ ) ||
                Taint::thaw( T_ )  ||
                Taint::store( T_ )  ||
                Taint::load( T_ )   ||
                Taint::gep( T_ )
            ,std::string >
        {
            return result() + "." + llvm_name( val->getType() );
        }

        static auto infix( llvm::Value * val ) -> std::string
        {
            if constexpr ( Taint::cmp( T ) ) {
                auto cmp = llvm::cast< llvm::CmpInst >( val );
                return "." + PredicateTable.at( cmp->getPredicate() );
            }

            return "";
        }

        #undef ENABLE_IF
    };


    template< Operation::Type T >
    struct TaintInst : LLVMUtil< TaintInst< T > >
    {
        using Self = TaintInst< T >;
        using Util = LLVMUtil< Self >;
        using Type = Operation::Type;

        using Util::i1Ty;
        using Util::i64Ty;
        using Util::i8PTy;

        using Util::i1;
        using Util::i64;

        using Util::ctx;
        using Util::argument;
        using Util::get_function;

        TaintInst( Matched & matched, llvm::Module & m )
            : _matched( matched ), module( &m )
        {}

        template< typename Default, typename Lifter >
        bool check_return_type( Default d, Lifter l ) const
        {
            auto expected = [&] () -> llvm::Type * {
                if ( auto fn = llvm::dyn_cast< llvm::Function >( d ) )
                    return fn->getReturnType();
                return d->getType();
            } ();

            return l->getReturnType() == expected;
        }

        llvm::Value * null() const noexcept
        {
            return Util::null_ptr( i8PTy() );
        }

        llvm::Value * default_value( const Operation & op )
        {
            auto ph = op.inst;

            if constexpr ( Operation::lower( T ) )
                UNREACHABLE( "Not implemented" );

            if constexpr ( Operation::toBool( T ) )
                return concrete( ph->getOperand( 0 ) );

            if constexpr ( Operation::assume( T ) )
                return null();

            if constexpr ( Operation::store( T ) )
                return null();

            if ( faultable_operation( op ) ) {
                // call concrete operation instead
                return concrete_function( ph );
            }

            return null();
        }

        llvm::Function * operation( const Operation &op )
        {
            Types args;
            for ( auto * arg : arguments( op.inst ) ) {
                args.push_back( i1Ty() );
                args.push_back( arg->getType() );
            }

            auto name = abstract_name( op );
            auto fn = get_function( name, return_type( op.inst ), args );
            Interrupt::make_invisible( fn );
            return fn;
        }

        #define ENABLE_IF(name) \
            typename std::enable_if_t< Taint::name( T_ ), Values >

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( gep )
        {
            auto gep = llvm::cast< llvm::GetElementPtrInst >( i->getOperand( 0 ) );
            auto con = gep->getPointerOperand();

            if ( auto cast = llvm::dyn_cast< llvm::BitCastInst >( con ) )
                con = cast->getOperand( 0 );

            auto abs = abstract( con );

            // FIXME ASSERT( gep->getNumIndices() == 1 );
            llvm::Value * idx = gep->idx_begin()->get();

            auto aidx = abstract( idx );

            if ( !idx->getType()->isIntegerTy( 64 ) ) {
                auto irb = llvm::IRBuilder<>( gep );
                idx = irb.CreateSExt( idx, i64Ty() );
            }

            // FIXME get rid of cast
            if ( !con->getType()->getPointerElementType()->isIntegerTy( 8 ) ) {
                auto irb = llvm::IRBuilder<>( gep );
                con = irb.CreateBitCast( con, i8PTy() );
            }

            return { con, abs, idx, aidx };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( extractvalue )
        {
            auto ev = llvm::cast< llvm::ExtractValueInst >( i->getOperand( 0 ) );
            auto agg = ev->getAggregateOperand();
            auto aagg = abstract( agg );
            // FIXME
            ASSERT( ev->getNumIndices() == 1 );
            auto idx = i64( *ev->idx_begin() );

            return { agg, aagg, idx };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( insertvalue )
        {
            auto iv = llvm::cast< llvm::InsertValueInst >( i->getOperand( 0 ) );
            auto val = iv->getInsertedValueOperand();
            auto aval = abstract( val );
            auto agg = iv->getAggregateOperand();
            auto aagg = abstract( agg );
            // FIXME
            ASSERT( iv->getNumIndices() == 1 );
            auto idx = i64( *iv->idx_begin() );

            return { agg, aagg, val, aval, idx };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( assume )
        {
            auto tobool = llvm::cast< llvm::Instruction >( i->getOperand( 0 ) );

            auto branch = query::query( tobool->users() )
                .map( query::llvmdyncast< llvm::BranchInst > )
                .filter( query::notnull )
                .freeze()[ 0 ];

            auto res = i1( branch->getSuccessor( 0 ) == i->getParent() );

            auto con = tobool->getOperand( 1 );
            auto abs = abstract( con );
            auto constraint = tobool->getOperand( 2 );
            auto cabs = abstract( constraint );
            return { con, abs, con, cabs, res };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( store )
        {
            auto s = llvm::cast< llvm::StoreInst >( concrete( i ) );
            auto val = s->getValueOperand();
            auto ptr = s->getPointerOperand();
            return { val, abstract( val ), ptr, abstract( ptr ) };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( load )
        {
            auto l = llvm::cast< llvm::LoadInst >( concrete( i ) );
            auto ptr = l->getPointerOperand();
            return { ptr, abstract( ptr ) };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( thaw )
        {
            auto l = llvm::cast< llvm::LoadInst >( concrete( i ) );
            return { l, i->getOperand( 0 ) };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( freeze )
        {
            auto s = llvm::cast< llvm::StoreInst >( concrete( i ) );
            auto con = s->getValueOperand();
            auto abs = abstract( con );
            return { con, abs, s->getPointerOperand() };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( toBool )
        {
            auto con = concrete( i->getOperand( 0 ) );
            return { con, abstract( con ) };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( cast )
        {
            auto ci = llvm::cast< llvm::CastInst >( concrete( i ) );
            auto src = ci->getOperand( 0 );
            return { src, abstract( src ) };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( binary )
        {
            auto con = concrete( i );
            auto c = llvm::cast< llvm::Instruction >( con );
            auto a = c->getOperand( 0 );
            auto da = abstract( a );
            auto b = c->getOperand( 1 );
            auto db = abstract( b );
            return { a, da, b, db };
        }

        template< Type T_ = T >
        auto arguments( llvm::Instruction * i ) -> ENABLE_IF( call )
        {
            auto call = llvm::cast< llvm::CallInst >( i->getOperand( 0 ) );

            Values args;
            for ( const auto & use : call->arg_operands() ) {
                auto arg = use.get();
                args.push_back( arg );
                args.push_back( abstract( arg ) );
            }

            return args;
        }

        #undef ENABLE_IF

        llvm::Function * concrete_function( llvm::Instruction * op )
        {
            auto args = arguments( op );
            auto name = concrete_name( op );
            auto fn = get_function( name, op->getType(), types_of( args ) );

            // call concrete instruction
            if ( fn->empty() ) {
                llvm::IRBuilder<> irb( llvm::BasicBlock::Create( ctx(), "entry", fn ) );

                llvm::ValueToValueMapTy vmap;

                auto *clone = op->getPrevNode()->clone(); // clone concrete operation
                clone->dropUnknownNonDebugMetadata();
                irb.Insert( clone );

                vmap[ op ] = clone;
                llvm::RemapInstruction( clone, vmap,
                    llvm::RF_NoModuleLevelChanges | llvm::RF_IgnoreMissingLocals );

                irb.CreateRet( clone );

                for ( size_t i = 0; i < fn->arg_size(); i += 2 ) {
                    // pass every second argument to concrete call
                    clone->setOperand( i / 2, argument( fn, i ) );
                }
            }

            return fn;
        }

        bool faultable_operation( const Operation & op ) const
        {
            if ( auto c = concrete( op.inst ) )
                if ( auto i = llvm::dyn_cast< llvm::Instruction >( c ) )
                    return is_faultable( i );
            return false;
        }

        auto abstract_name( const Operation &op ) const noexcept
        {
            if ( Operation::assume( T ) || !concrete( op.inst ) )
                return abstract_name( op.inst );
            else if constexpr ( Operation::call( T ) )
                return abstract_name( op.inst->getOperand( 0 ) ); // concrete call
            else
                return abstract_name( concrete( op.inst ) );
        }

        auto abstract_name( llvm::Value * val ) const noexcept
        {
            return NameBuilder< T >::abstract_name( val );
        }

        // name of default concrete operation
        auto concrete_name( llvm::Value * val ) const noexcept
        {
            return NameBuilder< T >::concrete_name( concrete( val ) );
        }

        Taint materialize( const Operation & op )
        {
            auto def = default_value( op );
            auto lifter = operation( op );

            ASSERT( check_return_type( def, lifter ) );

            auto placeholder = op.inst;

            Values args{ lifter, def };

            auto vals = arguments( placeholder );
            args.insert( args.end(), vals.begin(), vals.end() );

            auto rty = return_type( placeholder );
            auto tys = types_of( args );

            auto name = Taint::prefix + "." + abstract_name( op );

            auto fn = get_function( name, rty, tys );
            fn->addFnAttr( llvm::Attribute::NoUnwind );

            auto call = llvm::IRBuilder<>( placeholder ).CreateCall( fn, args );
            auto mat = Taint( call, T, true );

            inherit_index( mat, op );
            rematch( mat, op );

            return mat;
        }

        template< typename Lifter, typename Placeholder >
        void rematch( Lifter lifter, Placeholder ph )
        {
            if constexpr ( Taint::assume( T ) )
                return;

            auto lif = lifter.inst;

            if constexpr ( !Taint::toBool( T ) )
            {
                auto con = llvm::cast< llvm::Instruction >( concrete( ph.inst ) );
                if ( is_faultable( con ) ) {
                    _matched.concrete[ abstract( con ) ] = lif;
                    _matched.abstract[ lif ] = abstract( con );
                    _matched.abstract.erase( con );
                    if ( !con->getType()->isVoidTy() )
                        con->replaceAllUsesWith( lif );
                    con->eraseFromParent();
                } else {
                    _matched.match( T, lif, con );
                }
            }

            if ( !ph.inst->getType()->isVoidTy() )
                ph.inst->replaceAllUsesWith( lif );
        }

        template< typename Lifter, typename Placeholder >
        void inherit_index( Lifter lifter, Placeholder ph )
        {
            if constexpr ( Taint::assume( T ) || Taint::toBool( T ) )
                return;

            // set operation index
            auto con = llvm::cast< llvm::Instruction >( concrete( ph.inst ) );

            auto tag = meta::tag::operation::index;
            lifter.inst->setMetadata( tag, con->getMetadata( tag ) );

            lifter.function()->setMetadata( tag, con->getMetadata( tag ) );
        }

        auto concrete( llvm::Value * val )
        {
            return _matched.concrete.at( val );
        }

        auto concrete( llvm::Value * val ) const
        {
            return _matched.concrete.at( val );
        }

        auto abstract( llvm::Value * val ) const -> llvm::Value *
        {
            if ( _matched.abstract.count( val ) )
                return  _matched.abstract.at( val );
            return null();
        }

        auto return_type( llvm::Instruction * inst ) const -> llvm::Type *
        {
            if constexpr ( Operation::mem( T ) ) {
                UNREACHABLE( " not implemented" );
            }

            if constexpr ( Operation::freeze( T ) || Operation::store( T ) ) {
                return i8PTy();
            }

            return inst->getType();
        }

        Matched & _matched;
        llvm::Module * module;
    };


    Taint Tainting::dispatch( const Operation &op )
    {
        using Type = Operation::Type;

        #define DISPATCH( type ) \
            case Type::type: \
                return TaintInst< Type::type >( matched, *m ).materialize( op );

        auto m = op.inst->getModule();

        switch ( op.type )
        {
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
            // DISPATCH( Lift )
            // DISPATCH( Lower )
            // DISPATCH( Call )
            DISPATCH( ExtractValue )
            DISPATCH( InsertValue )
            // DISPATCH( Memcpy )
            // DISPATCH( Memmove )
            // DISPATCH( Memset )
            default:
                UNREACHABLE( "unsupported taint type", int( op.type ) );
        }
        UNREACHABLE( "not implemented" );

        #undef DISPATCH
    }

    void Tainting::run( llvm::Module & m )
    {
        matched.init( m );

        auto ops = operations( m );

        // process assumes as last
        std::partition( ops.begin(), ops.end(), [] ( const auto & op ) {
            return op.type != Operation::Type::Assume;
        });

        for ( const auto & op : ops )
            if ( op.type != Operation::Type::PHI )
                dispatch( op );

        for ( const auto & op : ops ) {
            auto i = op.inst;
            i->replaceAllUsesWith( llvm::UndefValue::get( i->getType() ) );
            i->eraseFromParent();
        }

        auto re = query::query( m )
            .map( query::refToPtr )
            .filter( meta::function::operation )
            .freeze();

        std::for_each( re.begin(), re.end(), [] ( auto * fn ) { fn->eraseFromParent(); } );
    }

} // namespace lart::abstract
