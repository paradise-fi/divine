// -*- C++ -*- (c) 2011 Petr Rockai
//             (2) 2012, 2013 Jan Kriho

#include <memory>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <iostream>

#include <brick-assert.h>

#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <divine/dve/expression.h>

#ifndef DIVINE_DVE_INTERPRETER_H
#define DIVINE_DVE_INTERPRETER_H

namespace divine {
namespace dve {

namespace parse {
typedef std::vector< Process > Procs;
typedef std::vector< Property > Props;
typedef std::vector< Declaration > Decls;
}

struct LValue {
    Symbol symbol;
    Expression idx;
    bool _valid;

    bool valid() { return _valid; }

    template< typename T >
    void set( EvalContext &ctx, T value, ErrorState &err ) {
        if ( symbol.item().is_array )
            return symbol.set( ctx.mem, idx.evaluate( ctx, &err ), value, err );
        return symbol.set( ctx.mem, 0, value, err );
    }

    LValue( const SymTab &tab, parse::LValue lv )
        : idx( tab, lv.idx ), _valid( lv.valid() )
    {
        symbol = tab.lookup( NS::Variable, lv.ident.name() );
        if ( symbol.item().is_constant )
            lv.fail( "Cannot use a constant as lvalue!", parse::System::Fail::Semantic );
        ASSERT( symbol.valid() );
    }

    LValue() : _valid( false ) {}
};

struct LValueList {
    std::vector< LValue > lvals;
    bool _valid;
    
    bool valid() { return _valid; }
    
    template< typename T >
    void set( EvalContext &ctx, std::vector< T > values, ErrorState &err ) {
        auto lvals_it = lvals.begin();
        for( auto it = values.begin(); it != values.end(); it++, lvals_it++ ) {
            (*lvals_it).set( ctx, *it, err );
        }
    }

    std::unordered_set< SymId > getChangedSymbols() {
        std::unordered_set< SymId > symbols;
        for ( LValue l : lvals ) {
            symbols.insert( l.symbol.id );
        }
        return symbols;
    }

    std::unordered_set< SymId > getReadSymbols() {
        std::unordered_set< SymId > symbols;
        for ( LValue l : lvals ) {
            if ( l.symbol.item().is_array ) {
                auto syms = l.idx.getSymbols();
                symbols.insert( syms.begin(), syms.end() );
            }
        }
        return symbols;
    }

    LValueList( const SymTab &tab, parse::LValueList lvl )
        : _valid( lvl.valid() )
    {
        for( auto lval = lvl.lvlist.begin(); lval != lvl.lvlist.end(); lval++ ) {
            lvals.push_back( LValue(tab, *lval) );
        }
    }
    
    LValueList() : _valid( false ) {}
};

struct Channel {
    bool is_buffered, is_compound, is_array;
    int size, bufsize, width;
    std::string name;

    Symbol thischan;

    std::shared_ptr< SymContext > context;
    std::vector< Symbol > components;

    bool _valid;

    std::ostream& dump( std::ostream &os, char * data );

    char * item(char * data, int i) {
        ASSERT( _valid );
        return data + i*context->offset;
    }

    int & count(char * data) {
        ASSERT( _valid );
        char *place = data + bufsize*context->offset;
        return *reinterpret_cast< int * >( place );
    }

    void enqueue( EvalContext &ctx, std::vector< int > values, ErrorState &err ) {
        ASSERT( _valid );
        char * data = thischan.getref( ctx.mem, 0 );
        char * _item = item( data, count( data ) );
        ASSERT_EQ( values.size(), components.size() );

        for ( unsigned i = 0; i < values.size(); i++ ) {
            components[ i ].set( _item, 0, values[ i ], err );
        }

        count( data )++;
    }

    std::vector< int > dequeue( EvalContext &ctx, ErrorState & ) {
        ASSERT( _valid );
        char * data = thischan.getref( ctx.mem, 0 );
        std::vector< int > retval;
        retval.resize( components.size() );

        for ( unsigned i = 0; i < components.size(); i++ ) {
            retval[ i ] = components[ i ].deref( data, 0 );
        }

        memmove( data, item( data, 1 ), context->offset * ( --count( data ) ) );
        memset( item( data, count( data ) ), 0, context->offset );

        return retval;
    }

