// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/tainting.h>

#include <lart/support/util.h>

namespace lart::abstract
{
    template< Taint::Type T >
    struct TaintBuilder
    {
        TaintBuilder( const Placeholder & ph ) : ph( ph ) {}

        Taint construct()
        {
            auto def = default_value();
            auto op = operation();

            ASSERT( op->getReturnType() == def->getType() );

            std::vector< llvm::Value * > args;
            args.push_back( operation() );
            args.push_back( def );

            auto vals = arguments();
            args.insert( args.end(), vals.begin(), vals.end() );

            auto rty = default_value()->getType();
            auto tys = types_of( args );
            auto fty = llvm::FunctionType::get( rty, tys, false );
            auto tname = name( inst()->getOperand( 0 ) );
            auto name = std::string( Taint::prefix ) + "." + tname;

            auto m = module();
            auto function = util::get_or_insert_function( m, fty, name );

            llvm::IRBuilder<> irb( inst() );

            auto taint = irb.CreateCall( function, args );
            return Taint{ taint, T };
        }

        llvm::Type * return_type() const
        {
            if constexpr ( Taint::freeze( T ) || Taint::store( T ) || Taint::mem( T ) )
            {
                return default_value()->getType();
            }

            return inst()->getType();
        }

        llvm::Function * operation() const
        {
            auto m = module();
            auto i1 = llvm::Type::getInt1Ty( m->getContext() );

            std::vector< llvm::Type * > args;
            for ( auto * arg : arguments() ) {
                args.push_back( i1 );
                args.push_back( arg->getType() );
            }

            auto fty = llvm::FunctionType::get( return_type(), args, false );
            auto tname = name( inst()->getOperand( 0 ) );

            return util::get_or_insert_function( m, fty, tname );
        }

        llvm::Value * dual( llvm::Value * val ) const
        {
            if ( meta::has_dual( val ) )
                return meta::get_dual( val );
            // TODO deal with frozen argument
            return default_value();
        }

        std::vector< llvm::Value * > arguments() const
        {
            if constexpr ( Taint::gep( T ) ) {
                auto gep = llvm::cast< llvm::GetElementPtrInst >( inst()->getOperand( 0 ) );

                auto concrete = gep->getPointerOperand();

                if ( auto cast = llvm::dyn_cast< llvm::BitCastInst >( concrete ) )
                    concrete = cast->getOperand( 0 );

                ASSERT( meta::has_dual( concrete ) );
                auto abstract = dual( concrete );

                ASSERT( gep->getNumIndices() == 1 );
                llvm::Value * idx = gep->idx_begin()->get();
                return { concrete, abstract, idx };
            }

            if constexpr ( Taint::assume( T ) ) {
                auto tobool = llvm::cast< llvm::Instruction >( inst()->getOperand( 0 ) );

                auto branch = query::query( tobool->users() )
                    .map( query::llvmdyncast< llvm::BranchInst > )
                    .filter( query::notnull )
                    .freeze()[ 0 ];

                auto & ctx = inst()->getContext();

                llvm::Value * res = nullptr;
                if ( branch->getSuccessor( 0 ) == inst()->getParent() )
                    res = llvm::ConstantInt::getTrue( ctx );
                else
                    res = llvm::ConstantInt::getFalse( ctx );

                auto concrete = tobool->getOperand( 1 );
                ASSERT( meta::has_dual( concrete ) );
                auto abstract = dual( concrete );
                auto constraint = tobool->getOperand( 2 );
                return { concrete, abstract, constraint, res };
            }

            if constexpr ( Taint::store( T ) ) {
                auto s = llvm::cast< llvm::StoreInst >( dual( inst() ) );
                auto val = s->getValueOperand();
                auto concrete = s->getPointerOperand();
                auto abstract = dual( concrete );
                return { val, concrete, abstract };
            }

            if constexpr ( Taint::load( T ) ) {
                auto l = llvm::cast< llvm::LoadInst >( dual( inst() ) );
                auto concrete = l->getPointerOperand();
                auto abstract = dual( concrete );
                return { concrete, abstract };
            }

            if constexpr ( Taint::thaw( T ) ) {
                auto l = llvm::cast< llvm::LoadInst >( dual( inst() ) );
                return { l, inst()->getOperand( 0 ) };
            }

            if constexpr ( Taint::freeze( T ) ) {
                auto s = llvm::cast< llvm::StoreInst >( dual( inst() ) );
                auto v = s->getValueOperand();
                auto d = dual( v );
                return { v, d, s->getPointerOperand() };
            }

            if constexpr ( Taint::unstash( T ) ) {
                return { inst()->getOperand( 0 ) };
            }

            if constexpr ( Taint::toBool( T ) || Taint::stash( T ) ) {
                auto op = inst()->getOperand( 0 );
                return { op, dual( op ) };
            }

            if constexpr ( Taint::cast( T ) ) {
                auto ci = llvm::cast< llvm::CastInst >( dual( inst() ) );
                auto src = ci->getOperand( 0 );
                return { src, dual( src ) };
            }

            if constexpr ( Taint::binary( T ) ) {
                auto i = llvm::cast< llvm::Instruction >( dual( inst() ) );
                auto a = i->getOperand( 0 );
                auto da = dual( a );
                auto b = i->getOperand( 1 );
                auto db = dual( b );
                return { a, da, b, db };
            }

            if constexpr ( Taint::call( T ) || Taint::mem( T ) ) {
                auto call = llvm::cast< llvm::CallInst >( dual( inst() ) );

                std::vector< llvm::Value * > args;
                for ( const auto & use : call->arg_operands() ) {
                    auto arg = use.get();
                    args.push_back( arg );
                    if ( meta::has_dual( arg ) ) {
                        args.push_back( dual( arg ) );
                    }
                }

                return args;
            }

            UNREACHABLE( "Not implemented" );
        }

