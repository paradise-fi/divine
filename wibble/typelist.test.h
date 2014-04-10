// -*- C++ -*- (c) 2013 Vladimír Štill
#include <wibble/test.h>
#include <wibble/typelist.h>

using namespace wibble;

struct TestBoolExpr {
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
        assert( (  EvalBoolExpr< LeafFn, T >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< F > >::value ) );
        assert( (  !EvalBoolExpr< LeafFn, F >::value ) );

        assert( (  EvalBoolExpr< LeafFn, And<> >::value ) );
        assert( (  EvalBoolExpr< LeafFn, And< T > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< And< F > > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, And< T, T > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< And< T, F > > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< And< F, F > > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< And< F, F > > >::value ) );

        assert( (  EvalBoolExpr< LeafFn, Not< Or<> > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Or< T > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< Or< F > > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Or< T, T > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Or< T, F > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Or< F, T > >::value ) );
        assert( (  EvalBoolExpr< LeafFn, Not< Or< F, F > > >::value ) );
    }

    Test basic() {
        _basic< ConstTrue, True, False >();
    }

    Test leafFn() {
        _basic< Inherit, std::true_type, std::false_type >();
    }

    Test inverted() {
        _basic< Switch, std::false_type, std::true_type >();
    }
};