    bool full( EvalContext &ctx ) {
        ASSERT( _valid );
        char * data;
        if ( is_buffered )
            data = thischan.getref( ctx.mem, 0 );
        else
            return false;
        return count( data ) >= bufsize;
    };

    bool empty( EvalContext &ctx ) {
        ASSERT( _valid );
        char * data;
        if ( is_buffered )
            data = thischan.getref( ctx.mem, 0 );
        else
            return false;
        return count( data ) <= 0;
    };

    Channel() : _valid( false ) {}

    Channel( SymTab &sym, const parse::ChannelDeclaration &chandecl )
        : is_compound( 1 ),  is_array( 0 ), size( 1 ),
        context( new SymContext() ), _valid( true )
    {
        is_buffered = chandecl.is_buffered;
        bufsize = chandecl.size;
        name = chandecl.name;
        components.resize( chandecl.components.size() );
        for( unsigned i = 0; i < chandecl.components.size(); i++ ) {
            context->ids.push_back( SymContext::Item() );
            SymContext::Item &it = context->ids[ context->id ];
            it.offset = context->offset;
            it.width = chandecl.components[ i ];
            context->offset += it.width;
            components[ i ] =  Symbol( context.get(), context->id );
            context->id++;
        }
        width = bufsize * context->offset + sizeof( int );
        if ( is_buffered )
            thischan = sym.allocate( NS::Channel, *this );
    }

};

struct Process;

struct Transition {
    Symbol process;
    std::string procname;
    int procIndex;
    Symbol from, to;

    Channel *sync_channel;
    LValueList sync_lval;
    ExpressionList sync_expr;

    // Reader transition to this one (in case of atomic sync)
    Transition *sync;

    std::vector< Expression > guards;
    typedef std::vector< std::pair< LValue, Expression > > Effect;
    Effect effect;
    
    parse::Transition parse;

    std::set< SymId > symDepends, symChanges, symReads;
    std::unordered_set< Transition * > pre, dep;
    std::unordered_map< Process *, bool > visible;

    bool from_commited, to_commited;
    Symbol flags;

    bool enabled( EvalContext &ctx, ErrorState &err ) {
        if ( process.deref( ctx.mem ) != from.deref( 0 ) )
            return false;
        for ( size_t i = 0; i < guards.size(); ++i )
            if ( !guards[i].evaluate( ctx, &err ) )
                return false;
        if ( sync_channel )
            if ( sync_expr.valid() && sync_channel->full( ctx ) )
                return false;
            if ( sync_lval.valid() && sync_channel->empty( ctx ) )
                return false;
        if ( sync ) {
            if ( from_commited && !sync->from_commited )
                return false;
            if ( !from_commited && sync->from_commited )
                return false;
            if ( !sync->enabled( ctx, err ) )
                return false;
        }
        return true;
    }

    void apply( EvalContext &ctx, ErrorState &err ) {
        if ( sync_channel && sync_channel->is_buffered ) {
                if( sync_expr.valid() )
                    sync_channel->enqueue( ctx, sync_expr.evaluate( ctx, &err ), err );
                else if ( sync_lval.valid() )
                    sync_lval.set( ctx, sync_channel->dequeue( ctx, err ), err );
                else
                    ASSERT_UNREACHABLE( "invalid sync expression" );
        }
        if ( sync ) {
            if (sync->sync_lval.valid() && sync_expr.valid() )
                    sync->sync_lval.set( ctx, sync_expr.evaluate( ctx, &err ), err );
            sync->apply( ctx, err );
        }
        for ( Effect::iterator i = effect.begin(); i != effect.end(); ++i )
            i->first.set( ctx, i->second.evaluate( ctx, &err ), err );
        process.set( ctx.mem, 0, to.deref(), err );
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        if ( from_commited )
            sflags.f.commited_dirty |= !to_commited;
        else
            sflags.f.commited |= to_commited;
        flags.set( ctx.mem, 0, sflags );
    }