        llvm::Value * default_value() const
        {
            if constexpr ( Taint::toBool( T ) ) {
                return dual( inst()->getOperand( 0 ) );
            }

            if constexpr ( Taint::load( T ) ) {
                return dual( inst() );
            }

            if constexpr ( Taint::lower( T ) ) {
                UNREACHABLE( "Not implemented" );
            }

            if constexpr ( Taint::call( T ) ) {
                auto fn = domain_function();
                auto meta = DomainMetadata::get( module(), domain() );
                if ( fn->getReturnType() == meta.base_type() )
                    return meta.default_value();
                return dual( inst() );
            }

            return DomainMetadata::get( module(), domain() ).default_value();
        }

        llvm::Function * domain_function() const
        {
            static_assert( Taint::call( T ) );
            std::string name = "__" + domain().name() + "_" + suffix( dual( inst() ) );
            if ( auto fn = module()->getFunction( name ) )
                return fn;
            UNREACHABLE( "Unknown domain function: " + name );
        }

        std::string suffix( llvm::Value * val ) const
        {
            std::string res = TaintTable[ T ];

            if constexpr ( Taint::call( T ) ) {
                auto fn = llvm::cast< llvm::CallInst >( val )->getCalledFunction();
                return fn->getName().str();
            }

            if constexpr ( Taint::cmp( T ) ) {
                auto cmp = llvm::cast< llvm::CmpInst >( val );
                return res + "." + llvm_name( cmp->getOperand( 0 )->getType() );
            }

            if constexpr ( Taint::thaw( T ) ) {
                return res + "." + llvm_name( val->getType()->getPointerElementType() );
            }

            if constexpr ( Taint::binary( T ) ) {
                std::string op = llvm::cast< llvm::Instruction >( val )->getOpcodeName();
                return op + "." + llvm_name( val->getType() );
            }

            if constexpr ( Taint::cast( T ) ) {
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

            if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() ) ) {
                return res + "." + aggr->getName().str();
            } else {
                return res + "." + llvm_name( val->getType() );
            }
        }

