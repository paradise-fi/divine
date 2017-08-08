// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2017 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS
#include <vector>
#include <lart/support/context.hpp>

#ifndef LART_SUPPORT_PASS_H
#define LART_SUPPORT_PASS_H

namespace lart {

struct Pass
{
    virtual void run( llvm::Module &m, context::Context & ) = 0;
    virtual ~Pass() { };
};

namespace detail {

template< typename T >
struct PassWrapper : Pass, T
{
    PassWrapper( T &&p ) : T( std::move( p ) ) { }

    void run( llvm::Module &m, context::Context &lctx ) override
    {
        T::run( m, lctx );
    }
};

template< typename T >
struct PassWrapperNocontext : Pass, T
{
    PassWrapperNocontext( T &&p ) : T( std::move( p ) ) { }

    void run( llvm::Module &m, context::Context & ) override
    {
        T::run( m );
    }
};

struct Preferred { };
struct NotPreferred{ NotPreferred( Preferred ) { } };

template< typename P,
          typename Check = decltype( std::declval< P & >()
                              .run( std::declval< llvm::Module & >(),
                                    std::declval< context::Context & >() ) ) >
std::unique_ptr< Pass > mkPass( P &&pass, Preferred ) {
    return std::unique_ptr< Pass >{ new detail::PassWrapper< P >( std::move( pass ) ) };
}

template< typename P >
std::unique_ptr< Pass > mkPass( P &&pass, NotPreferred ) {
    return std::unique_ptr< Pass >{ new detail::PassWrapperNocontext< P >( std::move( pass ) ) };
}

} // namespace detail

template< typename P >
std::unique_ptr< Pass > mkPass( P &&pass ) {
    return detail::mkPass( std::move( pass ), detail::Preferred() );
}

struct PassVector {

    template< typename Pass >
    void push_back( Pass &&p ) {
        _passes.push_back( mkPass( std::move( p ) ) );
    }

    template< typename Pass, typename... Args >
    void emplace_back( Args &&... args ) {
        _passes.push_back( mkPass( Pass( std::forward< Args >( args )... ) ) );
    }

    auto begin() const { return _passes.begin(); }
    auto end() const { return _passes.end(); }

  private:
    std::vector< std::unique_ptr< Pass > > _passes;
};

}

#endif
