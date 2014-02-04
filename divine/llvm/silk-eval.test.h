// -*- C++ -*- (c) 2014 Petr Rockai

#include <divine/llvm/silk-eval.h>
#include <divine/llvm/silk-parse.test.h>

using namespace divine::silk;
using namespace eval;

struct TestSilkEval
{
    Compiler comp;

    template< typename T >
    std::shared_ptr< T > _parse( std::string s ) {
        return std::make_shared< T >( TestSilkParse::_parse< T >( s ) );
    }

    int _expression( std::string s ) {
        auto top = _parse< Expression >( s );
        Label l = comp.compile( Compiler::thunk( top ) );
        Evaluator eval( comp.text );
        eval.enter( l, Value() );
        return eval.reduce();
    }

    Test basic() {
        assert_eq( _expression( "1 + 1" ), 2 );
        assert_eq( _expression( "2 - 1" ), 1 );
        assert_eq( _expression( "2 - (1 + 1)" ), 0 );
        assert_eq( _expression( "2 * 2 + 1" ), 5 );
        assert_eq( _expression( "2 * (2 + 1)" ), 6 );
    }

    Test lambda() {
        assert_eq( _expression( "1 + ((|x| x + 1) 4)" ), 6 );
        assert_eq( _expression( "1 + ((|x| x - 1) 4)" ), 4 );
        assert_eq( _expression( "1 + ((|x| |y| x - y) 4 2)" ), 3 );
        assert_eq( _expression( "1 + ((|x| 1 + (|y| x - y) 2) 4)" ), 4 );
        assert_eq( _expression( "(|x| 1 + (|y| x * y) 2) 4" ), 9 );
    }

    Test conditional() {
        assert_eq( _expression( "if 1 then 5 else 2" ), 5 );
        assert_eq( _expression( "1 + (if 1 then 5 else 2)" ), 6 );
        assert_eq( _expression( "(|x| if x then x + 5 else x + 2) 0" ), 2 );
        assert_eq( _expression( "(|x| if x then x + 5 else x + 2) 1" ), 6 );
    }
};
