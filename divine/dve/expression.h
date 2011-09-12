// -*- C++ -*- (c) 2011 Petr Rockai

#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <vector>

#ifndef DIVINE_DVE_EXPRESSION_H
#define DIVINE_DVE_EXPRESSION_H

#define DEBUG(x) do { /*x */ } while(false)

namespace divine {
namespace dve {

struct EvalContext {
    struct Value {
        union {
            Symbol symbol;
            int value;
        };
        Value ( Symbol s ) : symbol ( s ) {}
        Value ( int v  ) : value( v ) {}
    };

    Value pop() {
        assert( !stack.empty() );
        Value v = stack.back();
        DEBUG(std::cerr << "-[" << v.value << "]");
        stack.pop_back();
        return v;
    }

    void push( Value a ) {
        DEBUG(std::cerr << "+[" << a.value << "]");
        stack.push_back( a );
    }

    std::vector< Value > stack;
    char *mem;
};

struct Expression {
    struct Item {
        // Identifier = symbol reference; arg = symbol id
        // Reference = same, but push the *symbol* on the eval stack
        // Constant = in-place constant; arg = the constant

        // operator id = operator; arg is unused, operands are the top two
        // elements currently on the stack

        // Subscript = array index operation; the first operand is the symbol
        // id of the array, while the other is the array subscript
        Token op;
        EvalContext::Value arg;

        Item( Token op, EvalContext::Value arg ) : op( op ), arg( arg ) {}
    };

    std::vector< Item > rpn;
    bool _valid;
    bool valid() { return _valid; }

    inline int binop( const Token &op, int a, int b ) {
        switch ( op.id ) {
            case TI::Bool_Or: return a || b;
            case TI::Bool_And: return a && b;
            case TI::Imply: return (!a) || b;
            case TI::LEq: return a <= b;
            case TI::Lt: return a < b;
            case TI::GEq: return a >= b;
            case TI::Gt: return a > b;
            case TI::Eq: return a == b;
            case TI::NEq: return a != b;
            case TI::Plus: return a + b;
            case TI::Minus: return a - b;
            case TI::Mult: return a * b;
            case TI::Div: return a / b;
            case TI::Mod: return a % b;
            case TI::Or: return a | b;
            case TI::And: return a & b;
            case TI::Xor: return a ^ b;
            case TI::LShift: return a << b;
            case TI::RShift: return a >> b;
            default:
                std::cerr << "unknown operator: " << op << std::endl;
                assert_die(); // something went seriously wrong...
        }
    }

    void step( EvalContext &ctx, Item &i ) {
        int a, b;
        Symbol p, s;
        DEBUG(for ( int i = 0; i < ctx.stack.size(); ++ i )
                  std::cerr << ctx.stack[ i ].value << " ";
              std::cerr << i.op);

        switch (i.op.id) {
            case TI::Identifier:
                ctx.push( i.arg.symbol.deref< int >( ctx.mem ) ); break;
            case TI::Reference:
            case TI::Constant:
                ctx.push( i.arg ); break;
            case TI::Subscript:
                b = ctx.pop().value;
                s = ctx.pop().symbol;
                ctx.push( s.deref< int >( ctx.mem, b ) );
                break;
            case TI::Period:
                s = ctx.pop().symbol;
                p = ctx.pop().symbol;
                ctx.push( s.deref< short >( ctx.mem ) == p.deref< short >( ctx.mem ) );
                break;
            case TI::Bool_Not:
                a = ctx.pop().value;
                ctx.push( !a );
                break;
            case TI::Tilde:
                a = ctx.pop().value;
                ctx.push( ~a );
                break;
            default:
                b = ctx.pop().value;
                a = ctx.pop().value;
                ctx.push( binop( i.op, a, b ) );
        }
        DEBUG(std::cerr << std::endl);
    }

    int evaluate( EvalContext &ctx ) {
        assert( ctx.stack.empty() );
        for ( std::vector< Item >::iterator i = rpn.begin(); i != rpn.end(); ++i )
            step( ctx, *i );
        assert_eq( ctx.stack.size(), (size_t) 1 );
        DEBUG(std::cerr << "done: " << ctx.stack.back().value << std::endl);
        return ctx.pop().value;
    }

    void rpn_push( Token op, EvalContext::Value v ) {
        rpn.push_back( Item( op, v ) );
    }

    void build( const SymTab &sym, const parse::Expression &ex ) {
        if ( ex.op.id == TI::Period ) {
            assert( ex.lhs );
            assert( ex.rhs );
            parse::RValue *left = ex.lhs->rval, *right = ex.rhs->rval;
            assert( left );
            assert( right );
            Symbol process = sym.lookup( SymTab::Process, left->ident.name() );
            Symbol state = sym.toplevel()->child( process )->lookup(
                SymTab::State, right->ident.name() );
            assert( process.valid() );
            assert( state.valid() );
            rpn_push( Token( TI::Reference, "<synthetic>" ), process );
            rpn_push( Token( TI::Reference, "<synthetic>" ), state );
            rpn_push( ex.op, 0 );
            return;
        }

        if ( ex.rval ) {
            parse::RValue &v = *ex.rval;
            if ( v.ident.valid() ) {
                Symbol ident = sym.lookup( SymTab::Variable, v.ident.name() );
                Token t = v.ident.token;
                if ( v.idx )
                    t.id = TI::Reference;
                rpn_push( t, ident );
                if ( v.idx ) {
                    build( sym, *v.idx );
                    rpn_push( Token( TI::Subscript, "<synthetic>" ), 0 );
                }
            } else
                rpn_push( v.value.token, v.value.value );
        }

        if ( ex.lhs ) {
            assert( !ex.rval );
            build( sym, *ex.lhs );
        }

        if ( ex.rhs ) {
            assert( ex.lhs );
            build( sym, *ex.rhs );
        }

        if ( !ex.rval )
            rpn_push( ex.op, 0 );
    }

    Expression( const SymTab &sym, const parse::Expression &ex )
        : _valid( ex.valid() )
    {
        build( sym, ex );
    }

    Expression() : _valid( false ) {}
};

}
}

#endif
