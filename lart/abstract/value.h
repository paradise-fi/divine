// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/util.h>
#include <lart/abstract/types.h>

namespace lart {
namespace abstract {

struct AbstractValue {
    AbstractValue( llvm::Value * v, Domain d ) : value( v ), domain( d ) { }

    AbstractValue( const AbstractValue & ) = default;
    AbstractValue( AbstractValue && ) = default;
    AbstractValue &operator=( const AbstractValue & ) = default;
    AbstractValue &operator=( AbstractValue && ) = default;

    bool isAbstract() const { return domain != Domain::LLVM; }

    template< typename T >
    decltype(auto) get() const { return llvm::cast< T >( value ); }

    template< typename T >
    decltype(auto) safeGet() const { return llvm::dyn_cast< T >( value ); }

    template< typename T >
    bool isa() const { return llvm::isa< T >( value ); }

    llvm::Function * getFunction() const {
        if ( auto i = safeGet< llvm::Instruction >() )
            return i->getParent()->getParent();
        else
            return safeGet< llvm::Argument >()->getParent();
    }

    template< typename TMap >
    llvm::Type * type( TMap & tmap ) const {
        return liftType( value->getType(), domain, tmap );
    }

    llvm::Value * value;
    Domain domain;
};

using AbstractValues = std::vector< AbstractValue >;

inline bool operator==( const AbstractValue & a, const AbstractValue & b ) {
    return std::tie( a.domain, a.value ) == std::tie( b.domain, b.value );
}

} // namespace abstract
} // namespace lart

namespace std {
    template <>
    struct hash< lart::abstract::AbstractValue > {
        size_t operator()( const lart::abstract::AbstractValue & n ) const {
            return static_cast< std::size_t >( n.domain ) ^ hash_value( n.value );
        }
    };
} // namespace std
