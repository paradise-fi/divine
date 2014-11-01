// -*- C++ -*- (c) 2014 Petr Rockai

#include <divine/llvm/silk-parse.h>

#ifndef DIVINE_LLVM_SILK_EVAL_H
#define DIVINE_LLVM_SILK_EVAL_H

namespace divine {
namespace silk {

/*
 * A very simple strict evaluator for silk expressions. Uses 3 stacks:
 * - primitive values: integers and lambdas
 * - operations: prims and application
 * - scopes: keep track of variable mappings
 *
 * Each lambda in the program is compiled down to a pair of stacks, one for
 * operations and second for values and symbol refs. When the evaluator sees an
 * application, the top of the value stack is interpreted as an index into the
 * compiled program text. The opstack from the program text is pushed (in
 * reverse order) on the evaluator's opstack, the value stack is copied onto
 * the value stack (again, reverse order), dereferencing (copying) all free
 * variables in the process (sort-of lambda lifting).
 */

namespace eval {

struct Value { int v; explicit Value( int v ) : v( v ) {} Value() : v( 0 ) {} };

using SymbolMap = std::map< std::string, int >;
struct LabelMap { int counter; LabelMap() : counter( 0 ) {} };

struct Symbol {
    int id;
    Symbol( SymbolMap &m, std::string n ) {
        if ( m.find( n ) == m.end() )
            id = m[ n ] = m.size();
        else
            id = m[ n ];
    }
};

/*
 * An immediate use of a symbol (as opposed to a 'quoted symbol or a
 * right-hand-side of subscope access -- .).
 */
struct Use : Symbol {
    Use( SymbolMap &m, std::string n ) : Symbol( m, n ) {}
};

using Op = parse::TI; // a little bit of abuse to avoid conversions

struct Label {
    int id;
    explicit Label( LabelMap &m )
        : id( m.counter++ )
    {}
    explicit Label( int i )
        : id( i )
    {}
};

using SymItem = brick::types::Union< Value, Use, Symbol, Op >;
using ConItem = brick::types::Union< Value, Op >;
using SymStack = std::vector< SymItem >;
using ConStack = std::vector< ConItem >;
using DefBlock = std::map< int, int >;
using DefBlocks = std::vector< DefBlock >;


/* map an identifier to a value; each lambda binds exactly one value, hence we
 * always know where to pop when leaving a lambda; indexed by symbol IDs */
using ScopeStack = std::vector< std::vector< Value > >;

struct Text {
    void add( Label l, Symbol bind, const SymStack &c ) {
        ASSERT_EQ( l.id, int( code.size() ) );
        ASSERT_EQ( l.id, int( binders.size() ) );
        code.push_back( c );
        binders.push_back( bind );
    }

    std::vector< SymStack > code;
    std::vector< Symbol > binders;
    DefBlocks defs;
};

struct Compiler
{
    Symbol symbol( parse::Identifier i ) {
        return Symbol( syms, i.name );
    }

    Symbol symbol( parse::Pattern p ) {
        return Symbol( syms, p.name() );
    }

    void compile( parse::Expression &e, SymStack &code, bool symctx = false )
    {
        e.e.match(
            [&]( const parse::Constant &x ) {
                if ( x.scope )
                    code.push_back( Value( compile( *x.scope ) ) );
                else
                    code.push_back( Value( x.value ) );
            },
            [&]( const parse::Identifier &x ) {
                code.push_back( Use( syms, x.name ) );
            },
            [&]( const parse::Lambda &x ) { compile( x, code ); },
            [&]( const parse::SubScope &x ) {
                code.push_back( Op::SubScope );
                compile( *x.lhs, code );
                if ( x.rhs->e.is< parse::Identifier >() )
                    code.push_back( Symbol( syms, x.rhs->e.get< parse::Identifier >().name ) );
                else
                    compile( *x.rhs, code );
            },
            [&]( const parse::BinOp &x ) {
                code.push_back( x.op );
                compile( *x.lhs, code );
                compile( *x.rhs, code );
            },
            [&]( const parse::Application &x ) {
                code.push_back( Op::Application );
                compile( *x.lhs, code );
                compile( *x.rhs, code );
            },
            [&]( const parse::IfThenElse &x ) {
                code.push_back( Op::If );
                compile( *x.cond, code );
                thunk( *x.yes, code );
                thunk( *x.no, code );
            }
        );
    }

    Label lambda( parse::Expression &e, Symbol bind ) {
        SymStack code;

        compile( e, code );

        Label lbl( labels );
        text.add( lbl, bind, code );
        return lbl;
    }

    Label thunk( parse::Expression &e ) {
        return lambda( e, Symbol( syms, "_dummy" ) );
    }

    void thunk( parse::Expression &e, SymStack &code ) {
        code.push_back( Value( thunk( e ).id ) );
    }

    Label compile( const parse::Lambda &l ) {
        return lambda( *l.body, symbol( l.bind ) );
    }

    void compile( const parse::Lambda &l, SymStack &code ) {
        code.push_back( Value( compile( l ).id ) );
    }

    int compile( parse::Scope &s )
    {
        DefBlock def;
        for ( auto &b: s.bindings )
            def[ symbol( b.name ).id ] = thunk( b.value ).id;
        text.defs.push_back( def );
        return text.defs.size() - 1;
    }

    Text text; /* the result */
private:
    LabelMap labels;
    SymbolMap syms;
};

struct Evaluator {
    const Text &text;
    Evaluator( const Text &text ) : text( text ) {}

    Value vpop() {
        ASSERT( !stack.empty() );
        auto v = stack.back();
        stack.pop_back();
        return v;
    }

