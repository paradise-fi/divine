// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/composed.h>
#include <lart/abstract/types/scalar.h>

namespace lart {
namespace abstract {

struct AbstractValue {

    AbstractValue( llvm::Value * value, const DomainMap & dmap )
        : _value( value ),
          _type( std::make_shared< ComposedType >(
                      llvm::cast< llvm::StructType >( value->getType() ), dmap ) )
    {}

    AbstractValue( llvm::Value * value, Domain::Value dom )
        : _value( value ),
          _type( std::make_shared< ScalarType >( value->getType(), dom ) )
    {}

    AbstractValue( const AbstractValue & ) = default;
    AbstractValue( AbstractValue && ) = default;

    AbstractValue &operator=( const AbstractValue & other ) = default;
    AbstractValue &operator=( AbstractValue && other ) = default;

    Domain::Value domain() const  {
        return _type->domain();
    }

    llvm::Value * value() const {
        return _value;
    }

    template< typename T >
    decltype(auto) value() const {
        return llvm::cast< T >( _value );
    }

    const AbstractTypePtr type() const {
        return _type;
    }
private:
    llvm::Value * _value;
    AbstractTypePtr _type;
};

inline bool operator==( const AbstractValue & l, const AbstractValue & r ) {
    // TODO type
    return l.value() == r.value();
}

} // namespace abstract
} // namespace lart

namespace std {
    template <>
    struct hash< lart::abstract::AbstractValue > {
        size_t operator()( const lart::abstract::AbstractValue& n ) const {
            // TODO want root ant type ?
            return hash_value( n.value() );
        }
    };
} // namespace std
