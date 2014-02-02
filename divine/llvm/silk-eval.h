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

using SymItem = wibble::Union< Value, Symbol, Op >;
using ConItem = wibble::Union< Value, Op >;
using SymStack = std::vector< SymItem >;
using ConStack = std::vector< ConItem >;

/* map an identifier to a value; each lambda binds exactly one value, hence we
 * always know where to pop when leaving a lambda; indexed by symbol IDs */
using ScopeStack = std::vector< std::vector< Value > >;

struct Text {
    Label add( LabelMap &m, Symbol bind, const SymStack &c ) {
        Label l( m );
        assert_eq( l.id, code.size() );
        assert_eq( l.id, binders.size() );
        code.push_back( c );
        binders.push_back( bind );
        return l;
    }

    std::vector< SymStack > code;
    std::vector< Symbol > binders;
};

struct Compiler
{
    void compile( const parse::BinOp &e )
    {
    }

    static parse::Lambda thunk( parse::ExpressionPtr e ) {
        return parse::Lambda( parse::Identifier( "_dummy" ), e );
    }

    Symbol symbol( parse::Identifier i ) {
        return Symbol( syms, i.name );
    }

    void compile( parse::Expression &e, SymStack &code )
    {
        e.e.match(
            [&]( const parse::Constant &x ) {
                code.push_back( Value( x.value ) );
            },
            [&]( const parse::Identifier &x ) {
                code.push_back( Symbol( syms, x.name ) );
            },
            [&]( const parse::Lambda &x ) { compile( x, code ); },
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
                compile( thunk( x.yes ), code );
                compile( thunk( x.no ), code );
                compile( *x.cond, code );
                code.push_back( Op::If );
            }
        );
    }

    Label compile( const parse::Lambda &l, SymStack &code )
    {
        SymStack c2;

        compile( *l.body, c2 );

        Label lbl = text.add( labels, symbol( l.bind ), c2 );
        code.push_back( Value( lbl.id ) );
        return lbl;
    }

    Label compile( const parse::Lambda &l )
    {
        SymStack s;
        return compile( l, s );
    }

    void compile( const parse::Scope &toplevel )
    {
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
        assert( !stack.empty() );
        auto v = stack.back();
        stack.pop_back();
        return v;
    }

    ConItem cpop() {
        assert( !code.empty() );
        auto v = code.back();
        code.pop_back();
        return v;
    }

    void enter( Label l, Value v ) {
        auto &body = text.code[ l.id ];

        int binder = text.binders[ l.id ].id;
        scope.resize( std::max( int( scope.size() ), binder + 1 ) );
        scope[ text.binders[ l.id ].id ].push_back( v );

        std::transform( body.begin(), body.end(), std::back_inserter( code ),
                        [&]( SymItem x ) {
                            return x.match( [&]( Value v ) -> ConItem { return v; },
                                            [&]( Symbol s ) -> ConItem { return Value( scope[ s.id ].back() ); },
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
        }
        assert_unreachable( "unknown binop" );
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
        assert_eq( stack.size(), 1 );
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

#endif
