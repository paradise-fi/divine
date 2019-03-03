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

        llvm::Function * operation() const
        {
            auto m = module();
            auto i1 = llvm::Type::getInt1Ty( m->getContext() );

            std::vector< llvm::Type * > args;
            for ( auto * arg : arguments() ) {
                args.push_back( i1 );
                args.push_back( arg->getType() );
            }

            llvm::Type * rty = nullptr;
            if constexpr ( Taint::freeze( T ) ) {
                rty = default_value()->getType();
            } else {
                rty = inst()->getType();
            }

            auto fty = llvm::FunctionType::get( rty, args, false );
            auto tname = name( inst()->getOperand( 0 ) );

            return util::get_or_insert_function( m, fty, tname );
        }

        llvm::Value * dual( llvm::Value * val ) const
        {
            if ( !llvm::isa< llvm::Constant >( val ) && meta::has_dual( val ) )
                return meta::get_dual( val );
            // TODO deal with frozen argument
            return default_value();
        }

        std::vector< llvm::Value * > arguments() const
        {
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
                auto value = meta::get_dual( llvm::cast< llvm::Instruction >( concrete ) );
                auto constraint = tobool->getOperand( 2 );
                return { concrete, value, constraint, res };
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

            if constexpr ( Taint::binary( T ) ) {
                auto i = llvm::cast< llvm::Instruction >( dual( inst() ) );
                auto a = i->getOperand( 0 );
                auto da = dual( a );
                auto b = i->getOperand( 1 );
                auto db = dual( b );
                return { a, da, b, db };
            }

            UNREACHABLE( "Not implemented" );
        }

        llvm::Value * default_value() const
        {
            if constexpr ( T == Taint::Type::ToBool ) {
                return meta::get_dual( llvm::cast< llvm::Instruction >( inst()->getOperand( 0 ) ) );
            }

            if constexpr ( T == Taint::Type::Lower ) {
                UNREACHABLE( "Not implemented" );
            }

            return DomainMetadata::get( module(), domain() ).default_value();
        }

        std::string suffix( llvm::Value * val ) const
        {
            std::string res = TaintTable[ T ];

            if constexpr ( Taint::cmp( T ) ) {
                auto cmp = llvm::cast< llvm::CmpInst >( val );
                return res + "." + llvm_name( cmp->getOperand( 0 )->getType() );
            }

            if constexpr ( Taint::thaw( T ) ) {
                return res + "." + llvm_name( val->getType()->getPointerElementType() );
            }

            if constexpr ( Taint::binary( T ) ) {
                return llvm::cast< llvm::Instruction >( val )->getOpcodeName();
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
            case Type::PHI:
                return TaintBuilder< Type::PHI >( ph ).construct();
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
        }

        UNREACHABLE( "Unsupported taint type" );
    };

    void Tainting::run( llvm::Module & m )
    {
        const auto Concretized = Placeholder::Level::Concrete;

        for ( const auto & ph : placeholders< Concretized >( m ) ) {
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