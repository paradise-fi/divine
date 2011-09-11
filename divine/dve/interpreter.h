// -*- C++ -*- (c) 2011 Petr Rockai
#include <wibble/test.h>
#include <divine/dve/parse.h>
#include <stack>

namespace divine {
namespace dve {

struct NS {
    enum Namespace { Process, Variable, Channel, State };
};

typedef int SymId;

struct SymContext : NS {
    int offset; // allocation
    int id;

    std::vector< int > constants;

    struct Item {
        int offset;
        int width;
        bool is_array:1;
        bool is_constant:1;
        int array;
        Item() : is_array( false ), is_constant( false ), array( 1 ) {}
    };

    std::vector< Item > ids;

    SymContext() : offset( 0 ), id( 0 ) {}
};

struct Symbol {
    SymContext *context;
    SymId id;

    Symbol() : context( 0 ), id( 0 ) {}
    Symbol( SymContext *ctx, SymId id ) : context( ctx ), id( id ) {}

    bool valid() { return context; }

    bool operator==( const Symbol &o ) const {
        return id == o.id && context == o.context;
    }

    bool operator!=( const Symbol &o ) const {
        return id != o.id || context != o.context;
    }

    SymContext::Item &item() {
        assert( context );
        assert_leq( size_t( id + 1 ), context->ids.size() );
        return context->ids[ id ];
    }

    template< typename T >
    T deref( char *mem = 0, int idx = 0 ) {
        if ( item().is_constant )
            return context->constants[ item().offset + idx ];
        else {
            int value = *reinterpret_cast< int * >( mem + item().offset + idx * item().width );
            return value;
        }
    }

    template< typename T >
    void set( char *mem, int idx, T value ) {
        assert ( !item().is_constant );
        *reinterpret_cast< T * >( mem + item().offset + idx * item().width ) = value;
    }

};

std::ostream &operator<<( std::ostream &os, Symbol s )
{
    return os << s.id;
}

struct SymTab : NS {
    typedef std::map< std::string, SymId > Tab;
    std::vector< Tab > tabs;
    std::set< SymId > scope;

    SymContext *context;
    SymTab *parent; // scoping

    SymId newid( Namespace ns, std::string name ) {
        assert( context );
        assert( !tabs[ ns ].count( name ) );

        unsigned id = context->id ++;
        context->ids.resize( id + 1 );
        tabs[ ns ][ name ] = id;
        return id;
    }

    Symbol lookup( Namespace ns, std::string id ) const {
        if ( tabs[ ns ].count( id ) )
            return Symbol( context, tabs[ ns ].find( id )->second );
        if ( parent )
            return parent->lookup( ns, id );
        return Symbol();
    }

    Symbol lookup( Namespace ns, const parse::Identifier id ) const {
        return lookup( ns, id.name() );
    }

    Symbol allocate( Namespace ns, std::string name, int width )
    {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        i.offset = context->offset;
        i.width = width;
        context->offset += width;
        return Symbol( context, id );
    }

    Symbol allocate( Namespace ns, const parse::Declaration &decl ) {
        int width = 4; // XXX
        Symbol s = allocate( ns, decl.name, width * (decl.is_array ? decl.size : 1) );
        s.item().is_array = decl.is_array;
        s.item().width = width;
        s.item().array = decl.size;
        return s;
    }

    Symbol constant( Namespace ns, std::string name, int value ) {
        unsigned id = newid( ns, name );
        SymContext::Item &i = context->ids[ id ];
        std::vector< int > &c = context->constants;
        i.offset = c.size();
        i.is_constant = true;
        c.resize( c.size() + 1 );
        c[ i.offset ] = value;
        return Symbol( context, id );
    }

    SymTab( SymTab *parent = 0 ) : parent( parent ) {
        if ( !parent )
            context = new SymContext();
        else
            context = parent->context;
        tabs.resize( 4 ); // one for each namespace
    }

    std::ostream& dump(std::ostream &o, char *mem) {
        for ( Namespace ns = Process; ns < State; ns = NS::Namespace( ns + 1 ) ) {
            for ( std::map< std::string, int >::iterator i = tabs[ ns ].begin();
                  i != tabs[ ns ].end(); ++i ) {
                Symbol s( context, i->second );
                o << i->first << " = " << s.deref< short >( mem ) << ", ";
            }
        }
        return o;
    }
};

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
        stack.pop_back();
        return v;
    }

