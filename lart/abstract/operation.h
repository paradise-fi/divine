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
            ToBool,
            Assume,
            Store,
            Load,
            Cmp,
            Cast,
            Binary,
            BinaryFaultable,
            Lift,
            Lower,
            Call,
            Memcpy,
            Memmove,
            Memset
        };

        static const Bimap< Type, std::string > TypeTable;

        explicit Operation( llvm::Instruction * inst, Type type, bool taint )
            : inst( inst ), type( type ), taint( taint )
        {
            meta::set( inst, meta::tag::operation::type, TypeTable[ type ] );
        }

        explicit Operation( llvm::Instruction * inst, bool taint = false )
            : Operation( inst, TypeTable[ meta::get( inst, meta::tag::operation::type ).value() ], taint )
        { }

        bool is_taint() const { return taint; }

        static bool is( llvm::Instruction * inst )
        {
            return meta::has( inst, meta::tag::operation::type );
        }

        static std::vector< Operation > enumerate( llvm::Module & m, bool taints = false )
        {
            return query::query( m ).flatten().flatten()
                .map( query::refToPtr )
                .filter( Operation::is )
                .map( [taints] ( auto * inst ) { return Operation( inst, taints ); } )
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

        template< Operation::Type... Ts >
        static constexpr bool is_one_of( Operation::Type type )
        {
            return ( (Ts == type) || ... );
        }

        static constexpr bool binary( Operation::Type type )
        {
            return is_one_of< Type::Cmp, Type::Binary, Type::BinaryFaultable >( type );
        }

        static constexpr bool arith( Operation::Type type ) {
            return Type::Binary == type || Type::BinaryFaultable == type;
        }

        static constexpr bool gep( Operation::Type type ) { return Type::GEP == type; }
        static constexpr bool store( Operation::Type type ) { return Type::Store == type; }
        static constexpr bool load( Operation::Type type ) { return Type::Load == type; }
        static constexpr bool cmp( Operation::Type type ) { return Type::Cmp == type; }
        static constexpr bool cast( Operation::Type type ) { return Type::Cast == type; }
        static constexpr bool assume( Operation::Type type ) { return Type::Assume == type; }
        static constexpr bool toBool( Operation::Type type ) { return Type::ToBool == type; }
        static constexpr bool freeze( Operation::Type type ) { return Type::Freeze == type; }
        static constexpr bool thaw( Operation::Type type ) { return Type::Thaw == type; }
        static constexpr bool call( Operation::Type type ) { return Type::Call == type; }
        static constexpr bool lift( Operation::Type type ) { return Type::Lift == type; }
        static constexpr bool lower( Operation::Type type ) { return Type::Lower == type; }

        static constexpr bool mem( Operation::Type type )
        {
            return is_one_of< Type::Memcpy, Type::Memmove, Type::Memset >( type );
        }

        static constexpr bool faultable( Operation::Type type )
        {
            // TODO memory operations
            return is_one_of< Type::Store, Type::Load, Type::BinaryFaultable, Type::Call >( type );
        }

        llvm::Instruction * inst;
        Type type;

        bool taint;
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

    static inline auto operations( llvm::Module & m )
    {
        return Operation::enumerate( m, false /* taint */ );
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


    struct Matched
    {
        void init( llvm::Module & m );

        void match( Operation::Type type, llvm::Value * a, llvm::Value * c );

        std::map< llvm::Value *, llvm::Value * > concrete;
        std::map< llvm::Value *, llvm::Value * > abstract;
    };

} // namespace lart::abstract

#include <lart/abstract/operation.tpp>
