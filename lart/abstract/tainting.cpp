// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/tainting.h>

#include <lart/support/util.h>

namespace lart::abstract
{
    constexpr bool unary( Taint::Type type )
    {
        return is< Taint::Type::Thaw >( type );
    }

    constexpr bool binary( Taint::Type type )
    {
        return is< Taint::Type::Cmp >( type ) || is< Taint::Type::Binary >( type );
    }

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
            if constexpr ( is< Taint::Type::Freeze >( T ) ) {
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
            if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
                if ( meta::has_dual( inst ) )
                        return meta::get_dual( inst );

            ASSERT( !llvm::isa< llvm::Argument >( val ) );
            // TODO deal with frozen argument
            // if ( auto arg = llvm::dyn_cast< llvm::Argument >( val ) )
            //    if ( has::unstash )
            //      dual = unstash

            return default_value();
        }

        std::vector< llvm::Value * > arguments() const
        {
            if constexpr ( is< Taint::Type::Unstash >( T ) ) {
                return { inst()->getOperand( 0 ) };
            }

            if constexpr ( is< Taint::Type::ToBool >( T ) ) {
                return { inst()->getOperand( 0 ) };
            }

            if constexpr ( is< Taint::Type::Assume >( T ) ) {
                auto tobool = llvm::cast< llvm::Instruction >( inst()->getOperand( 0 ) );

                auto branch = query::query( tobool->users() )
                    .map( query::llvmdyncast< llvm::BranchInst > )
                    .filter( query::notnull )
                    .freeze()[ 0 ];

                auto & ctx = inst()->getContext();

                llvm::Value * val = nullptr;
                if ( branch->getSuccessor( 0 ) == inst()->getParent() )
                    val = llvm::ConstantInt::getTrue( ctx );
                else
                    val = llvm::ConstantInt::getFalse( ctx );

                return { tobool->getOperand( 1 ), tobool->getOperand( 2 ), val };
            }

            if constexpr ( is< Taint::Type::Freeze >( T ) ) {
                auto s = llvm::cast< llvm::StoreInst >( meta::get_dual( inst() ) );
                auto v = s->getValueOperand();
                auto d = dual( v );
                return { v, d, s->getPointerOperand() };
            }

            if constexpr ( unary( T ) ) {
                return { inst()->getOperand( 0 ) };
            }

            if constexpr ( binary( T ) ) {
                auto i = llvm::cast< llvm::Instruction >( meta::get_dual( inst() ) );
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

        std::string name( llvm::Value * val ) const
        {
            if ( auto i = llvm::dyn_cast< llvm::Instruction >( val ) ) {
                if ( Placeholder::is( i ) )
                    return name( i->getOperand( 0 ) );
                if ( Taint::is( i ) )
                    if ( meta::has_dual( i ) )
                        return name( meta::get_dual( i ) );
            }

            std::string suffix = Placeholder::TypeTable[ T ];

            std::string infix = "";

            if constexpr ( is< Taint::Type::Cmp >( T ) ) {
                auto cmp = llvm::cast< llvm::CmpInst >( val );
                infix += "." + PredicateTable.at( cmp->getPredicate() );
            }

            if ( auto aggr = llvm::dyn_cast< llvm::StructType >( val->getType() ) ) {
                suffix += "." + aggr->getName().str();
            } else {
                suffix += "." + llvm_name( val->getType() );
            }

            auto dom = domain();
            return dom.name() + infix + "." + suffix;
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
                auto concrete = llvm::cast< llvm::Instruction >( meta::get_dual( ph.inst ) );
                meta::make_duals( taint.inst, concrete );
            }

            meta::abstract::inherit( taint.inst, ph.inst );
            if ( !ph.inst->user_empty() ) {
                ph.inst->replaceAllUsesWith( taint.inst );
            }
        }
    }

} // namespace lart::abstract
