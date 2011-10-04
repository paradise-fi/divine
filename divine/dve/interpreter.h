// -*- C++ -*- (c) 2011 Petr Rockai
#include <wibble/test.h>

#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <divine/dve/expression.h>

#ifndef DIVINE_DVE_INTERPRETER_H
#define DIVINE_DVE_INTERPRETER_H

namespace divine {
namespace dve {

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
        assert( symbol.valid() );
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
        if ( process.deref( ctx.mem ) != from.deref( 0 ) )
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
        process.set( ctx.mem, 0, to.deref() );
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
            else if ( t.syncexpr.lval.valid() )
                sync_lval = LValue( sym, t.syncexpr.lval );
        }
    }
};

static inline void declare( SymTab &symtab, const parse::Decls &decls )
{
    for ( parse::Decls::const_iterator i = decls.begin(); i != decls.end(); ++i ) {
        symtab.allocate( i->is_chan ? NS::Channel : NS::Variable, *i );
        std::vector< int > init;
        EvalContext ctx;
        for ( int j = 0; j < i->initial.size(); ++j ) {
            Expression ex( symtab, i->initial[ j ] );
            init.push_back( ex.evaluate( ctx ) );
        }
        while ( init.size() < i->size )
            init.push_back( 0 );
        symtab.constant( NS::Initialiser, i->name, init );
    }
}

struct Process {
    Symbol id;
    SymTab symtab;

    std::vector< std::vector< Transition > > trans;

    std::vector< Transition > readers;
    std::vector< Transition > writers;

    int state( EvalContext &ctx ) {
        return id.deref( ctx.mem );
    }

    int enabled( EvalContext &ctx, int i ) {
        assert_leq( size_t( state( ctx ) + 1 ), trans.size() );
        std::vector< Transition > &tr = trans[ state( ctx ) ];
        for ( ; i < tr.size(); ++i ) {
            if ( tr[ i ].enabled( ctx ) )
                break;
        }
        return i + 1;
    }

    bool valid( EvalContext &ctx, int i ) {
        return i <= trans[ state( ctx ) ].size();
    }

    Transition &transition( EvalContext &ctx, int i ) {
        assert_leq( size_t( state( ctx ) + 1 ), trans.size() );
        assert_leq( i, trans[ state( ctx ) ].size() );
        assert_leq( 1, i );
        return trans[ state( ctx ) ][ i - 1 ];
    }

    Process( SymTab *parent, Symbol id, const parse::Process &proc )
        : id( id ), symtab( parent )
    {
        int states = 0;
        assert( id.valid() );

        declare( symtab, proc.decls );
        for ( std::vector< parse::Identifier >::const_iterator i = proc.states.begin();
              i != proc.states.end(); ++i ) {
            if ( proc.inits.size() && i->name() == proc.inits.front().name() )
                parent->constant( NS::InitState, proc.name.name(), states );
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
                trans[ from.deref() ].push_back( t );
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
                    trans[ tw.from.deref() ].push_back( tw );
                }
            }
        }
    }
};

struct System {
    SymTab symtab;
    std::vector< Process > processes;
    Process *property;

    std::vector< bool > is_accepting;

    struct Continuation {
        unsigned process:16; // the process whose turn it is
        unsigned property:16; // active property transition; 0 = none
        unsigned transition:32; // active process transition; 0 = none
        Continuation() : process( 0 ), property( 0 ), transition( 0 ) {}
        bool operator==( const Continuation &o ) const {
            return process == o.process && property == o.property && transition == o.transition;
        }
    };

    System( const parse::System &sys )
        : property( 0 )
    {
        assert( !sys.synchronous ); // XXX

        declare( symtab, sys.decls );

        // ensure validity of pointers into the process array
        processes.reserve( sys.processes.size() );

        for ( parse::Procs::const_iterator i = sys.processes.begin();
              i != sys.processes.end(); ++i )
        {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            processes.push_back( Process( &symtab, id, *i ) );
            symtab.children[id] = &processes.back().symtab;
        }

        // compute synchronisations
        for ( int i = 0; i < processes.size(); ++ i ) {
            for ( int j = 0; j < processes.size(); ++ j ) {
                if ( i == j )
                    continue;
                processes[ i ].setupSyncs( processes[ j ].readers );
            }
        }

        // find the property process
        if ( sys.property.valid() ) {
            Symbol propid = symtab.lookup( NS::Process, sys.property );
            const parse::Process *propast = 0;
            for ( int i = 0; i < processes.size(); ++ i ) {
                if ( processes[ i ].id == propid ) {
                    property = &processes[ i ];
                    propast = &sys.processes[ i ];
                }
            }

            assert( property );
            assert( propast );

            is_accepting.resize( propast->states.size(), false );
            for ( int i = 0; i < is_accepting.size(); ++ i ) {
                for ( int j = 0; j < propast->accepts.size(); ++ j )
                    if ( propast->states[ i ].name() == propast->accepts[ j ].name() )
                        is_accepting[i] = true;
            }
        }
    }

    Continuation enabled( EvalContext &ctx, Continuation cont ) {
        bool system_deadlock = cont == Continuation() || cont.process >= processes.size();

        for ( ; cont.process < processes.size(); ++cont.process ) {
            Process &p = processes[ cont.process ];

            if ( &p == property ) // property process cannot progress alone
                continue;

            // try other property transitions first
            if ( cont.transition && property && property->valid( ctx, cont.property ) ) {
                cont.property = property->enabled( ctx, cont.property );
                if ( property->valid( ctx, cont.property ) )
                    return cont;
            }

            cont.transition = p.enabled( ctx, cont.transition );

            // is the result a valid transition?
            if ( p.valid( ctx, cont.transition ) ) {
                if ( !property )
                    return cont;
                cont.property = property->enabled( ctx, 0 );
                if ( property->valid( ctx, cont.property ) )
                    return cont;
            }

            // no more enabled transitions from this process
            cont.transition = 0;
        }

        if ( system_deadlock && property )
            cont.property = property->enabled( ctx, cont.property );

        return cont;
    }

    void initial( EvalContext &ctx ) {
        for ( int i = 0; i < processes.size(); ++i )
            initial( ctx, processes[i].symtab, NS::Variable, NS::Initialiser );
        initial( ctx, symtab, NS::Variable, NS::Initialiser );
        initial( ctx, symtab, NS::Process, NS::InitState );
    }

    void initial( EvalContext &ctx, SymTab &tab, NS::Namespace vns, NS::Namespace ins ) {
        typedef std::map< std::string, SymId > Scope;
        Scope &scope = tab.tabs[ vns ];

        for ( Scope::iterator i = scope.begin(); i != scope.end(); ++i ) {
            Symbol vsym = tab.lookup( vns, i->first ), isym = tab.lookup( ins, i->first );
            assert( vsym.valid() );
            if ( isym.valid() )
                vsym.set( ctx.mem, 0, isym.deref() );
        }
    }

    void apply( EvalContext &ctx, Continuation c ) {
        if ( c.process < processes.size() )
            processes[ c.process ].transition( ctx, c.transition ).apply( ctx );
        if ( property )
            property->transition( ctx, c.property ).apply( ctx );
    }

    bool valid( EvalContext &ctx, Continuation c ) {
        if ( c.process < processes.size() )
            return true;
        if ( property && property->valid( ctx, c.property ) )
            return true;
        return false;
    }

    bool accepting( EvalContext &ctx ) {
        if ( property )
            return is_accepting[ property->state( ctx ) ];
        return false;
    }
};

}
}

#endif