    ConItem cpop() {
        ASSERT( !code.empty() );
        auto v = code.back();
        code.pop_back();
        return v;
    }

    void enter( Label l, Value v ) {
        auto &body = text.code[ l.id ];

        int binder = text.binders[ l.id ].id;
        scope.resize( std::max( int( scope.size() ), binder + 1 ) );
        scope[ binder ].push_back( v );

        std::transform( body.begin(), body.end(), std::back_inserter( code ),
                        [&]( SymItem x ) {
                            return x.match( [&]( Value v ) -> ConItem { return v; },
                                            [&]( Use s ) -> ConItem {
                                                ASSERT( int( scope.size() ) > s.id );
                                                ASSERT( scope[ s.id ].size() );
                                                return Value( scope[ s.id ].back() );
                                            },
                                            [&]( Symbol s ) -> ConItem { return Value( s.id ); },
                                            [&]( Op o ) -> ConItem { return o; }
                                          ).value();
                        } );
    }

    int binop( Op op, int a, int b ) {
        switch ( op ) {
            case Op::Bool_Or: return a || b;
            case Op::Bool_And: return a && b;
            case Op::LEq: return a <= b;
            case Op::Lt: return a < b;
            case Op::GEq: return a >= b;
            case Op::Gt: return a > b;
            case Op::Eq: return a == b;
            case Op::NEq: return a != b;
            case Op::Plus: return a + b;
            case Op::Minus: return a - b;
            case Op::Mult: return a * b;
            case Op::Div: return a / b;
            case Op::Mod: return a % b;
            case Op::Or: return a | b;
            case Op::And: return a & b;
            case Op::Xor: return a ^ b;
            case Op::LShift: return a << b;
            case Op::RShift: return a >> b;
        default:
            ASSERT_UNREACHABLE( "unknown binop" );                    
        }
    }

    void step() {
        auto op = cpop();

        op.match(
            [&]( Op op ) { step( op ); },
            [&]( Value v ) { stack.push_back( v ); }
        );
    }

    void step( Op op ) {
        switch ( op ) {
            case Op::Application: {
                Value fun = vpop();
                Value param = vpop();
                return enter( Label( fun.v ), param );
            }
            case Op::SubScope: {
                Value block = vpop();
                Value sym = vpop();
                ASSERT( text.defs[ block.v ].count( sym.v ) );
                /* bindings are always thunks */
                return enter( Label( text.defs[ block.v ].find( sym.v )->second ), Value() );
            }
            case Op::Bind: { /* more horrible token-id abuse */
                Value symbol = vpop();
                scope[ symbol.v ].pop_back();
                return;
            }
            case Op::If: {
                Value cond = vpop();
                Value yes = vpop(), no = vpop();
                return enter( Label( cond.v ? yes.v : no.v ), Value() ); // thunks
            }
            default: {
                Value a = vpop(), b = vpop();
                stack.push_back( Value( binop( op, a.v, b.v ) ) );
                return;
            }
        }
    }

    int reduce() {
        while ( !code.empty() )
            step();
        ASSERT_EQ( int(stack.size()), 1 );
        Value r = stack.back();
        stack.pop_back();
        return r.v;
    }

private:
    ConStack code;
    ScopeStack scope;
    std::vector< Value > stack;
};

}
}
}

namespace divine_test {
namespace silk {

using namespace divine::silk;
using namespace eval;

struct Eval
{
    Compiler comp;

    template< typename T >
    std::shared_ptr< T > _parse( std::string s ) {
        return std::make_shared< T >( Parse::_parse< T >( s ) );
    }

    int _expression( std::string s ) {
        auto top = _parse< Expression >( s );
        Label l = comp.thunk( *top );
        Evaluator eval( comp.text );
        eval.enter( l, Value() );
        return eval.reduce();
    }

    TEST(basic) {
        assert_eq( _expression( "1 + 1" ), 2 );
        assert_eq( _expression( "2 - 1" ), 1 );
        assert_eq( _expression( "2 - (1 + 1)" ), 0 );
        assert_eq( _expression( "2 * 2 + 1" ), 5 );
        assert_eq( _expression( "2 * (2 + 1)" ), 6 );
    }

    TEST(lambda) {
        assert_eq( _expression( "1 + ((|x| x + 1) 4)" ), 6 );
        assert_eq( _expression( "1 + ((|x| x - 1) 4)" ), 4 );
        assert_eq( _expression( "1 + ((|x| |y| x - y) 4 2)" ), 3 );
        assert_eq( _expression( "1 + ((|x| 1 + (|y| x - y) 2) 4)" ), 4 );
        assert_eq( _expression( "(|x| 1 + (|y| x * y) 2) 4" ), 9 );
    }

    TEST(conditional) {
        assert_eq( _expression( "if 1 then 5 else 2" ), 5 );
        assert_eq( _expression( "1 + (if 1 then 5 else 2)" ), 6 );
        assert_eq( _expression( "(|x| if x then x + 5 else x + 2) 0" ), 2 );
        assert_eq( _expression( "(|x| if x then x + 5 else x + 2) 1" ), 6 );
    }

    TEST(scope) {
        assert_eq( _expression( "{ a = 5 }.a" ), 5 );
        assert_eq( _expression( "{ a = 5 + 3 }.a" ), 8 );
        assert_eq( _expression( "{ a = |x| x + 3 }.a 5" ), 8 );
        assert_eq( _expression( "(|x| x.x + 3) { x = 1 }" ), 4 );
        assert_eq( _expression( "{ a = |x| x.x + 3 }.a { x = 1 }" ), 4 );
    }
};

}
}

#endif
