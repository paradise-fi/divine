// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types/common.h>
#include <lart/abstract/types/composed.h>
#include <lart/abstract/types/scalar.h>
#include <lart/abstract/types/utils.h>

namespace lart {
namespace abstract {

struct AbstractValue {

    AbstractValue( llvm::Value * value, DomainPtr dom )
        : _value( value ),
          _type( liftType( value->getType(), dom ) )
    {}

    AbstractValue( llvm::Value * value, const DomainMap & dmap )
        : AbstractValue( value, Domain::make( value->getType(), dmap ) )
    {}

    AbstractValue( llvm::Value * value, DomainValue dom )
        : AbstractValue( value, Domain::make( dom ) )
    {}

    AbstractValue( const AbstractValue & ) = default;
    AbstractValue( AbstractValue && ) = default;
    AbstractValue &operator=( const AbstractValue & other ) = default;
    AbstractValue &operator=( AbstractValue && other ) = default;

    template< typename T >
    decltype(auto) value() const { return llvm::cast< T >( _value ); }

    llvm::Value * value() const { return _value; }
    DomainPtr domain() const  { return _type->domain(); }
    const AbstractTypePtr type() const { return _type; }

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
            // TODO type
            return hash_value( n.value() );
        }
    };
} // namespace std