    std::ostream& dump( std::ostream &o ) {
        o << procname << ": ";
        parse.dump( o );
        return o;
    }

    void init( SymTab &sym ) {
        for ( size_t i = 0; i < parse.guards.size(); ++ i )
            guards.push_back( Expression( sym, parse.guards[i] ) );
        for ( size_t i = 0; i < parse.effects.size(); ++ i )
            effect.push_back( std::make_pair( LValue( sym, parse.effects[i].lhs ),
                                              Expression( sym, parse.effects[i].rhs ) ) );

        if ( parse.syncexpr.valid() ) {
            if ( parse.syncexpr.proc.valid() ) {
                Symbol chanProc = sym.lookup( NS::Process, parse.syncexpr.proc.ident );
                sync_channel = sym.toplevel()->child( chanProc )->lookupChannel( parse.syncexpr.chan );
            }
            else
                sync_channel = sym.lookupChannel( parse.syncexpr.chan );
            if ( parse.syncexpr.write )
                sync_expr = ExpressionList( sym, parse.syncexpr.exprlist );
            else if ( parse.syncexpr.lvallist.valid() )
                sync_lval = LValueList( sym, parse.syncexpr.lvallist );
        }
    }

    void gatherSymbols() {
        for ( auto e : effect ) {
            symChanges.insert( e.first.symbol.id );
            auto symbols = e.second.getSymbols();
            symReads.insert( symbols.begin(), symbols.end() );
            if ( e.first.symbol.item().is_array ) {
                symbols = e.first.idx.getSymbols();
                symReads.insert( symbols.begin(), symbols.end() );
            }
        }

        for ( Expression g : guards ) {
            auto symbols = g.getSymbols();
            symDepends.insert( symbols.begin(), symbols.end() );
        }

        if ( sync_expr.valid() ) {
            auto symbols = sync_expr.getSymbols();
            symReads.insert( symbols.begin(), symbols.end() );
        }

        if ( sync_lval.valid() ) {
            auto symbols = sync_lval.getReadSymbols();
            symReads.insert( symbols.begin(), symbols.end() );
            symbols = sync_lval.getChangedSymbols();
            symChanges.insert( symbols.begin(), symbols.end() );
        }

        if ( from != to )
            symChanges.insert( process.id );

        if ( sync )
            sync->gatherSymbols();
    }

    bool isInPre( Transition & t ) {
        std::vector< SymId > intersection;
        if ( t.to == from )
            return true;

        std::set_intersection(
            t.symChanges.begin(), t.symChanges.end(),
            symDepends.begin(), symDepends.end(),
            std::back_inserter( intersection )
        );
        if ( !intersection.empty() )
            return true;

        if ( sync_channel && sync_channel->is_buffered
                && t.sync_channel && t.sync_channel->is_buffered
                && sync_channel == t.sync_channel )
        {
            if ( sync_expr.valid() && t.sync_lval.valid() )
                return true;
            if ( sync_lval.valid() && t.sync_expr.valid() )
                return true;
        }

        return false;
    }

    void buildPreSet( std::vector< Transition > &trans ) {
        for ( Transition &t : trans ) {
                if ( &t == this )
                    continue;
                if ( isInPre( t ) )
                    pre.insert( &t );
                if ( t.sync && isInPre( *t.sync ) )
                    pre.insert( &t );
        }

        if ( sync ) {
            sync->buildPreSet( trans );
            pre.insert( sync->pre.begin(), sync->pre.end() );
        }
    }

