// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>
#include <lart/support/util.h>

#include <brick-types>

namespace lart::abstract {

    struct Placeholder : brick::types::Ord
    {
        enum class Type {
            PHI,
            GEP,
            Thaw,
            Freeze,
            Stash,
            Unstash,
            ToBool,
            Assume,
            Store,
            Load,
            Cmp,
            Cast,
            Binary,
            Lift,
            Lower,
            Call
        };

        static const Bimap< Type, std::string > TypeTable;

        enum class Level {
            Abstract, // represents abstract operation place
            Concrete  // represents concretized abstract place
        };

        static const Bimap< Level, std::string > LevelTable;

        explicit Placeholder( llvm::Instruction * inst, Level level, Type type )
            : inst( inst ), level( level ), type( type )
        {
            meta::set( inst, meta::tag::placeholder::level, LevelTable[ level ] );
            meta::set( inst, meta::tag::placeholder::type, TypeTable[ type ] );
        }

        explicit Placeholder( llvm::Instruction * inst )
            : inst( inst )
        {
            level = LevelTable[ meta::get( inst, meta::tag::placeholder::level ).value() ];
            type = TypeTable[ meta::get( inst, meta::tag::placeholder::type ).value() ];
        }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::placeholder::type );
        }

        static std::vector< Placeholder > enumerate( llvm::Module & m )
        {
            return query::query( m ).flatten().flatten()
                .map( query::refToPtr )
                .filter( Placeholder::is )
                .map( [] ( auto * inst ) { return Placeholder( inst ); } )
                .freeze();
        }

        auto as_tuple() const
        {
            return std::tuple( inst, level, type );
        }

        bool operator==( const Placeholder & other ) const
        {
            return this->as_tuple() == other.as_tuple();
        }

        bool operator<( const Placeholder & other ) const
        {
            return this->as_tuple() < other.as_tuple();
        }

        friend std::ostream& operator<<( std::ostream & os, const Placeholder & ph )
        {
            auto fn = llvm::cast< llvm::CallInst >( ph.inst )->getCalledFunction();
            os << "[" << fn->getName().str()
               << ", " << LevelTable[ ph.level ]
               << ", " << TypeTable[ ph.type ] << "]";
            return os;
        }

        llvm::Instruction * inst;
        Level level;
        Type type;
    };

    constexpr bool abstract_level( Placeholder::Level L )
    {
        return L == Placeholder::Level::Abstract;
    }

    constexpr bool concrete_level( Placeholder::Level L )
    {
        return L == Placeholder::Level::Concrete;
    }


    template< Placeholder::Level L, Placeholder::Type T >
    struct Construct
    {
        using Type = Placeholder::Type;
        using Level = Placeholder::Level;

        static constexpr const char * prefix = "lart.placeholder";

        template< typename Value, typename Builder >
        Placeholder placeholder( Value * val, Builder & builder );
        Placeholder placeholder( llvm::Instruction * inst );
        Placeholder placeholder( llvm::Argument * arg );
    private:
        auto arguments( llvm::Value * val ) -> std::vector< llvm::Value * >;

        auto output( llvm::Value * val ) -> llvm::Type *;
        auto function_type( llvm::Value * val ) -> llvm::FunctionType *;

        std::string name_suffix( llvm::Value * val );
        std::string name( llvm::Value * val );
    };


    struct Empty {};

    struct ConcretizationBuilder
    {
        using Map = std::map< Placeholder, Placeholder >;
        Map concretized;
    };


    template< Placeholder::Level L, Placeholder::Type T >
    struct Concretize
    {
        using Type = Placeholder::Type;
        using Level = Placeholder::Level;
        using Map = ConcretizationBuilder::Map;

        Placeholder placeholder( const Placeholder & ph, Map & map );
    };


    template< Placeholder::Level L >
    struct PlaceholderBuilder : std::conditional_t< concrete_level( L ), ConcretizationBuilder, Empty >
    {
        using Type = Placeholder::Type;
        using Level = Placeholder::Level;

        Placeholder construct( llvm::Instruction * inst );

        template< Type T, typename Value >
        Placeholder construct( Value * val )
        {
            return Construct< L, T >().placeholder( val );
        }

        template< Type T, typename Value, typename Builder >
        Placeholder construct( Value * val, Builder & builder )
        {
            return Construct< L, T >().placeholder( val, builder );
        }

        template< Type T >
        Placeholder concretize( const Placeholder & ph )
        {
            static_assert( concrete_level( L ) );
            assert( abstract_level( ph.level ) );
            return Concretize< L, T >().placeholder( ph, this->concretized );
        }

        Placeholder concretize( const Placeholder & ph );
    };

    template< Placeholder::Type... Ts >
    static constexpr bool is_one_of( Placeholder::Type type )
    {
        return ( (Ts == type) || ... );
    }

    static inline auto placeholders( llvm::Module & m )
    {
        return Placeholder::enumerate( m );
    }

    template< typename Filter >
    auto placeholders( llvm::Module & m, Filter filter )
    {
        return query::query( placeholders( m ) ).filter( filter ).freeze();
    }

    template< Placeholder::Type T >
    auto placeholders( llvm::Module & m )
    {
        return placeholders( m, [] ( const auto & ph ) { return ph.type == T; } );
    }

    template< Placeholder::Level L >
    auto placeholders( llvm::Module & m )
    {
        return placeholders( m, [] ( const auto & ph ) { return ph.level == L; } );
    }

    using APlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Abstract >;
    using CPlaceholderBuilder = PlaceholderBuilder< Placeholder::Level::Concrete >;

} // namespace lart::abstract

#include <lart/abstract/placeholder.tpp>
