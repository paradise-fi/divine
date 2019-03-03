// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract {

    namespace {
        const auto & TaintTable = Placeholder::TypeTable;
    }


    struct Taint
    {
        using Type = Placeholder::Type;
        static constexpr auto Tag = meta::tag::taint::type;

        static constexpr const char * prefix = "__vm_test_taint";

        explicit Taint( llvm::Instruction * inst, Type type )
            : type( type ), inst( inst )
        {
            meta::set( inst, Tag, TaintTable[ type ] );
        }

        explicit Taint( llvm::Instruction * inst )
            : Taint( inst, TaintTable[ meta::get( inst, Tag ).value() ] )
        {}

        inline llvm::Function * function() const
        {
            return llvm::cast< llvm::Function >( inst->getOperand( 0 ) );
        }

        std::string name() const
        {
            if ( Taint::cmp( type ) ) {
                if ( auto cmp = llvm::dyn_cast< llvm::CmpInst >( dual() ) ) {
                    if ( cmp->isIntPredicate() ) {
                        return "icmp_" + PredicateTable.at( cmp->getPredicate() );
                    }

                    if ( cmp->isFPPredicate() ) {
                        return "fcmp_" + PredicateTable.at( cmp->getPredicate() );
                    }

                    UNREACHABLE( "Unknown predicate type." );
                }
            }

            if ( Taint::binary( type ) || Taint::cast( type ) ) {
                return llvm::cast< llvm::Instruction >( dual() )->getOpcodeName();
            }

            return TaintTable[ type ];
        }

        llvm::Value * dual() const
        {
            ASSERT( meta::has_dual( inst ) );
            return meta::get_dual( inst );
        }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::taint::type );
        }

        template< Taint::Type... Ts >
        static constexpr bool is_one_of( Taint::Type type )
        {
            return ( (Ts == type) || ... );
        }

        static constexpr bool binary( Taint::Type type )
        {
            return is_one_of< Type::Cmp, Type::Binary >( type );
        }

        static constexpr bool cmp( Taint::Type type ) { return Type::Cmp == type; }
        static constexpr bool cast( Taint::Type type ) { return Type::Cast == type; }
        static constexpr bool assume( Taint::Type type ) { return Type::Assume == type; }
        static constexpr bool toBool( Taint::Type type ) { return Type::ToBool == type; }
        static constexpr bool freeze( Taint::Type type ) { return Type::Freeze == type; }
        static constexpr bool thaw( Taint::Type type ) { return Type::Thaw == type; }
        static constexpr bool stash( Taint::Type type ) { return Type::Stash == type; }
        static constexpr bool unstash( Taint::Type type ) { return Type::Unstash == type; }

        static std::vector< Taint > enumerate( llvm::Module & m )
        {
            return query::query( m ).flatten().flatten()
                .map( query::refToPtr )
                .filter( Taint::is )
                .map( [] ( auto * inst ) { return Taint( inst ); } )
                .freeze();
        }

        Type type;
        llvm::Instruction * inst;
    };

    struct Tainting {
        void run( llvm::Module & m );
        Taint dispach( const Placeholder & ph ) const;
    };

    static auto taints( llvm::Module & m )
    {
        return Taint::enumerate( m );
    }

    template< typename Filter >
    auto taints( llvm::Module & m, Filter filter )
    {
        return query::query( taints( m ) ).filter( filter ).freeze();
    }

    template< Taint::Type T >
    auto taints( llvm::Module & m )
    {
        return taints( m, [] ( const auto & taint ) { return taint.type == T; } );
    }

} // namespace lart::abstract
