// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS
#include <vector>

#ifndef LART_SUPPORT_PASS_H
#define LART_SUPPORT_PASS_H

namespace lart {

struct Pass
{
    virtual void run( llvm::Module &m ) = 0;
    virtual ~Pass() { };
};

namespace detail {

template< typename T >
struct PassWrapper : Pass, T
{
    PassWrapper( T &&p ) : T( std::move( p ) ) { }

    void run( llvm::Module &m ) override
    {
        T::run( m );
    }
};

} // namespace detail

template< typename P >
std::unique_ptr< Pass > mkPass( P &&pass ) {
    return std::unique_ptr< Pass >{ new detail::PassWrapper< P >( std::move( pass ) ) };
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

template< typename... Passes >
struct ChainedPass {
    using passes_t = std::tuple< Passes... >;

    ChainedPass() : passes( std::tuple< Passes... >() ) {}
    ChainedPass( Passes... passes ) : passes( std::forward_as_tuple( passes... ) ) {}

    void run( llvm::Module & m ) {
        apply( [&]( auto... p ) { ( p.run( m ),... ); }, passes );
    }

    passes_t passes;
};

template < typename... Passes >
auto make_chained_pass( Passes... passes ) {
    return ChainedPass< Passes... >( passes... );
}

}

#endif
