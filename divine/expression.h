// -*- C++ -*-

#include <iostream>
#include <vector>
#include <cassert>

enum Op {
    SetOp, // set location c to value a (ByteCode only)
    ConstOp, // constant operand (Expression only)
    AddOp, SubOp, DivOp, MulOp, // arithmetic; a, b are operands,
                                // c is output location
    AndOp, OrOp, // boolean operators, a/b are operands, c is
                 // output location
    GtOp, // greater-than
    CondOp, // conditional forward jump; a is boolean, b is
            // number of instructions to move forward if a is true, c is
            // number of instructions to move forward if a is false
    ReturnOp // stops evaluation of a block and gives the value it
             // evaluated to (ByteCode only)
};

struct Instr {
    Op op;
    int a, b, c;
    Instr( Op _op, int _a, int _b, int _c )
        : op( _op ), a( _a ), b( _b ), c( _c ) {}
};

Instr insn( Op op, int a, int b, int c )
{
    return Instr( op, a, b, c );
}

struct Expression {
    Op op;
    int value; // for ConstOp
    Expression *left, *right;
};

struct ByteCode {
    void append( Op op, int a, int b, int c ) {
        code.push_back( insn( op, a, b, c ) );
    }
    std::vector< Instr > code;
    int locations;
    ByteCode() : locations( 0 ) {}
};

struct Evaluator {

    typedef std::vector< int > Memory;

    Memory m_locations;

    int &location( int idx ) {
        return m_locations[ idx ];
    }

    std::string opName( Op op )
    {
        switch (op) {
        case SetOp: return "set";
        case AddOp: return "add";
        case SubOp: return "sub";
        case CondOp: return "cond";
        case GtOp: return "gt";
        case AndOp: return "and";
        case ReturnOp: return "return";
        default: return "foo";
        }
    }

    ByteCode compile( Expression &e ) {
        ByteCode ret;
        ret.locations = 1;
        _compile( e, ret, 0 );
        ret.append( ReturnOp, 0, 0, 0 );
        return ret;
    }

    void _compile( Expression &e, ByteCode &b, int out = 0 )
    {
        if ( e.op == AndOp ) {
            int my = b.locations;
            ++ b.locations;

            b.append( SetOp, 0, 0, out );
            _compile( *e.left, b, my );

            b.append( CondOp, my, 1, 0 );
            int idx = b.code.size();
            _compile( *e.right, b, out );
            b.code[ idx - 1 ].c = b.code.size() - idx + 1;
        } else if ( e.op == OrOp ) {
            int my = b.locations;
            ++ b.locations;

            b.append( SetOp, 1, 0, out );
            _compile( *e.left, b, my );

            b.append( CondOp, my, 0, 1 );
            int idx = b.code.size();
            _compile( *e.right, b, out );
            b.code[ idx - 1 ].b = b.code.size() - idx + 1;
        } else if ( e.op == ConstOp ) {
            b.append( SetOp, e.value, 0, out );
            return;
        } else {
            int my = b.locations;
            b.locations += 2;
            _compile( *e.left, b, my );
            _compile( *e.right, b, my + 1 );
            b.append( e.op, my, my + 1, out );
        }
    }

    void dumpCompiled( Expression &e, std::ostream &o )
    {
        ByteCode exp = compile( e );
        dump( exp, o );
    }

    void dump( Expression &e, std::ostream &o );

    void dump( Instr &i, std::ostream &o ) {
        std::cerr << "op = " << opName( i.op ) << " (" << i.op << ")"
                  << ", a = [" << i.a << "] " // << location( i.a )
                  << ", b = [" << i.b << "] " // << location( i.b )
                  << ", c = [" << i.c << "] " // << location( i.c )
                  << std::endl;
    }

    void dump( ByteCode &e, std::ostream &o ) {
        for ( int pc = 0; pc < e.code.size(); ++ pc ) {
            dump( e.code[ pc ], o );
        }
    }

    int evaluate( const ByteCode &e )
    {
        int pc = 0;

        if ( e.locations >= m_locations.size() )
            m_locations.resize( e.locations + 1 );

        while( true )
        {
            assert( pc <= e.code.size() );
            const Instr &i = e.code[pc];

            // control flow
            if ( i.op == CondOp ) {
                if ( location( i.a ) )
                    pc += i.b;
                else
                    pc += i.c;
                continue;
            } else if ( i.op == ReturnOp ) {
                return location( i.a );
            }

            // arithmetics
            switch ( i.op ) {
            case AddOp:
                location( i.c ) = location( i.a ) + location( i.b ); break;
            case SubOp:
                location( i.c ) = location( i.a ) - location( i.b ); break;
            case DivOp:
                location( i.c ) = location( i.a ) / location( i.b ); break;
            case MulOp:
                location( i.c ) = location( i.a ) * location( i.b ); break;
            case AndOp:
                location( i.c ) = location( i.a ) && location( i.b ); break;
             case OrOp:
                 location( i.c ) = location( i.a ) || location( i.b ); break;
             case GtOp:
                 location( i.c ) = location( i.a ) > location( i.b ); break;
            case SetOp:
                location( i.c ) = i.a; break;
            }

            ++ pc;
        }
    }

};
