// -*- C++ -*- (c) 2012-2013 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_LIST_H
#define DIVINE_LIST_H

#include <type_traits>

namespace divine {
namespace list {

struct Nil {
    static const int length = 0;
};

template< int N > struct ConsAt;

template<>
struct ConsAt< 0 > {
    template< typename Cons >
    static constexpr auto get( Cons &c ) -> decltype( c.car ) {
        return c.car;
    }
};

template< int N >
struct ConsAt {
    template< typename Cons >
    static constexpr auto get( Cons &c ) -> decltype( ConsAt< N - 1 >::get( c.cdr ) )
    {
        return ConsAt< N - 1 >::get( c.cdr );
    }
};

template< int, int > struct Eq;
template< int i > struct Eq< i, i > { typedef int Yes; };

template< typename A, typename B >
struct Cons {
    typedef A Car;
    typedef B Cdr;
    A car;
    B cdr;
    static const int length = 1 + B::length;

    template< int N >
    auto constexpr get() const -> decltype( ConsAt< N >::get( *this ) )
    {
        return ConsAt< N >::get( *this );
    }

    constexpr Cons( A a, B b ) : car( a ), cdr( b ) {}
};

template< typename... > struct List;
template<> struct List<> { typedef Nil T; };

template< typename X, typename... Xs >
struct List< X, Xs... > {
    using T = Cons< X, typename List< Xs... >::T >;
};

template< typename L1, typename L2 >
struct Concat {
    using R = Concat< typename L1::Cdr, L2 >;
    using T = Cons< typename L1::Car, typename R::T >;
    static constexpr T make( L1 l1, L2 l2 ) { return cons( l1.car, R::make( l1.cdr, l2 ) ); }
};

template< typename L2 >
struct Concat< Nil, L2 > {
    using T = L2;
    static constexpr T make( Nil, L2 l2 ) { return l2; }
};

template< typename A, typename B >
constexpr Cons< A, B > cons( A a, B b )
{
    return Cons< A, B >( a, b );
}

template< typename L1, typename L2 >
constexpr auto concat( L1 l1, L2 l2 ) -> typename Concat< L1, L2 >::T {
    return Concat< L1, L2 >::make( l1, l2 );
}

template< int I, typename Cons >
auto decons( Cons c ) -> decltype( ConsAt< I >::get( c ) )
{
    return c.template get< I >();
}

constexpr Nil list() { return Nil(); }

template< typename X, typename... Xs >
constexpr auto list( X x, Xs... xs ) -> typename List< X, Xs... >::T {
    return cons( x, list( xs... ) );
}

template< typename X >
constexpr auto lookup( X x, Nil, int ) -> int { return -1; }

struct preferred {};
struct not_preferred { constexpr not_preferred( preferred ) {} };

template< typename A, typename B >
constexpr bool compare( not_preferred, A, B ) {
    return false;
}

template< typename A, typename B >
constexpr auto compare( preferred, A a, B b ) -> decltype( a == b ) {
    return a == b;
}

template< typename X, typename L >
constexpr auto lookup( X x, L l, int idx = 1 ) -> typename std::enable_if< L::length != 0, int >::type {
    return compare( preferred(), x, l.car ) ? idx : lookup( x, l.cdr, idx + 1 );
}

}
}

#endif
