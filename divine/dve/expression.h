// -*- C++ -*- (c) 2011 Petr Rockai
//             (c) 2012, 2013 Jan Kriho

#include <vector>
#include <unordered_set>
#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <divine/dve/flag.h>

#ifndef DIVINE_DVE_EXPRESSION_H
#define DIVINE_DVE_EXPRESSION_H

#define DEBUG(x) do { /*x */ } while(false)

namespace divine {
namespace dve {

struct EvalContext {
    struct ImmValue {
        int value:24;
        uint8_t error:8;
        ImmValue ( int v ) : value( v ), error( 0 ) {}
        ImmValue ( ErrorState state ) : value( 0 ), error( state.error ) {}
        ImmValue () : value( 0 ), error( 0 ) {}

        inline ErrorState errState() {
            return ErrorState( error );
        }
    };

    struct Value {
        union {
            Symbol symbol;
            ImmValue ival;
        };
        Value ( Symbol s ) : symbol ( s ) {}
        Value ( ImmValue iv ) : ival( iv ) {}
        Value ( int v  ) : ival( v ) {}
        Value ( ErrorState state ) : ival( state ) {}
        Value () : ival() {}
    };

    ImmValue pop() {
        ASSERT( !stack.empty() );
        ImmValue v = stack.back();
        DEBUG(std::cerr << "-[" << v.value << "]");
        stack.pop_back();
        return v;
    }

    void push( ImmValue a ) {
        DEBUG(std::cerr << "+[" << a.value << "]");
        stack.push_back( a );
    }

    std::vector< ImmValue > stack;
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
        // id of the array (placed on static stack), while the other is the array subscript
        Token op;
        EvalContext::Value arg1, arg2;

        Item( Token op, EvalContext::Value arg1, EvalContext::Value arg2 = 0 ) : op( op ), arg1( arg1 ), arg2( arg2 ) {}
    };

    std::vector< Item > rpn;
    bool _valid;
    bool valid() { return _valid; }
    
