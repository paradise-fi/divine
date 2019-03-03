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

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::taint::type );
        }

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
