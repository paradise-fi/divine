// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Two heterogenous list types. TypeList is purely type-level (no data),
 * whereas Cons/Nil represent data-carrying heterogenous lists with structure
 * fixed at the type level.
 */

/*
 * (c) 2012-2013 Petr Ročkai <me@mornfall.net>
 * (c) 2013-2014 Vladimír Štill <xstill@fi.muni.cz>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <type_traits>
#include <brick-unittest.h>

#ifndef BRICK_HLIST_H
#define BRICK_HLIST_H

namespace brick {
namespace hlist {

template< size_t, typename > struct GetType;

template< typename... >
struct TypeList {
    struct Empty {};
    static constexpr size_t length = 0;
};

template< typename _T, typename... _Ts >
struct TypeList< _T, _Ts... >  : private TypeList< _Ts... > {
    using Self = TypeList< _T, _Ts... >;
    using Head = _T;
    using Tail = TypeList< _Ts... >;
    static constexpr size_t length = 1 + Tail::length;

    template< size_t i >
    using Get = typename GetType< i, Self >::T;
};

/* Basic TypeList functions ******************************************************/

template< size_t i, typename List >
struct GetType : GetType< i - 1, typename List::Tail > { };

template< typename List >
struct GetType< 0, List > { using T = typename List::Head; };

template< typename X, typename EmptyList >
struct Append {
    static_assert( std::is_same< EmptyList, TypeList<> >::value, "Invalid usage" );
    using T = TypeList< X >;
};

template< typename _X, typename _T, typename... _Ts >
struct Append< _X, TypeList< _T, _Ts... > > {
    using T = TypeList< _X, _T, _Ts... >;
};

template< template< typename > class Cond, typename List >
struct Filter : public
    std::conditional< Cond< typename List::Head >::value,
        Append< typename List::Head, typename Filter< Cond, typename List::Tail >::T >,
        Filter< Cond, typename List::Tail >
    >::type { };

template< template< typename > class Cond >
struct Filter< Cond, TypeList<> > {
    using T = TypeList<>;
};

template< template< typename > class F, typename List >
struct Map : Append< typename F< typename List::Head >::T,
                  typename Map< F, typename List::Tail >::T
             > { };

template< template< typename > class F >
struct Map< F, TypeList<> > {
    using T = TypeList<>;
};

template< typename List >
struct Last : public Last< typename List::Tail > { };

template< typename _T >
struct Last< TypeList< _T > > {
    using T = _T;
};

template< typename SeedType, template< SeedType, typename > class F,
    SeedType seed, typename List >
struct VFoldl : public VFoldl< SeedType, F,
    F< seed, typename List::Head >::value, typename List::Tail >
{ };

template< typename SeedType, template< SeedType, typename > class F,
    SeedType seed, template< typename... > class List >
struct VFoldl< SeedType, F, seed, List<> > {
    static constexpr SeedType value = seed;
};

template< typename List, typename Elem >
struct Contains : public
    std::conditional< std::is_same< typename List::Head, Elem >::value,
        std::true_type,
        Contains< typename List::Tail, Elem >
    >::type { };

template< typename Elem >
struct Contains< TypeList<>, Elem > : public std::false_type { };

template< typename List >
struct ContainsP {
    template< typename _Elem >
    struct Elem : public Contains< List, _Elem > { };
};

template< typename List, typename Reversed >
struct _ReverseImpl : public
    _ReverseImpl< typename List::Tail,
        typename Append< typename List::Head, Reversed >::T >
{ };

template< typename Reversed >
struct _ReverseImpl< TypeList<>, Reversed > {
    using T = Reversed;
};

template< typename List >
struct Reverse : public _ReverseImpl< List, TypeList<> > { };

template< typename Original, typename New, typename List >
struct Replace {
    using T = typename std::conditional<
            std::is_same< Original, typename List::Head >::value,
            typename Append< New, typename List::Tail >::T,
            typename Append< typename List::Head,
                typename Replace< Original, New, typename List::Tail >::T
            >::T
        >::type;
};

template< typename Original, typename New >
struct Replace< Original, New, TypeList<> > {
    using T = TypeList<>;
};

// 0 and 1 sized TypeLists
template< typename > struct NoDuplicates : std::true_type { };

template< typename T, typename... Ts >
struct NoDuplicates< TypeList< T, Ts... > > : std::integral_constant< bool,
    !Contains< TypeList< Ts... >, T >::value && NoDuplicates< TypeList< Ts... > >::value >
{ };

template< template< typename... > class, typename > struct Apply { };

template< template< typename... > class TCon, typename... Ts >
struct Apply< TCon, TypeList< Ts... > > {
    using T = TCon< Ts... >;
};


/* boolean expressions encoded in TypeList ***********************************/

template< typename _T >
struct Not { };

struct True { };
using False = Not< True >;

template< typename... Ts >
struct And {
    using List = TypeList< Ts... >;
};

template< typename... Ts >
struct And< TypeList< Ts ... > > : public And< Ts... > { };

template< typename... Ts >
struct Or {
    using List = TypeList< Ts... >;
};

template< typename... Ts >
struct Or< TypeList< Ts... > > : public Or< Ts... > { };


template< template< typename > class LeafFn, bool _true, typename Head >
struct _EvalBoolExprAnd3;
template< template< typename > class LeafFn, typename H >
struct _EvalBoolExprAnd3< LeafFn, false, H >;

template< template< typename > class LeafFn >
struct _EvalBoolExprAnd {
    template< bool X, typename H >
    using Fn = _EvalBoolExprAnd3< LeafFn, X, H >;
};