    bool isInDep( Transition & t, const std::vector< SymId > union1 ) {
        std::vector< SymId > union2, intersection;
        std::set_union(
            t.symDepends.begin(), t.symDepends.end(),
            t.symChanges.begin(), t.symChanges.end(),
            std::back_inserter( union2 )
        );
        std::set_intersection(
            union1.begin(), union1.end(),
            t.symChanges.begin(), t.symChanges.end(),
            std::back_inserter( intersection )
        );
        std::set_intersection(
            union2.begin(), union2.end(),
            symChanges.begin(), symChanges.end(),
            std::back_inserter( intersection )
        );
        if ( !intersection.empty() )
            return true;

        if ( procIndex == t.procIndex )
            return true;

        if ( sync_channel && sync_channel->is_buffered
                && t.sync_channel && t.sync_channel->is_buffered
                && sync_channel == t.sync_channel )
        {
            if ( sync_expr.valid() && t.sync_expr.valid() )
                return true;
            if ( sync_lval.valid() && t.sync_lval.valid() )
                return true;
        }

        return false;
    }

    void buildDepSet( std::vector< Transition > &trans ) {
        std::vector< SymId > union1;
        std::set_union(
            symDepends.begin(), symDepends.end(),
            symChanges.begin(), symChanges.end(),
            std::back_inserter( union1 )
        );

        for ( Transition &t : trans ) {
                if ( &t == this )
                    continue;
                if ( isInDep( t, union1 ) )
                    dep.insert( &t );
                if ( t.sync && isInDep( *t.sync, union1 ) )
                    dep.insert( &t );
        }

        if ( sync ) {
            sync->buildDepSet( trans );
            dep.insert( sync->dep.begin(), sync->dep.end() );
        }
    }

    bool isVisible( Transition &t ) {
        std::vector< SymId > intersection;
        std::set_intersection(
            t.symDepends.begin(), t.symDepends.end(),
            symChanges.begin(), symChanges.end(),
            std::back_inserter( intersection )
        );
        std::set_intersection(
            t.symReads.begin(), t.symReads.end(),
            symChanges.begin(), symChanges.end(),
            std::back_inserter( intersection )
        );
        return intersection.empty();
    }

    void computeVisibility( Process * prop );

    Transition( SymTab &sym, Symbol proc, parse::Transition t )
        : process( proc ), procIndex( -1 ), sync_channel( 0 ), sync( 0 ), parse( t )
    {
        procname = "";
        for ( auto it = sym.parent->tabs[ NS::Process ].begin(); it != sym.parent->tabs[ NS::Process ].end(); it++ ) {
            if ( it->second == process.id) {
                procname = it->first;
            }
        }

        from = sym.lookup( NS::State, t.from );
        ASSERT( from.valid() );
        to = sym.lookup( NS::State, t.to );
        ASSERT( to.valid() );

        flags = sym.lookup( NS::Flag, "Flags" );
    }
};

static inline void declare( SymTab &symtab, const parse::Decls &decls )
{
    for ( parse::Decls::const_iterator i = decls.begin(); i != decls.end(); ++i ) {
        if ( !i->is_const )
            symtab.allocate( NS::Variable, *i );
        std::vector< int > init;
        EvalContext ctx;
        for ( size_t j = 0; j < i->initial.size(); ++j )
            init.push_back( i->initial[ j ]);

        // TODO: add proper runtime check for array size validity
        ASSERT_LEQ( 0, i->size );
        while ( init.size() < static_cast<unsigned>(i->size) )
            init.push_back( 0 );
        symtab.constant( i->is_const ? NS::Variable : NS::Initialiser, i->name, init );
    }
}

struct Process {
    Symbol id;
    SymTab symtab;

    std::vector< Transition > all_trans;
    std::vector< std::vector< Transition > > trans;
    std::vector< std::vector< Transition > > state_readers;

    std::vector< Transition > readers;
    std::vector< Transition > writers;

    std::vector< bool > is_accepting, is_commited;

    std::vector< std::vector< Expression > > asserts;

    std::vector< Channel > channels;

    int state( EvalContext &ctx ) {
        return id.deref( ctx.mem );
    }

    bool commited( EvalContext &ctx ) {
        return is_commited[ state( ctx ) ];
    }

