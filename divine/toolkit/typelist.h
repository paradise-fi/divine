// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>

#include <wibble/sfinae.h>

#ifndef DIVINE_TOOLKIT_TYPELIST
#define DIVINE_TOOLKIT_TYPELIST

namespace divine {

/* TypeList ******************************************************************/

template< typename... >
struct TypeList {
    using Empty = wibble::Unit;
    static constexpr size_t length = 0;
};

template< typename _T, typename... _Ts >
struct TypeList< _T, _Ts... >  : private TypeList< _Ts... > {
    using Head = _T;
    using Tail = TypeList< _Ts... >;
    static constexpr size_t length = 1 + Tail::length;
};

/* Basic list functions ******************************************************/

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
        Filter< Cond, typename List::Tail >,
        Append< typename List::Head, typename Filter< Cond, typename List::Tail >::T >
    >::type { };

template< template< typename > class Cond >
struct Filter< Cond, TypeList<> > {
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
    static constexpr const SeedType value = seed;
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
    static constexpr const bool value = !EvalBoolExpr< LeafFn, T >::value;
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

}


#endif // DIVINE_TOOLKIT_TYPELIST