    inline EvalContext::ImmValue binop( const Token &op, EvalContext::ImmValue a, EvalContext::ImmValue b ) {
        if ( a.error )
            return EvalContext::ImmValue(a.error);
        
        switch (op.id) {
            case TI::Bool_Or:
                if ( a.value )
                    return true;
                if ( b.error )
                    return EvalContext::ImmValue(b.errState());
                return b.value;
            case TI::Bool_And:
                if ( !a.value )
                    return false;
                if ( b.error )
                    return EvalContext::ImmValue(b.errState());
                return b.value;
            case TI::Imply:
                if ( !a.value )
                    return true;
                if ( b.error )
                    return EvalContext::ImmValue(b.errState());
                return b.value;
            default:
                if ( b.error )
                    return EvalContext::ImmValue(b.errState());
                if ( op.id == TI::Div || op.id == TI::Mod )
                    if ( b.value == 0 )
                        return EvalContext::ImmValue( ErrorState::e_divByZero );
                int result = binop( op, a.value, b.value );
                EvalContext::ImmValue retval = result;
                if (retval.value != result)
                    return ErrorState::e_overflow;
                return retval;
        }
    }

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
                ASSERT_UNREACHABLE_F( "unknown operator %s", brick::string::fmt( op ).c_str() );
        }
    }

    void step( EvalContext &ctx, Item &i ) {
        EvalContext::ImmValue a, b;
        DEBUG(for ( int x = 0; x < ctx.stack.size(); ++ x )
                  std::cerr << ctx.stack[ x ].value << " ";
              std::cerr << i.op);

        switch (i.op.id) {
            case TI::Identifier:
                ctx.push( i.arg1.symbol.deref( ctx.mem ) ); break;
            case TI::Reference:
                ASSERT_UNREACHABLE( "unexpected reference" ); // obsolete code path
                break;
            case TI::Constant:
                ctx.push( i.arg1.ival ); break;
            case TI::Subscript:
                b = ctx.pop();
                if ( b.error )
                    ctx.push( b.error );
                else if ( b.value < 0 || (i.arg1.symbol.item().is_array && i.arg1.symbol.item().array <= b.value ) )
                    ctx.push( ErrorState::e_arrayCheck );
                else
                    ctx.push( i.arg1.symbol.deref( ctx.mem, b.value ) );
                break;
            case TI::Period:
                ctx.push( i.arg1.symbol.deref( ctx.mem ) == i.arg2.symbol.deref( ctx.mem ) );
                break;
            case TI::Bool_Not:
                a = ctx.pop();
                ctx.push( !a.value );
                break;
            case TI::Tilde:
                a = ctx.pop();
                ctx.push( ~a.value );
                break;
            default:
                b = ctx.pop();
                a = ctx.pop();
                ctx.push( binop( i.op, a, b ) );
        }
        DEBUG(std::cerr << std::endl);
    }

    int evaluate( EvalContext &ctx, ErrorState * err = 0 ) {
        ASSERT( ctx.stack.empty() );
        for ( std::vector< Item >::iterator i = rpn.begin(); i != rpn.end(); ++i )
            step( ctx, *i );
        ASSERT_EQ( ctx.stack.size(), static_cast<size_t>( 1) );
        DEBUG(std::cerr << "done: " << ctx.stack.back().value << std::endl);
        EvalContext::ImmValue retval = ctx.pop();
        if ( retval.error ) {
            ASSERT( !retval.errState().padding() );
            if ( err )
                err->error |= retval.error;
            return true; // Black magic - We want to have transition to the error state
        }
        return retval.value;
    }

    void rpn_push( Token op, EvalContext::Value v1, EvalContext::Value v2 = 0 ) {
        rpn.push_back( Item( op, v1, v2 ) );
    }

    void build( const SymTab &sym, const parse::Expression &ex, const SymTab * immscope = 0 ) {
        if ( ex.op.id == TI::Period ) {
            ASSERT( ex.lhs );
            ASSERT( ex.rhs );
            auto left = ex.lhs->rval, right = ex.rhs->rval;
            ASSERT( left );
            ASSERT( right );
            Symbol process = sym.lookup( SymTab::Process, left->ident.name() );
            if ( !process.valid() )
                left->fail( ( "Couldn't find process: " + left->ident.name() ).c_str(),
                            parse::System::Fail::Semantic );
            Symbol state = sym.toplevel()->child( process )->lookup(
                SymTab::State, right->ident.name() );
            if ( !state.valid() )
                left->fail( ( "Couldn't find state " + right->ident.name() +
                              " in process " + left->ident.name() +
                              " Did you mean " + left->ident.name() +
                              "->" + right->ident.name() + "?"
                            ).c_str(),
                            parse::System::Fail::Semantic );
            rpn_push( ex.op, process, state );
            return;
        }

        if ( ex.op.id == TI::Arrow ) {
            ASSERT( ex.lhs );
            ASSERT( ex.rhs );
            auto left = ex.lhs->rval;
            Symbol process = sym.lookup( SymTab::Process, left->ident );
            if ( !process.valid() )
                left->fail( ( "Couldn't find process: " + left->ident.name() ).c_str(),
                            parse::System::Fail::Semantic );
            const SymTab * procVars = sym.toplevel()->child( process );
            build( sym, *ex.rhs, procVars );
            return;
        }

        if ( ex.rval ) {
            parse::RValue &v = *ex.rval;
            if ( v.ident.valid() ) {
                if ( !immscope )
                    immscope = &sym;
                Symbol ident = immscope->lookup( SymTab::Variable, v.ident.name() );
                if ( !ident.valid() ) {
                    v.fail( ( "Unresolved identifier: " + v.ident.name() ).c_str(), parse::System::Fail::Semantic );
                }
                Token t = v.ident.token;
                if ( v.idx ) {
                    t.id = TI::Subscript;
                    build( sym, *v.idx );
                }
                rpn_push( t, ident );
            } else
                rpn_push( v.value.token, v.value.value );
        }

        if ( ex.lhs ) {
            ASSERT( !ex.rval );
            build( sym, *ex.lhs );
        }

        if ( ex.rhs ) {
            ASSERT( ex.lhs );
            build( sym, *ex.rhs );
        }

        if ( !ex.rval )
            rpn_push( ex.op, 0 );
    }

    std::unordered_set< SymId > getSymbols() {
        std::unordered_set< SymId > symbols;
        for ( Item it : rpn ) {
            switch( it.op.id ) {
                case TI::Period:
                case TI::Identifier:
                case TI::Subscript:
                    symbols.insert( it.arg1.symbol.id );
                    break;
                default:
                    break;
            }
        }
        return symbols;
    }

    Expression( const SymTab &sym, const parse::Expression &ex )
        : _valid( ex.valid() )
    {
        build( sym, ex );
    }

    Expression() : _valid( false ) {}
};

struct ExpressionList {
    std::vector< Expression > exprs;
    bool _valid;
    
    bool valid() { return _valid; }
    
    std::vector< int > evaluate( EvalContext &ctx, ErrorState * err = 0 ) {
        std::vector< int > retval;
        retval.resize( exprs.size() );
        auto retit = retval.begin();
        for( auto it = exprs.begin(); it != exprs.end(); it++, retit++ ) {
            ASSERT( retit != retval.end() );
            *retit = (*it).evaluate( ctx, err );
        }
        return retval;
    }

    std::unordered_set< SymId > getSymbols() {
        std::unordered_set< SymId > symbols;
        for ( Expression e : exprs ) {
            auto syms = e.getSymbols();
            symbols.insert( syms.begin(), syms.end() );
        }
        return symbols;
    }

    ExpressionList( const SymTab &tab, parse::ExpressionList explist )
        : _valid( explist.valid() )
    {
        for( auto expr = explist.explist.begin(); expr != explist.explist.end(); expr++ ) {
            exprs.push_back( Expression(tab, *expr) );
        }
    }
    
    ExpressionList() : _valid( false ) {}
};

}
}

#endif
