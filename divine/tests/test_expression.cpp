#include <divine/expression.h>
#include <wibble/tests.h>

namespace tut {

struct expression_shar {};

TESTGRP( expression );

template<> template<>
void to::test< 1 >()
{
    ByteCode expr;

    expr.append( SetOp, 3, 0, 1 );
    expr.append( SetOp, 4, 0, 2 );
    expr.append( SetOp, 5, 0, 3 );
    expr.append( SetOp, 0, 0, 7 );

    expr.append( AddOp, 1, 2, 4 );
    expr.append( GtOp, 4, 3, 5 );
    expr.append( CondOp, 5, 1, 4 );

    expr.append( SubOp, 1, 2, 6 );
    expr.append( GtOp, 3, 6, 7 );
    expr.append( CondOp, 7, 1, 2 );

    expr.append( ReturnOp, 7, 0, 0 );

    expr.locations = 8;

    Evaluator ev;
    ensure_equals( ev.evaluate( expr ), true );
    /* for (int i = 1; i < 1000000; ++i ) {
        expr[0].a = i * 3;
        expr[1].a = i * 4;
        expr[2].a = i * 5;
        int res = ev.evaluate( expr );
        assert( res );
        } */
}

template<> template<>
void to::test< 2 >()
{
    Evaluator ev;
    Expression e, a, b, x, y;
    int res;

    e.op = AndOp;
    a.op = ConstOp;
    a.value = 1;

    x.op = ConstOp;
    x.value = 1;

    y.op = ConstOp;
    y.value = 2;

    b.op = GtOp;
    b.left = &x;
    b.right = &y;
    e.left = &a; e.right = &b;

    // ev.dumpCompiled( e, std::cerr );
    // std::cerr << "---" << std::endl;
    res = ev.evaluate( ev.compile( e ) );
    ensure( !res );

    b.left = &y;
    b.right = &x;
    res = ev.evaluate( ev.compile( e ) );
    ensure( res );

    a.value = 0;
    res = ev.evaluate( ev.compile( e ) );
    ensure( !res );

    e.op = OrOp;
    res = ev.evaluate( ev.compile( e ) );
    ensure( res );

    e.left = &y;
    e.right = &x;
    res = ev.evaluate( ev.compile( e ) );
    ensure( res );
}

}