    void push( Value a ) {
        stack.push_back( a );
    }

    std::vector< Value > stack;
    char *mem;
};

struct Expression {
    struct Item {
        // Identifier = symbol reference; arg = symbol id
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
        Symbol s;
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
    }

    int evaluate( EvalContext &ctx ) {
        assert( ctx.stack.empty() );
        for ( std::vector< Item >::iterator i = rpn.begin(); i != rpn.end(); ++i )
            step( ctx, *i );
        assert_eq( ctx.stack.size(), (size_t) 1 );
        return ctx.pop().value;
    }

    void rpn_push( Token op, EvalContext::Value v ) {
        rpn.push_back( Item( op, v ) );
    }

    void build( const SymTab &sym, const parse::Expression &ex ) {
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

namespace parse {
typedef std::vector< Process > Procs;
typedef std::vector< Declaration > Decls;
}

struct LValue {
    Symbol symbol;
    Expression idx;
    bool _valid;

    bool valid() { return _valid; }

    template< typename T >
    void set( EvalContext &ctx, T value ) {
        if ( symbol.item().is_array )
            return symbol.set( ctx.mem, idx.evaluate( ctx ), value );
        return symbol.set( ctx.mem, 0, value );
    }

    LValue( const SymTab &tab, parse::LValue lv )
        : idx( tab, lv.idx ), _valid( lv.valid() )
    {
        symbol = tab.lookup( NS::Variable, lv.ident.name() );
    }

    LValue() : _valid( false ) {}
};

struct Transition {
    Symbol process;
    Symbol from, to;
    Symbol sync_channel;
    LValue sync_lval;
    Expression sync_expr;
    Transition *sync;

    std::vector< Expression > guards;
    typedef std::vector< std::pair< LValue, Expression > > Effect;
    Effect effect;

    bool enabled( EvalContext &ctx ) {
        if ( process.deref< short >( ctx.mem ) != from.deref< short >( 0 ) )
            return false;
        for ( int i = 0; i < guards.size(); ++i )
            if ( !guards[i].evaluate( ctx ) )
                return false;
        if ( sync && !sync->enabled( ctx ) )
            return false;
        return true;
    }

    void apply( EvalContext &ctx ) {
        for ( Effect::iterator i = effect.begin(); i != effect.end(); ++i )
            i->first.set( ctx, i->second.evaluate( ctx ) );
        process.set( ctx.mem, 0, to.deref< short >() );
        if ( sync ) {
            if (sync->sync_lval.valid() && sync_expr.valid() )
                sync->sync_lval.set( ctx, sync_expr.evaluate( ctx ) );
            sync->apply( ctx );
        }
    }

    Transition( SymTab &sym, Symbol proc, parse::Transition t )
        : process( proc ), sync( 0 )
    {
        for ( int i = 0; i < t.guards.size(); ++ i )
            guards.push_back( Expression( sym, t.guards[i] ) );
        for ( int i = 0; i < t.effects.size(); ++ i )
            effect.push_back( std::make_pair( LValue( sym, t.effects[i].lhs ),
                                              Expression( sym, t.effects[i].rhs ) ) );
        from = sym.lookup( NS::State, t.from );
        assert( from.valid() );
        to = sym.lookup( NS::State, t.to );
        assert( to.valid() );

        if ( t.syncexpr.valid() ) {
            sync_channel = sym.lookup( NS::Channel, t.syncexpr.chan );
            if ( t.syncexpr.write )
                sync_expr = Expression( sym, t.syncexpr.expr );
            else
                sync_lval = LValue( sym, t.syncexpr.lval );
        }
    }
};

struct Process {
    Symbol id;
    SymTab symtab;

    std::vector< std::vector< Transition > > trans;

    std::vector< Transition > readers;
    std::vector< Transition > writers;

    template< typename I >
    I enabled( EvalContext &ctx, I i ) {
        int state = id.deref< short >( ctx.mem );
        assert_leq( size_t( state + 1 ), trans.size() );
        std::vector< Transition > &tr = trans[ state ];
        for ( size_t j = 0; j < tr.size(); ++j ) {
            if ( tr[ j ].enabled( ctx ) )
                *i++ = tr[ j ];
        }
        return i;
    }

    int enabled( EvalContext &ctx, int i ) {
        int state = id.deref< short >( ctx.mem );
        assert_leq( size_t( state + 1 ), trans.size() );
        std::vector< Transition > &tr = trans[ state ];
        for ( ; i < tr.size(); ++i ) {
            if ( tr[ i ].enabled( ctx ) )
                return i + 1;
        }
        // std::cerr << "no further enabled transitions" << std::endl;
        return 0;
    }

    Transition &transition( EvalContext &ctx, int i ) {
        int state = id.deref< short >( ctx.mem );
        assert_leq( size_t( state + 1 ), trans.size() );
        assert_leq( i, trans[ state ].size() );
        assert_leq( 1, i );
        return trans[ state ][ i - 1 ];
    }

    Process( SymTab *parent, Symbol id, const parse::Process &proc )
        : id( id ), symtab( parent )
    {
        int states = 0;
        assert( id.valid() );
        for ( std::vector< parse::Identifier >::const_iterator i = proc.states.begin();
              i != proc.states.end(); ++i ) {
            symtab.constant( NS::State, i->name(), states++ );
        }

        assert_eq( states, proc.states.size() );
        trans.resize( proc.states.size() );


        for ( std::vector< parse::Transition >::const_iterator i = proc.trans.begin();
              i != proc.trans.end(); ++i ) {
            Symbol from = symtab.lookup( NS::State, i->from.name() );
            Transition t( symtab, id, *i );
            if ( i->syncexpr.valid() ) {
                if ( i->syncexpr.write )
                    writers.push_back( t );
                else
                    readers.push_back( t );
            } else
                trans[ from.deref< short >() ].push_back( t );
        }
    }

    void setupSyncs( std::vector< Transition > &readers )
    {
        for ( int w = 0; w < writers.size(); ++ w ) {
            Transition &tw = writers[w];
            for ( int r = 0; r < readers.size(); ++r ) {
                Transition &tr = readers[r];
                if ( tw.sync_channel == tr.sync_channel ) {
                    if ( tw.sync_expr.valid() != tr.sync_lval.valid() )
                        throw "Booh";
                    tw.sync = &tr;
                    trans[ tw.from.deref< short >() ].push_back( tw );
                }
            }
        }
    }
};

void declare( SymTab &symtab, const parse::Decls &decls )
{
    for ( parse::Decls::const_iterator i = decls.begin(); i != decls.end(); ++i )
        symtab.allocate( i->is_chan ? NS::Channel : NS::Variable, *i );
}

struct System {
    SymTab symtab;
    std::vector< Process > processes;

    System( const parse::System &sys ) {
        declare( symtab, sys.decls );
        for ( parse::Procs::const_iterator i = sys.processes.begin();
              i != sys.processes.end(); ++i ) {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            processes.push_back( Process( &symtab, id, *i ) );
        }

        // compute synchronisations
        for ( int i = 0; i < processes.size(); ++ i ) {
            for ( int j = 0; j < processes.size(); ++ j ) {
                if ( i == j )
                    continue;
                processes[ i ].setupSyncs( processes[ j ].readers );
            }
        }
    }

    template< typename I >
    I enabled( EvalContext &ctx, I i ) {
        for ( size_t j = 0; j < processes.size(); ++j )
            i = processes[ j ].enabled( ctx, i );
        return i;
    }

    std::pair< int, int > enabled( EvalContext &ctx, std::pair< int, int > cont ) {
        for ( ; cont.first < processes.size(); ++cont.first ) {
            cont.second = processes[ cont.first ].enabled( ctx, cont.second );
            if ( cont.second )
                return cont;
        }
        return cont; // make_pair( processes.size(), 0 );
    }

    Transition &transition( EvalContext &ctx, std::pair< int, int > p ) {
        assert_leq( p.first + 1, processes.size() );
        return processes[ p.first ].transition( ctx, p.second );
    }

    bool invalid( std::pair< int, int > p ) {
        return p.first >= processes.size();
    }
};

}
}