    bool assertValid( EvalContext &ctx ) {
        for ( auto expr = asserts[ state( ctx ) ].begin(); expr != asserts[ state( ctx ) ].end(); ++ expr )
            if ( !expr->evaluate( ctx ) )
                return false;
        return true;
    }

    int available( EvalContext &ctx ) {
        ASSERT_LEQ( size_t( state( ctx ) + 1 ), trans.size() );
        return trans[ state( ctx ) ].size() > 0 ||
                state_readers[ state( ctx ) ].size() > 0;
    }

    int enabled( EvalContext &ctx, unsigned i, ErrorState &err ) {
        ErrorState temp_err = ErrorState::e_none;
        ASSERT_LEQ( size_t( state( ctx ) + 1 ), trans.size() );
        std::vector< Transition > &tr = trans[ state( ctx ) ];
        for ( ; i < tr.size(); ++i ) {
            if ( tr[ i ].enabled( ctx, temp_err ) )
                break;
            temp_err.error = ErrorState::i_none;
        }
        err |= temp_err;
        return i + 1;
    }

    bool valid( EvalContext &ctx, unsigned i ) {
        return i <= trans[ state( ctx ) ].size();
    }

    Transition &transition( EvalContext &ctx, int i ) {
        ASSERT_LEQ( size_t( state( ctx ) + 1 ), trans.size() );
        ASSERT_LEQ( i, int( trans[ state( ctx ) ].size() ) );
        ASSERT_LEQ( 1, i );
        return trans[ state( ctx ) ][ i - 1 ];
    }

    Process( SymTab *parent, Symbol id, const parse::Automaton &proc )
        : id( id ), symtab( parent )
    {
        int states = 0;
        ASSERT( id.valid() );

        declare( symtab, proc.body.decls );
        for ( std::vector< parse::Identifier >::const_iterator i = proc.body.states.begin();
              i != proc.body.states.end(); ++i ) {
            if ( proc.body.inits.size() && i->name() == proc.body.inits.front().name() )
                parent->constant( NS::InitState, proc.name.name(), states );
            symtab.constant( NS::State, i->name(), states++ );
        }

        // declare channels
        channels.resize( proc.body.chandecls.size() );
        for( size_t i = 0; i < proc.body.chandecls.size(); i++ ) {
            channels[i] = Channel( symtab, proc.body.chandecls[i] );
            symtab.channels[ proc.body.chandecls[i].name ] =  &channels[i];
        }

        ASSERT_EQ( states, int( proc.body.states.size() ) );

        is_accepting.resize( proc.body.states.size(), false );
        is_commited.resize( proc.body.states.size(), false );
        asserts.resize( proc.body.states.size() );
        for ( size_t i = 0; i < is_accepting.size(); ++ i ) {
            for ( size_t j = 0; j < proc.body.accepts.size(); ++ j )
                if ( proc.body.states[ i ].name() == proc.body.accepts[ j ].name() )
                    is_accepting[i] = true;
            for ( size_t j = 0; j < proc.body.commits.size(); ++ j )
                if ( proc.body.states[ i ].name() == proc.body.commits[ j ].name() )
                    is_commited[i] = true;
            for ( size_t j = 0; j < proc.body.asserts.size(); ++ j )
                if ( proc.body.states[ i ].name() == proc.body.asserts[ j ].state.name() )
                    asserts[ i ].push_back( Expression( symtab, proc.body.asserts[ j ].expr) );
        }

        trans.resize( proc.body.states.size() );
        state_readers.resize( proc.body.states.size() );

        for ( std::vector< parse::Transition >::const_iterator i = proc.body.trans.begin();
              i != proc.body.trans.end(); ++i ) {
            Transition t( symtab, id, *i );
            t.from_commited = is_commited[ t.from.deref( 0 ) ];
            t.to_commited = is_commited[ t.to.deref( 0 ) ];
            all_trans.push_back( t );
        }

    }

