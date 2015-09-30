// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/Support/Casting.h>
#include <brick-query.h>

#ifndef LART_SUPPORT_QUERY_H
#define LART_SUPPORT_QUERY_H

namespace lart {
namespace query {

using namespace brick::query;

template< typename T >
bool is( llvm::Value *v ) { return llvm::isa< T >( v ); }

template< typename T >
bool isnot( llvm::Value *v ) { return !llvm::isa< T >( v ); }

template< typename F >
bool negate( F f ) { return [=]( auto x ) { return !f( x ); }; }

static inline bool notnull( void *ptr ) { return ptr != nullptr; }

// for some reason gcc (5.2) does not compile template< typename T > auto x = []( … )…
template< typename T >
struct DynCastClosure {
    template< typename F >
    T *operator()( F *f ) { return llvm::dyn_cast< T >( f ); }
};

template< typename T >
struct CastClosure {
    template< typename F >
    T *operator()( F *f ) { return llvm::cast< T >( f ); }
};

template< typename T > DynCastClosure< T > llvmdyncast;
template< typename T > CastClosure< T > llvmcast;

} // namespace query
} // namespace lart

#endif // LART_SUPPORT_QUERY_H
