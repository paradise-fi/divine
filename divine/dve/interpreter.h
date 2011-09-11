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

#endif