template< template< typename > class LeafFn, bool _false, typename Head >
struct _EvalBoolExprOr3;
template< template< typename > class LeafFn, typename H >
struct _EvalBoolExprOr3< LeafFn, true, H >;

template< template< typename > class LeafFn >
struct _EvalBoolExprOr {
    template< bool X, typename H >
    using Fn = _EvalBoolExprOr3< LeafFn, X, H >;
};

template< template< typename > class LeafFn, typename BoolExpr >
struct EvalBoolExpr : public LeafFn< BoolExpr > { };

template< template< typename > class LeafFn, typename... Ts >
struct EvalBoolExpr< LeafFn, And< Ts... > > : public
    VFoldl< bool, _EvalBoolExprAnd< LeafFn >::template Fn, true,
        typename And< Ts... >::List >
{ };

template< template< typename > class LeafFn, typename... Ts >
struct EvalBoolExpr< LeafFn, Or< Ts... > > : public
    VFoldl< bool, _EvalBoolExprOr< LeafFn >::template Fn, false,
        typename Or< Ts... >::List >
{ };

template< template< typename > class LeafFn, typename T >
struct EvalBoolExpr< LeafFn, Not< T > > {
    static constexpr bool value = !EvalBoolExpr< LeafFn, T >::value;
};

template< template< typename > class LeafFn >
struct EvalBoolExpr< LeafFn, True > : public std::true_type { };

template< template< typename > class LeafFn, bool _true, typename Head >
struct _EvalBoolExprAnd3 : public EvalBoolExpr< LeafFn, Head > { };

template< template< typename > class LeafFn, typename Head >
struct _EvalBoolExprAnd3< LeafFn, false, Head > : public std::false_type { };

template< template< typename > class LeafFn, bool _false, typename Head >
struct _EvalBoolExprOr3 : public EvalBoolExpr< LeafFn, Head > { };

template< template< typename > class LeafFn, typename Head >
struct _EvalBoolExprOr3< LeafFn, true, Head > : public std::true_type { };

/* data-carrying lists (Cons/Nil) */

struct Nil {
    static const int length = 0;
};

template< int N > struct ConsAt;

template<>
struct ConsAt< 0 > {
    template< typename Cons >
    using T = typename Cons::Car;

    template< typename Cons >
    static constexpr T< Cons > get( const Cons &c ) {
        return c.car;
    }
};

template< int N >
struct ConsAt {
    template< typename Cons >
    using T = typename ConsAt< N - 1 >::template T< typename Cons::Cdr >;

    template< typename Cons >
    static constexpr T< Cons > get( const Cons &c )
    {
        return ConsAt< N - 1 >::get( c.cdr );
    }
};

template< typename A, typename B >
struct Cons {
    typedef A Car;
    typedef B Cdr;
    A car;
    B cdr;
    static const int length = 1 + B::length;

    template< int N >
    constexpr auto get() const -> typename ConsAt< N >::template T< Cons< A, B > >
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
auto decons( const Cons &c ) -> typename ConsAt< I >::template T< Cons >
{
    return c.template get< I >();
}

constexpr Nil list() { return Nil(); }

template< typename X, typename... Xs >
constexpr auto list( X x, Xs... xs ) -> typename List< X, Xs... >::T {
    return cons( x, list( xs... ) );
}

template< typename X >
constexpr auto lookup( X, Nil, int ) -> int { return -1; }

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

namespace brick_test {
namespace hlist {

using namespace ::brick::hlist;

static_assert( std::is_same< TypeList< int, long, bool >::Get< 0 >, int >::value, "" );
static_assert( std::is_same< TypeList< int, long, bool >::Get< 1 >, long >::value, "" );
static_assert( std::is_same< TypeList< int, long, bool >::Get< 2 >, bool >::value, "" );

static_assert( NoDuplicates< TypeList<> >::value, "" );
static_assert( NoDuplicates< TypeList< int > >::value, "" );
static_assert( !NoDuplicates< TypeList< int, int > >::value, "" );
static_assert( !NoDuplicates< TypeList< int, long, bool, float, long > >::value, "" );

struct BoolExpr {
    template< typename T >
    struct ConstTrue : public std::true_type { };

    template< typename T >
    struct Inherit : public T { };

    template< typename T >
    struct Switch : public
            std::conditional< T::value, std::false_type, std::true_type >::type
    { };

    template< template< typename > class LeafFn, typename T, typename F >
    void _basic() {
        ASSERT( (  EvalBoolExpr< LeafFn, T >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< F > >::value ) );
        ASSERT( (  !EvalBoolExpr< LeafFn, F >::value ) );

        ASSERT( (  EvalBoolExpr< LeafFn, And<> >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, And< T > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< And< F > > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, And< T, T > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< And< T, F > > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< And< F, F > > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< And< F, F > > >::value ) );

        ASSERT( (  EvalBoolExpr< LeafFn, Not< Or<> > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Or< T > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< Or< F > > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Or< T, T > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Or< T, F > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Or< F, T > >::value ) );
        ASSERT( (  EvalBoolExpr< LeafFn, Not< Or< F, F > > >::value ) );
    }

    TEST(basic) {
        _basic< ConstTrue, True, False >();
    }

    TEST(leafFn) {
        _basic< Inherit, std::true_type, std::false_type >();
    }

    TEST(inverted) {
        _basic< Switch, std::false_type, std::true_type >();
    }
};

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
