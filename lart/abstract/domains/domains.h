// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>

#include <brick-data>
#include <brick-types>

namespace lart {
namespace abstract {

using brick::types::Union;

enum class DomainValue {
    LLVM,
    Tristate,
    Symbolic,
    Zero
};

namespace {
const brick::data::Bimap< DomainValue, std::string > DomainTable = {
     { DomainValue::LLVM, "llvm" }
    ,{ DomainValue::Tristate, "tristate" }
    ,{ DomainValue::Symbolic, "sym" }
    ,{ DomainValue::Zero, "zero" }
};
} // anonymous namespace

struct Domain;
using DomainPtr = std::shared_ptr< Domain >;

struct UnitDomain {
    UnitDomain( DomainValue && value ) : value( value ) {}
    std::string name() const { return DomainTable[ value ]; }
    bool isAbstract() const { return value != DomainValue::LLVM; }
    DomainValue value;
};

struct ComposedDomain {
    std::string name() const { return "struct"; } //TODO return domain list
    bool isAbstract() const {
        return query::query( values )
            .any( [] ( const auto & v ) {
                return v->isAbstract();
            } );
    }

    std::vector< DomainPtr > values;
};

using DomainMap = std::map< size_t, DomainPtr >;

using ADomain = Union< UnitDomain, ComposedDomain >;
struct Domain : ADomain {
    using ADomain::Union;

    std::string name() {
        return apply( [] ( const auto & d ) { return d.name(); } ).value();
    }

    bool isAbstract() {
        return apply( [] ( const auto & d ) { return d.isAbstract(); } ).value();
    }

    template< typename Value >
    static DomainPtr make( Value && val ) {
        return std::make_shared< Domain >( std::forward< Value >( val ) );
    }

    static DomainPtr make( llvm::StructType * type ) {
        ComposedDomain dom;
        for ( const auto & e : type->elements() ) {
            auto d = e->isStructTy()
                   ? make( llvm::cast< llvm::StructType >( e ) )
                   : make( DomainValue::LLVM );
            dom.values.emplace_back( d );
        }
        dom.values.resize( type->getNumElements(), make( DomainValue::LLVM ) );
        return std::make_shared< Domain >( dom );
    }

    static DomainPtr make( llvm::StructType * type, const DomainMap & dmap ) {
        auto dom = make( type );
        for( const auto & e : dmap )
            dom->get< ComposedDomain >().values.at( e.first ) = e.second;
        return dom;
    }

    static DomainPtr make( llvm::Type * type, const DomainMap & dmap ) {
        if ( type->isPointerTy() )
            type = type->getPointerElementType();
        return make( llvm::cast< llvm::StructType >( type ), dmap );
    }

    static DomainPtr make( Domain && dom ) {
        return std::make_shared< Domain >( std::forward< Domain >( dom ) );
    }
};

inline bool operator==(const Domain& l, const Domain& r) {
    return ADomain( l ) == ADomain( r );
}

inline bool operator==(const UnitDomain& l, const UnitDomain& r) {
    return l.value == r.value;
}

inline bool operator==(const ComposedDomain& l, const ComposedDomain& r) {
    return std::equal( l.values.begin(), l.values.end(), r.values.begin(),
        [] ( const DomainPtr & l, const DomainPtr & r ) {
            return (*l) == (*r);
        } );
}

const DomainPtr LLVMDomain = Domain::make( DomainValue::LLVM );
const DomainPtr SymbolicDomain = Domain::make( DomainValue::Symbolic );

static DomainPtr domain( const std::string & name ) {
    return Domain::make( DomainTable[ name ] );
}

} // namespace abstract
} // namespace lart
