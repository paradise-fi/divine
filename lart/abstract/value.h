// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/analysis/postorder.h>
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

inline bool operator!=( const AbstractValue & a, const AbstractValue & b ) {
    return !(a == b);
}

inline bool operator<( const AbstractValue & a, const AbstractValue & b ) {
    return std::tie( a.domain, a.value ) < std::tie( b.domain, b.value );
}

inline AbstractValues reachFrom( const AbstractValues & roots ) {
    auto succs = [&] ( const AbstractValue& av ) -> AbstractValues {
        // do not propagate through calls that are not in roots
        // i.e. that are those calls which do not return an abstract value
        bool root = std::find( roots.begin(), roots.end(), av ) != roots.end();
        if ( llvm::CallSite( av.value ) && !root )
            return {};
        if ( !av.isAbstract() )
            return {};

        return query::query( av.value->users() )
            .map( [&] ( const auto & val ) -> AbstractValue {
                return { val, av.domain };
            } ).freeze();
    };

    return lart::analysis::postorder( roots, succs );
};

inline AbstractValues reachFrom( const AbstractValue & root ) {
    return reachFrom( AbstractValues{ root } );
};

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
