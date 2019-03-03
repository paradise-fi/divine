// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/tainting.h>

namespace lart::abstract
{
    template< Taint::Type T >
    Taint TaintBuilder::construct( const Placeholder & /*ph*/ )
    {
        UNREACHABLE( "Not implemented" );
    }

    Taint TaintBuilder::construct( const Placeholder & ph )
    {
        using Type = Taint::Type;
        assert( ph.level == Placeholder::Level::Concrete );

        switch ( ph.type )
        {
            case Type::PHI:
                return construct< Type::PHI >( ph );
            case Type::Thaw:
                return construct< Type::Thaw >( ph );
            case Type::Freeze:
                return construct< Type::Freeze >( ph );
            case Type::Stash:
                return construct< Type::Stash >( ph );
            case Type::Unstash:
                return construct< Type::Unstash >( ph );
            case Type::ToBool:
                return construct< Type::ToBool >( ph );
            case Type::Assume:
                return construct< Type::Assume >( ph );
            case Type::Store:
                return construct< Type::Store >( ph );
            case Type::Load:
                return construct< Type::Load >( ph );
            case Type::Cmp:
                return construct< Type::Cmp >( ph );
            case Type::Cast:
                return construct< Type::Cast >( ph );
            case Type::Binary:
                return construct< Type::Binary >( ph );
            case Type::Lift:
                return construct< Type::Lift >( ph );
            case Type::Lower:
                return construct< Type::Lower >( ph );
            case Type::Call:
                return construct< Type::Call >( ph );
        }

        UNREACHABLE( "Unsupported taint type" );
    };

    void Tainting::run( llvm::Module & m )
    {
        const auto Concretized = Placeholder::Level::Concrete;
        TaintBuilder builder;

        for ( const auto & ph : placeholders< Concretized >( m ) )
            builder.construct( ph );
    }

} // namespace lart::abstract