    void init() {
        for ( Transition &t : all_trans ) {
            t.init( symtab );
            if ( t.parse.syncexpr.valid() && !t.sync_channel->is_buffered ) {
                if ( t.parse.syncexpr.write )
                    writers.push_back( t );
                else {
                    readers.push_back( t );
                    state_readers[ t.from.deref() ].push_back( t );
                }
            } else
                trans[ t.from.deref() ].push_back( t );
        }
    }

    void setupSyncs( std::vector< Transition > &readers )
    {
        for ( size_t w = 0; w < writers.size(); ++ w ) {
            Transition &tw = writers[w];
            for ( size_t r = 0; r < readers.size(); ++r ) {
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

    void gatherSymbols() {
        for ( auto &tv : trans)
            for ( Transition &t : tv )
                t.gatherSymbols();
    }

    void buildPreSet( Process & proc ) {
        for ( std::vector< Transition > &tv : trans )
            for ( Transition &t : tv )
                for ( std::vector< Transition > &ftv : proc.trans )
                    t.buildPreSet( ftv );
    }

    void buildDepSet( Process & proc ) {
        for ( std::vector< Transition > &tv : trans )
            for ( Transition &t : tv )
                for ( std::vector< Transition > &ftv : proc.trans )
                    t.buildDepSet( ftv );
    }

    void computeVisibility( Process & prop ) {
        for ( std::vector< Transition > &tv : trans )
            for ( Transition &t : tv )
                t.computeVisibility( &prop );
    }

    void setProcIndex( int pid ) {
        for( auto &i : readers )
            i.procIndex = pid;
        for ( auto &i : writers )
            i.procIndex = pid;

        for ( auto &tv : trans )
            for ( auto &t : tv )
                t.procIndex = pid;
    }
};

struct System {
    SymTab symtab;
    std::vector< Process > processes;
    std::vector< Process > properties;
    std::vector< Channel > channels;
    Process *property;

    Symbol flags, errFlags;

    struct Continuation {
        unsigned process:16; // the process whose turn it is
        unsigned property:16; // active property transition; 0 = none
        unsigned transition:32; // active process transition; 0 = none
        ErrorState err;
        bool deadlocked;
        Continuation() : process( 0 ), property( 0 ), transition( 0 ), err( ErrorState::e_none ), deadlocked( 0 ) {}
        bool operator==( const Continuation &o ) const {
            return process == o.process && property == o.property && transition == o.transition;
        }
        bool operator!=( const Continuation &o ) const {
            return !this->operator==( o );
        }
    };

    System( const parse::System &sys )
        : property( 0 )
    {
        ASSERT( !sys.synchronous ); // XXX

        declare( symtab, sys.decls );
        flags = symtab.lookup( NS::Flag, "Flags" );
        errFlags = symtab.lookup( NS::Flag, "Error" );

        // declare channels
        channels.resize( sys.chandecls.size() );
        for( size_t i = 0; i < sys.chandecls.size(); i++ ) {
            channels[i] = Channel( symtab, sys.chandecls[i] );
            symtab.channels[ sys.chandecls[i].name ] =  &channels[i];
        }

        // ensure validity of pointers into the process array
        processes.reserve( sys.processes.size() );
        properties.reserve( sys.properties.size() + 1 );

        for ( parse::Procs::const_iterator i = sys.processes.begin();
              i != sys.processes.end(); ++i )
        {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            if ( i->name.name() == sys.property.name() ) {
                properties.push_back( Process( &symtab, id, *i ) );
                symtab.children[id] = &properties.back().symtab;
            }
            else {
                processes.push_back( Process( &symtab, id, *i ) );
                symtab.children[id] = &processes.back().symtab;
            }
        }

        for ( parse::Props::const_iterator i = sys.properties.begin();
              i != sys.properties.end(); ++i )
        {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            properties.push_back( Process( &symtab, id, *i ) );
            symtab.children[id] = &properties.back().symtab;
        }

        for ( Process &p : processes ) {
            p.init();
        }

        for ( Process &p : properties ) {
            p.init();
        }

        // compute synchronisations
        for ( size_t i = 0; i < processes.size(); ++ i ) {
            processes[ i ].setProcIndex( i );
        }

        for ( size_t i = 0; i < processes.size(); ++ i ) {
            for ( size_t j = 0; j < processes.size(); ++ j ) {
                if ( i == j )
                    continue;
                processes[ i ].setupSyncs( processes[ j ].readers );
            }
        }

        // find the property process
        if ( sys.property.valid() ) {
            Symbol _propid = symtab.lookup( NS::Process, sys.property );
            for ( size_t i = 0; i < properties.size(); ++ i ) {
                if ( properties[ i ].id == _propid ) {
                    property = &properties[ i ];
                }
            }

            ASSERT( property );
        }
    }

    void PORInit() {
        for ( Process &p : processes )
            p.gatherSymbols();
        for ( Process &p : properties )
            p.gatherSymbols();

        for ( Process &p1 : processes ) {
            for ( Process &p2 : processes ) {
                p1.buildPreSet( p2 );
                p1.buildDepSet( p2 );
            }

            for ( Process &prop : properties )
                p1.computeVisibility( prop );
        }
    }

    bool assertValid( EvalContext &ctx ) {
        for ( auto proc = processes.begin(); proc != processes.end(); ++ proc)
            if ( !proc->assertValid( ctx ) )
                return false;
        return true;
    }

    void setCommitedFlag( EvalContext &ctx ) {
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        sflags.f.commited_dirty = 0;
        sflags.f.commited = 0;
        for ( size_t i = 0; i < processes.size(); i++ ) {
            sflags.f.commited |= static_cast<bool>(processes[i].commited( ctx ));
        }
        flags.set( ctx.mem, 0, sflags );
    }

    bool processEnabled( EvalContext &ctx, Continuation &cont ) {
        Process &p = processes[ cont.process ];

        // try other property transitions first
        if ( cont.transition && property && property->valid( ctx, cont.property ) ) {
            cont.property = property->enabled( ctx, cont.property, cont.err );
            if ( property->valid( ctx, cont.property ) )
                return true;
        }

        cont.transition = p.enabled( ctx, cont.transition, cont.err );

        // is the result a valid transition?
        if ( p.valid( ctx, cont.transition ) ) {
            if ( !property )
                return true;
            cont.property = property->enabled( ctx, 0, cont.err );
            if ( property->valid( ctx, cont.property ) )
                return true;
        }

        // no more enabled transitions from this process
        cont.transition = 0;
        cont.err.error = ErrorState::e_none.error;

        return false;
    }

    Continuation enabledPrioritized( EvalContext &ctx, Continuation cont ) {
        if ( cont == Continuation() ) {
            bool commitEnable = false;
            for ( size_t i = 0; i < processes.size(); i++ ) {
                Process &pa = processes[ i ];
                if ( pa.commited( ctx ) && pa.available( ctx ) ) {
                    commitEnable = true;
                    break;
                }
            }
            if ( !commitEnable ) {
                StateFlags sflags;
                flags.deref( ctx.mem, 0, sflags );
                sflags.f.commited = 0;
                flags.set( ctx.mem, 0, sflags );
                return enabledAll( ctx, cont );
            }
        }
        for ( ; cont.process < processes.size(); ++cont.process ) {
            Process &pb = processes[ cont.process ];
            if ( !pb.commited( ctx ) )
                continue;
            if ( processEnabled( ctx, cont ) )
                return cont;
        }
        return cont;
    }

    Continuation enabledAll( EvalContext &ctx, Continuation cont ) {
        for ( ; cont.process < processes.size(); ++cont.process ) {
            if ( processEnabled( ctx, cont ) )
                return cont;
        }
        return cont;
    }

    Continuation enabled( EvalContext &ctx, Continuation cont ) {
        cont.deadlocked = cont == Continuation() || cont.deadlocked;
        cont.err.error = ErrorState::i_none;
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );

        if ( sflags.f.commited ) {
            cont = enabledPrioritized( ctx, cont );
        }
        else {
            cont = enabledAll( ctx, cont );
        }

        cont.deadlocked = cont.deadlocked && cont.process >= processes.size();

        if ( cont.deadlocked && property )
            cont.property = property->enabled( ctx, cont.property, cont.err );

        return cont;
    }

    Transition & getTransition( EvalContext &ctx, Continuation cont ) {
        return processes[ cont.process ].transition( ctx, cont.transition );
    }

    bool processAffected( EvalContext &ctx, Continuation cont, int pid ) {
        if ( cont.process >= processes.size() )
            return true;
        if ( cont.process == getProcIndex( pid ) )
            return true;
        Transition &trans = getTransition( ctx, cont );
        if ( trans.sync ) {
            ASSERT_LEQ( 0, trans.sync->procIndex );
            if (trans.sync->procIndex == getProcIndex( pid ) )
                return true;
        }
        return false;
    }

    int getProcIndex( int pid ) {
        return pid;
    }

    int processCount() {
        return processes.size();
    }

    void initial( EvalContext &ctx ) {
        flags.set(ctx.mem, 0, StateFlags::f_none);
        for ( size_t i = 0; i < processes.size(); ++i ) {
            initial( ctx, processes[i].symtab, NS::Variable, NS::Initialiser );
        }
        for ( size_t i = 0; i < properties.size(); ++i ) {
            initial( ctx, properties[i].symtab, NS::Variable, NS::Initialiser );
        }
        initial( ctx, symtab, NS::Variable, NS::Initialiser );
        initial( ctx, symtab, NS::Process, NS::InitState );
        errFlags.set( ctx.mem, 0, ErrorState::e_none );
        setCommitedFlag( ctx );
    }

    void initial( EvalContext &ctx, SymTab &tab, NS::Namespace vns, NS::Namespace ins ) {
        typedef std::map< std::string, SymId > Scope;
        Scope &scope = tab.tabs[ vns ];
        ErrorState err;

        for ( Scope::iterator i = scope.begin(); i != scope.end(); ++i ) {
            Symbol vsym = tab.lookup( vns, i->first ), isym = tab.lookup( ins, i->first );
            ASSERT( vsym.valid() );
            if ( isym.valid() ) {
                if ( vsym.item().is_array )
                    for ( int index = 0; index < vsym.item().array; index++ )
                        vsym.set( ctx.mem, index, isym.deref( 0, index ), err );
                else
                    vsym.set( ctx.mem, 0, isym.deref(), err );
	    }
        }
    }

    void apply( EvalContext &ctx, Continuation c ) {
        if ( c.process < processes.size() )
            processes[ c.process ].transition( ctx, c.transition ).apply( ctx, c.err );
        if ( property )
            property->transition( ctx, c.property ).apply( ctx, c.err );
        if ( c.err.error )
            bail( ctx, c );
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        if ( sflags.f.commited_dirty )
            setCommitedFlag( ctx );

    }

    void bail( EvalContext &ctx, Continuation c ) {
        for( int i = 0; i < symtab.context->offset; i++ )
            ctx.mem[i] = 0;
        errFlags.set( ctx.mem, 0, c.err );
    }

    bool valid( EvalContext &ctx, Continuation c ) {
        ErrorState err;
        errFlags.deref( ctx.mem, 0, err );
        if ( err.error )
            return false;
        if ( c.process < processes.size() )
            return true;
        if ( property && property->valid( ctx, c.property ) )
            return true;
        return false;
    }

    bool accepting( EvalContext &ctx ) {
        if ( property )
            return property->is_accepting[ property->state( ctx ) ];
        return false;
    }

    std::ostream& printTrans( std::ostream &o, EvalContext &ctx, Continuation c ) {
        if ( c.process < processes.size() ) {
            Transition &trans = processes[ c.process ].transition( ctx, c.transition );
            trans.dump( o );
            if ( trans.sync ) {
                o << std::endl;
                trans.sync->dump( o );
            }
        }
        if ( property ) {
            o << std::endl;
            property->transition( ctx, c.property ).dump( o );
        }
        return o;
    }
};

}
}

#endif
