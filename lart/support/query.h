// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/Casting.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-query>

#ifndef LART_SUPPORT_QUERY_H
#define LART_SUPPORT_QUERY_H

namespace lart {
namespace query {

using namespace brick::query;

template< typename T >
struct IsClosure {
    template< typename F >
    bool operator()( F *v ) const { return llvm::isa< T >( v ); }
    template< typename F >
    bool operator()( F &v ) const { return llvm::isa< T >( &v ); }
};

template< typename T >
struct IsNotClosure {
    template< typename F >
    bool operator()( F *v ) const { return !llvm::isa< T >( v ); }
    template< typename F >
    bool operator()( F &v ) const { return !llvm::isa< T >( &v ); }
};

template< typename T > IsClosure< T > is;
template< typename T > IsNotClosure< T > isnot;

template< typename F >
static inline auto negate( F f ) { return [=]( auto x ) { return !f( x ); }; }

static inline bool notnull( void *ptr ) { return ptr != nullptr; }

// for some reason gcc (5.2) does not compile template< typename T > auto x = []( … )…
template< typename T >
struct DynCastClosure {
    template< typename F >
    T *operator()( F *f ) const { return llvm::dyn_cast< T >( f ); }
    template< typename F >
    T *operator()( F &f ) const { return llvm::dyn_cast< T >( &f ); }
};

template< typename T >
struct CastClosure {
    template< typename F >
    T *operator()( F *f ) { return llvm::cast< T >( f ); }
    template< typename F >
    T *operator()( F &f ) { return llvm::cast< T >( &f ); }
};

template< typename T > DynCastClosure< T > llvmdyncast;
template< typename T > CastClosure< T > llvmcast;

struct RefToPtr {
    template< typename T >
    T *operator()( T &x ) { return &x; }
};

static RefToPtr refToPtr;

template< typename F, typename G >
auto operator&&( F f, G g ) { return [f,g]( auto &&x ) { return f( x ) && g( x ); }; }

template< typename F, typename G >
auto operator||( F f, G g ) { return [f,g]( auto &&x ) { return f( x ) || g( x ); }; }

} // namespace query
} // namespace lart

#endif // LART_SUPPORT_QUERY_H
