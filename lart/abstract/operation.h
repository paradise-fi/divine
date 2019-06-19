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

    using brick::data::Bimap;

    struct Operation : brick::types::Ord
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
            Call,
            Memcpy,
            Memmove,
            Memset,
            Union
        };

        static const Bimap< Type, std::string > TypeTable;

        explicit Operation( llvm::Instruction * inst, Type type )
            : inst( inst ), type( type )
        {
            meta::set( inst, meta::tag::operation::type, TypeTable[ type ] );
        }

        explicit Operation( llvm::Instruction * inst )
            : Operation( inst, TypeTable[ meta::get( inst, meta::tag::operation::type ).value() ] )
        { }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::operation::type );
        }

        static std::vector< Operation > enumerate( llvm::Module & m )
        {
            return query::query( m ).flatten().flatten()
                .map( query::refToPtr )
                .filter( Operation::is )
                .map( [] ( auto * inst ) { return Operation( inst ); } )
                .freeze();
        }

        auto as_tuple() const
        {
            return std::tuple( inst, type );
        }

        bool operator==( const Operation & other ) const
        {
            return this->as_tuple() == other.as_tuple();
        }

        bool operator<( const Operation & other ) const
        {
            return this->as_tuple() < other.as_tuple();
        }

        friend std::ostream& operator<<( std::ostream & os, const Operation & ph )
        {
            auto fn = llvm::cast< llvm::CallInst >( ph.inst )->getCalledFunction();
            os << "[" << fn->getName().str() << ", " << TypeTable[ ph.type ] << "]";
            return os;
        }

        llvm::Instruction * inst;
        Type type;
    };

    template< Operation::Type T >
    struct Construct
    {
        using Type = Operation::Type;

        static constexpr const char * prefix = "lart.placeholder.";

        template< typename Value, typename Builder >
        Operation operation( Value * val, Builder & builder );
        Operation operation( llvm::Instruction * inst );
        Operation operation( llvm::Argument * arg );

    private:
        auto arguments( llvm::Value * val ) -> std::vector< llvm::Value * >;

        auto output( llvm::Value * val ) -> llvm::Type *;
        auto function_type( llvm::Value * val ) -> llvm::FunctionType *;

        std::string name_suffix( llvm::Value * val );
        std::string name( llvm::Value * val );
    };

    struct OperationBuilder
    {
        using Type = Operation::Type;

        Operation construct( llvm::Instruction * inst );

        template< Type T, typename Value >
        Operation construct( Value * val )
        {
            return Construct< T >().operation( val );
        }

        template< Type T, typename Value, typename Builder >
        Operation construct( Value * val, Builder & builder )
        {
            return Construct< T >().operation( val, builder );
        }
    };

    static inline constexpr bool to_concrete( Operation::Type T )
    {
        return T == Operation::Type::ToBool || T == Operation::Type::Lower;
    }

    template< Operation::Type... Ts >
    constexpr bool is_any_of( Operation::Type type )
    {
        return ( (Ts == type) || ... );
    }

    static inline constexpr bool is_mem_intrinsic( Operation::Type T )
    {
        using Type = Operation::Type;
        return is_any_of< Type::Memset, Type::Memcpy, Type::Memmove >( T );
    }

    static inline auto operations( llvm::Module & m )
    {
        return Operation::enumerate( m );
    }

    template< typename Filter >
    auto operations( llvm::Module & m, Filter filter )
    {
        return query::query( operations( m ) ).filter( filter ).freeze();
    }

    template< Operation::Type T >
    auto operations( llvm::Module & m )
    {
        return operations( m, [] ( const auto & ph ) { return ph.type == T; } );
    }

} // namespace lart::abstract

#include <lart/abstract/operation.tpp>
