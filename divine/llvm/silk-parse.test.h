// -*- C++ -*- (c) 2014 Petr Rockai

#include <divine/llvm/silk-parse.h>
#include <wibble/test.h>

using namespace divine::silk::parse;

struct TestSilkParse {

    template< typename T >
    static T _parse( std::string s ) {
        Buffer b( s );
        Lexer< Buffer > lexer( b );
        Parser::Context ctx( lexer, "<inline>" );
        try {
            auto r = T( ctx );
            assert( lexer.eof() );
            return r;
        } catch (...) {
            ctx.errors( std::cerr );
            throw;
        }
    }

    Test scope() {
        auto c = _parse< Scope >( "a = 2 + 3" );
        assert_eq( int(c.bindings.size()), 1 );
        auto b = c.bindings[0];
        assert_eq( b.name.name, "a" );
        auto v = b.value.e;

        auto op = v.get< BinOp >();
        assert( op.op == TI::Plus );
        Constant cnst = op.lhs->e;
        assert_eq( cnst.value, 2 );
        cnst = op.rhs->e;
        assert_eq( cnst.value, 3 );
    }

    Test scope2() {
        auto c = _parse< Scope >( "a = 2 + 3\nb = 4" );
        assert_eq( int ( c.bindings.size() ), 2 );
        auto b1 = c.bindings[0];
        assert_eq( b1.name.name, "a" );
        auto b2 = c.bindings[1];
        assert_eq( b2.name.name, "b" );
        auto cn = b2.value.e.get< Constant >();
        assert_eq( cn.value, 4 );
    }

    Test scope3() {
        auto c = _parse< Scope >( "a = 2 + 3; b = 4" );
        assert_eq( int ( c.bindings.size() ), 2 );
        auto b1 = c.bindings[0];
        assert_eq( b1.name.name, "a" );
        auto b2 = c.bindings[1];
        assert_eq( b2.name.name, "b" );
    }

    Test scope_expr() {
        SubScope c = _parse< Expression >( "{ a = 2 + 3; b = 4 }.b" ).e;
        Constant s = c.lhs->e;
        Identifier id = c.rhs->e;
    }

    Test scope_block() {
        Constant c = _parse< Expression >( "def a = 2 + 3; b = 4 end" ).e;
        assert( c.scope );
    }

    Test simple() {
        BinOp op = _parse< Expression >( "x + 3" ).e;
        assert( op.op == TI::Plus );
        Identifier id = op.lhs->e;
        Constant cnst = op.rhs->e;
        assert_eq( id.name, "x" );
        assert_eq( cnst.value, 3 );
    }

    Test lambda() {
        Lambda l = _parse< Expression >( "|x| x + 3" ).e;
        assert_eq( l.bind.name(), "x" );
        BinOp op = l.body->e;
        assert( op.op == TI::Plus );
        Identifier id = op.lhs->e;
        Constant cnst = op.rhs->e;
        assert_eq( id.name, "x" );
        assert_eq( cnst.value, 3 );
    }

    Test nestedLambda() {
        Lambda l1 = _parse< Expression >( "|x| |y| x + y" ).e;
        assert_eq( l1.bind.name(), "x" );

        Lambda l2 = l1.body->e;
        assert_eq( l2.bind.name(), "y" );

        BinOp op = l2.body->e;
        Identifier id1 = op.lhs->e;
        Identifier id2 = op.rhs->e;
        assert_eq( id1.name, "x" );
        assert_eq( id2.name, "y" );
    }

    Test ifThenElse() {
        IfThenElse i = _parse< Expression >( "if x then x + y else z + y" ).e;
        assert( i.cond->e.is< Identifier >() );
        assert( i.yes->e.is< BinOp >() );
        assert( i.no->e.is< BinOp >() );
    }

    Test complex() {
        auto i = _parse< Expression >( "if (|x| 3) then (if x + y then |a| 4 else |a| a) else |z| z + y" );
        IfThenElse top = i.e;
        Lambda cond = top.cond->e;
        IfThenElse yes = top.yes->e;
        Lambda no = top.no->e;
        BinOp op = yes.cond->e;
        assert( op.op == TI::Plus );
    }

    Test associativity() {
        BinOp top = _parse< Expression >( "1 + 2 + 3" ).e;
        BinOp left = top.lhs->e;
        Constant right = top.rhs->e;
    }

    Test subscope1() {
        BinOp top = _parse< Expression >( "1 + x.x" ).e;
        Constant left = top.lhs->e;
        SubScope right = top.rhs->e;
    }

    Test subscope2() {
        BinOp top = _parse< Expression >( "x.x + 1" ).e;
        SubScope left = top.lhs->e;
        Constant right = top.rhs->e;
    }

    Test application() {
        Application top = _parse< Expression >( "a b c" ).e;
        Application left = top.lhs->e;
        Identifier a = left.lhs->e;
        Identifier b = left.rhs->e;
        Identifier c = top.rhs->e;
        assert_eq( a.name, "a" );
        assert_eq( b.name, "b" );
        assert_eq( c.name, "c" );
    }

    Test compound() {
        Application top = _parse< Expression >( "a (b + 1) c" ).e;
        Application left = top.lhs->e;
        Identifier a = left.lhs->e;
        BinOp b = left.rhs->e;
        Identifier c = top.rhs->e;
        assert_eq( a.name, "a" );
        assert( b.op == TI::Plus );
        assert_eq( c.name, "c" );
    }

    Test print() {
        std::stringstream s;
        auto top = _parse< Expression >( "a + 3" );
        s << top;
        assert_eq( s.str(), "(a + 3)" );
    }

    Test bad1() {
        wibble::ExpectFailure _ef;
        _parse< Expression >( "a |x| a c" );
    }

    Test type() {
        Lambda top = _parse< Expression >( "|x : _t| x" ).e;
    }

    Test type_fun() {
        Lambda top = _parse< Expression >( "|x : _t -> _t| x" ).e;
    }

    Test type_pred() {
        Lambda top = _parse< Expression >( "|x : fun? _t| x" ).e;
    }
};