        std::string infix( llvm::Value * val ) const
        {
            if constexpr ( Taint::cmp( T ) ) {
                auto cmp = llvm::cast< llvm::CmpInst >( val );
                return "." + PredicateTable.at( cmp->getPredicate() );
            }
            return "";
        }

        std::string name( llvm::Value * val ) const
        {
            if ( auto i = llvm::dyn_cast< llvm::Instruction >( val ) ) {
                if ( Placeholder::is( i ) )
                    return name( i->getOperand( 0 ) );
                if ( Taint::is( i ) )
                    if ( meta::has_dual( i ) )
                        return name( meta::get_dual( i ) );
            }

            return domain().name() + infix( val ) + "." + suffix( val );
        }


        llvm::Instruction * inst() const { return ph.inst; }

        llvm::Module * module() const { return inst()->getModule(); }

        Domain domain() const { return Domain::get( inst() ); }
    private:
        const Placeholder & ph;
    };

    Taint Tainting::dispach( const Placeholder & ph ) const
    {
        using Type = Taint::Type;
        assert( ph.level == Placeholder::Level::Concrete );

        switch ( ph.type )
        {
            case Type::GEP:
                return TaintBuilder< Type::GEP >( ph ).construct();
            case Type::Thaw:
                return TaintBuilder< Type::Thaw >( ph ).construct();
            case Type::Freeze:
                return TaintBuilder< Type::Freeze >( ph ).construct();
            case Type::Stash:
                return TaintBuilder< Type::Stash >( ph ).construct();
            case Type::Unstash:
                return TaintBuilder< Type::Unstash >( ph ).construct();
            case Type::ToBool:
                return TaintBuilder< Type::ToBool >( ph ).construct();
            case Type::Assume:
                return TaintBuilder< Type::Assume >( ph ).construct();
            case Type::Store:
                return TaintBuilder< Type::Store >( ph ).construct();
            case Type::Load:
                return TaintBuilder< Type::Load >( ph ).construct();
            case Type::Cmp:
                return TaintBuilder< Type::Cmp >( ph ).construct();
            case Type::Cast:
                return TaintBuilder< Type::Cast >( ph ).construct();
            case Type::Binary:
                return TaintBuilder< Type::Binary >( ph ).construct();
            case Type::Lift:
                return TaintBuilder< Type::Lift >( ph ).construct();
            case Type::Lower:
                return TaintBuilder< Type::Lower >( ph ).construct();
            case Type::Call:
                return TaintBuilder< Type::Call >( ph ).construct();
            case Type::Memcpy:
                return TaintBuilder< Type::Memcpy >( ph ).construct();
            case Type::Memmove:
                return TaintBuilder< Type::Memmove >( ph ).construct();
            case Type::Memset:
                return TaintBuilder< Type::Memset >( ph ).construct();
            default:
                UNREACHABLE( "Unsupported taint type" );
        }

    };

    void Tainting::run( llvm::Module & m )
    {
        const auto Concretized = Placeholder::Level::Concrete;

        auto places = placeholders< Concretized >( m );

        std::partition( places.begin(), places.end(), [] ( const auto & ph ) {
            return ph.type != Placeholder::Type::Assume;
        });

        for ( const auto & ph : places ) {
            auto taint = dispach( ph );

            if ( meta::has_dual( ph.inst ) ) {
                auto concrete = meta::get_dual( ph.inst );
                meta::make_duals( concrete, taint.inst );
            }

            meta::abstract::inherit( taint.inst, ph.inst );
            if ( !ph.inst->user_empty() ) {
                ph.inst->replaceAllUsesWith( taint.inst );
            }
            ph.inst->eraseFromParent();
        }

        auto remove = query::query( m )
            .map( query::refToPtr )
            .filter( meta::function::placeholder )
            .freeze();

        std::for_each( remove.begin(), remove.end(), [] ( auto * fn ) { fn->eraseFromParent(); } );
    }

} // namespace lart::abstract
