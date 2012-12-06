#include "gen.h"
#include <string>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <limits>

std::string errString( int code ) {
    assert( code != 0 );
    switch ( code ) {
    case Evaluator::ERR_DIVIDE_BY_ZERO:
        return "Divide by zero";
    case Evaluator::ERR_ARRAY_INDEX:
        return "Array index out of bounds";
    case Evaluator::ERR_OUT_OF_RANGE:
        return "Out of range variable assignment";
    case Evaluator::ERR_SHIFT:
        return "Invalid shift operation";
    case Evaluator::ERR_NEG_CLOCK:
        return "Negative clock assignment";
    case TAGen::ERR_INVARIANT:
        return "Invariant does not hold";
    default:
        return "Unknown error";
    }
}

void TAGen::EdgeInfo::assignSubst( const EdgeInfo& ei, const UTAP::symbol_t& sym, const UTAP::expression_t& expr ) {
    guard = ei.guard.subst( sym, expr );
    assign = ei.assign.subst( sym, expr );
    sync = ei.sync.subst( sym, expr );
    syncType = ei.syncType;
    dstId = ei.dstId;
    procId = ei.procId;
}

void TAGen::processEdge( int procId, const UTAP::edge_t& e, std::vector< EdgeInfo >& out ) {
    EdgeInfo einf;
    einf.guard = e.guard;
    einf.assign = e.assign;
    if ( !e.sync.empty() ) {
        einf.syncType = e.sync.getSync();
        einf.sync = e.sync[ 0 ];    // e.sync is SYNC expression with single subexpression evaluating to a channel
    } else {
        einf.syncType = -1;
        einf.sync = UTAP::expression_t();
    }
    einf.dstId = e.dst->locNr;
    einf.procId = procId;

    out.push_back( einf );
    int pushed = 1;
    // for select edges, make one copy for each possible combination of the select variables
    for ( uint32_t v = 0; v < e.select.getSize(); ++v ) {
        auto r = eval.evalRange( procId, e.select[ v ].getType() );
        int base = out.size() - pushed;
        out.resize( base + pushed * ( r.second - r.first + 1 ) );
        // copy every edge we already pushed to _out_ for every possible assignment to this variable
        for ( int i = 0; i < pushed; ++i ) {
            for ( int cur = r.second; cur >= r.first; --cur ) {
                out[ base + i + (cur - r.first) * pushed ].assignSubst(
                    out[ base + i ], e.select[ v ], UTAP::expression_t::createConstant( cur ) );
            }
        }
        pushed = out.size() - base;
    }
}

void TAGen::processInstance( const UTAP::instance_t& inst, std::vector< UTAP::instance_t >& out ) {
    out.push_back( inst );
    int pushed = 1;
    // for partial instances, make one copy for each possible combination of unbound parameters
    for ( uint32_t v = 0; v < inst.unbound; ++v ) {
        // unbound parameters are listed first in the _parameters_ array
        auto r = eval.evalRange( -1, inst.parameters[ v ].getType() );
        int base = out.size() - pushed;
        out.resize( base + pushed * ( r.second - r.first + 1 ) );
        // copy every instance we already pushed to _out_ for every possible assignment to this parameter
        for ( int i = 0; i < pushed; ++i ) {
            for ( int cur = r.second; cur >= r.first; --cur ) {
                unsigned int index = base + i + (cur - r.first) * pushed;
                out[ index ] = out[ base + i ];
                out[ index ].mapping[ inst.parameters[ v ] ] = UTAP::expression_t::createConstant( cur );
            }
        }
        pushed = out.size() - base;
    }
}

bool TAGen::evalInv() {
    int nInst = 0;
    for ( auto proc = states.begin(); proc != states.end(); ++proc, ++nInst ) {
        StateInfo& s = (*proc)[ locs.get( nInst ) ];
        if ( !eval.evalBool( nInst, s.inv ) )
            return false;
    }
    return true;
}

void TAGen::slice( TAGen::SuccList& list, const std::vector< Cut >& cuts ) {
    if ( cuts.empty() ) {
        if ( evalInv() ) {
            this->eval.extrapolate();
            list.push_back();
        }
        return;
    }
    std::vector< char > srcCopy( stateSize() );
    memcpy( &srcCopy[0], list.back(), stateSize() );

    for ( unsigned int sel = 0; sel < (1 << cuts.size()); ++sel ) {
        newSucc( list.back(), &srcCopy[0] );
        bool possible = true;
        for ( unsigned int i = 0; i < cuts.size(); ++i ) {
            possible = possible && eval.evalBool( cuts[ i ].pId, (sel & (1 << i)) ? cuts[ i ].pos : cuts[ i ].neg );
        }
        if ( possible && evalInv() ) {
            this->eval.extrapolate();
            list.push_back();
        }
    }
}

void TAGen::listSuccessors( char* source, SuccList& succs, unsigned int& begin ) {
    std::vector< EdgeInfo* > trans; // non-synchronizing transitions and sending ends of synchronizations
    std::map< chan_id, std::vector< EdgeInfo* > > sync; // receiving ends
    bool inUrgent = false;
    std::set< int > commit;   // set of processes in a commited location
    begin = 0;

    // find possible transitions and synchronizations
    int nInst = 0;
    for ( auto proc = states.begin(); proc != states.end(); ++proc, ++nInst ) {
        StateInfo& s = (*proc)[ locs.get( nInst ) ];
        inUrgent = inUrgent || s.urgent;
        if ( s.commit )
            commit.insert( nInst );
        for ( auto tr = s.edges.begin(); tr != s.edges.end(); ++tr ) {
            if ( tr->syncType == UTAP::Constants::SYNC_QUE ) {
                assert( !tr->sync.empty() );
                setData( source );  // resets error flag
                chan_id chan = eval.evalChan( nInst, tr->sync );
                if ( !eval.error )
                    sync[ chan ].push_back( &*tr );
            } else {
                trans.push_back( &*tr );
            }
        }
    }

    // process synchronizations
    for ( auto itr = trans.begin(); itr != trans.end(); ++itr ) {
        newSucc( succs.back(), source );
        // continue if the guard holds ar there was an error while evaluating the guard (makeSucc will produce an error state if the error flag is set)
        if ( !evalGuard( *itr ) && !eval.error ) continue;

        if ( (*itr)->syncType == UTAP::Constants::SYNC_BANG ) {
            assert( !(*itr)->sync.empty() );
            chan_id chan = eval.evalChan( (*itr)->procId, (*itr)->sync );

            if ( eval.error )
                makeSucc( succs );
            else if ( eval.isChanBcast( chan ) ) {
                // n-ary synchronization
                bool fromCommit = commit.empty() || commit.count( (*itr)->procId );
                succs.setInfo( *itr, eval.getChanPriority( chan ), edgePrio( *itr ) );
                std::vector< std::vector< EdgeInfo* > > receivers;  // enabled receiving edges for each process
                int lastPID = -1;
                for ( auto recv = sync[ chan ].begin(); recv != sync[ chan ].end(); ++recv ) {
                    if ( (*itr)->procId == (*recv)->procId ) continue;  // prevent synchronization with itself
                    if ( !evalGuard( *recv ) ) continue;    // this does not change the state, because broadcast guards can not contain clocks
                    if ( (*recv)->procId != lastPID ) { // edges in _sync_ are sorted by process id
                        receivers.push_back( std::vector< EdgeInfo* >() );
                        lastPID = (*recv)->procId;
                        fromCommit = fromCommit || commit.count( (*recv)->procId );
                        succs.appendPriority( edgePrio( *recv ) );
                    }
                    receivers.back().push_back( *recv );
                }
                if ( !fromCommit ) continue;    // skip if system is in commited state, but no participant is in commited location
                if ( eval.error ) {     // if there was an error while evaluating guards, create an error state and quit
                    makeSucc( succs );
                    continue;
                }

                std::vector< unsigned int > sel( receivers.size() );    // combination of edges from _receivers_
                do {
                    newSucc( succs.back(), source );
                    evalGuard( *itr );  // already evaluated earlier, needed here for clock constraints
                    // at this point, the transition is considered to be enabled
                    inUrgent = inUrgent || eval.isChanUrgent( chan );
                    applyEdge( *itr );
                    for ( unsigned int i = 0; i < sel.size(); i++ )
                        applyEdge( receivers[ i ][ sel[ i ] ] );
                    makeSucc( succs );
                } while ( nextSelection( sel, receivers ) );
            } else {
                // binary synchronization
                for ( auto recv = sync[ chan ].begin(); recv != sync[ chan ].end(); ++recv ) {
                    if ( (*itr)->procId == (*recv)->procId ) continue;  // prevent synchronization with itself
                    newSucc( succs.back(), source );
                    evalGuard( *itr );  // already evaluated earlier, needed here for clock constraints
                    if ( !evalGuard( *recv ) ) continue;
                    // if in commited state, at least one synchronizing process has to be in commited location
                    if ( !commit.empty() && !commit.count( (*itr)->procId ) && !commit.count( (*recv)->procId ) ) continue;
                    // at this point, the transition is considered to be enabled
                    inUrgent = inUrgent || eval.isChanUrgent( chan );
                    succs.setInfo( *itr, eval.getChanPriority( chan ), edgePrio( *itr ) );
                    succs.appendPriority( edgePrio( *recv ) );
                    applyEdge( *itr );
                    applyEdge( *recv );
                    makeSucc( succs );
                }
            }
        } else {
            // no synchronization
            if ( !commit.empty() && !commit.count( (*itr)->procId ) ) continue;
            // at this point, the transition is considered to be enabled
            succs.setInfo( *itr, sys.getTauPriority(), edgePrio( *itr ) );
            applyEdge( *itr );
            makeSucc( succs );
        }
    }

    succs.finalizePriorities();

    if ( !inUrgent && commit.empty() ) {
        unsigned int timedIndex = succs.count(); // beginning of time sucessors in the successor list

        newSucc( succs.back(), source );
        eval.up();
        makeTimeSucc( succs );

#ifndef TIMED_NO_POR
        // If we are in non-urgent state and there is no time self-loop, skip all the transition successors
        // because then there is a time successor with a strictly greater zone with all the successors this state would have.
        // Reduces state-space, but breaks the LTL next operator
        bool selfLoop = false;
        for ( unsigned int i = timedIndex; i < succs.size() - 1; i++ ) {
            selfLoop = selfLoop || ( memcmp( succs[ i ], source, stateSize() ) == 0 );
        }
        if ( !selfLoop )
            begin = timedIndex; // skip to time successors
#endif
    }
}

void TAGen::read( const std::string& path ) {
    // clear internal containers
    states.clear();
    procs.clear();
    cutClocks.clear();
    sys.~TimedAutomataSystem(); // this is the only functional way to reset TimedAutomataSystem
    new (&sys) UTAP::TimedAutomataSystem;
    eval = Evaluator();

    // read model
    parseXMLFile( path.c_str(), &sys, true );
    if ( sys.hasErrors() ) {
        std::cerr << "Errors in the input file:" << std::endl;
        for ( auto err = sys.getErrors().begin(); err != sys.getErrors().end(); ++err )
            std::cerr << *err << std::endl;
        throw std::runtime_error( "Error reading input model." );
    }

    // initialize evaluator
    eval.processDeclGlobals( sys );

    std::vector< UTAP::instance_t > instances;
    for ( auto inst = sys.getProcesses().begin(); inst != sys.getProcesses().end(); ++inst ) {
        processInstance( *inst, instances );
    }

    eval.processDecl( instances );

    // fill internal structures
    int nInst = 0;
    for ( auto inst = instances.begin(); inst != instances.end(); ++inst, ++nInst ) {
        UTAP::template_t *tmp = inst->templ;
        ProcessInfo pi;
        pi.prio = sys.getProcPriority( inst->uid.getName().c_str() );
        pi.initial = ( static_cast< UTAP::state_t* >( tmp->init.getData() ) )->locNr;
        procs.push_back( pi );
        states.push_back( std::vector< StateInfo >( tmp->states.size() ) );
        for ( auto st = tmp->states.begin(); st != tmp->states.end(); ++st ) {
            assert( st->locNr < states.back().size() );
            StateInfo& stinf = states.back()[ st->locNr ];
            UTAP::type_t type = st->uid.getType();
            stinf.urgent = type.is( UTAP::Constants::URGENT );
            stinf.commit = type.is( UTAP::Constants::COMMITTED );
            stinf.inv = st->invariant;
            stinf.name = st->uid.getName();
        }
        for ( auto ed = tmp->edges.begin(); ed != tmp->edges.end(); ++ed ) {
            processEdge( nInst, *ed, states.back()[ ed->src->locNr ].edges );
        }
    }
    propertyId = procs.size();
    // compute required space for locations (including property automaton and error state)
    offVar = Locations::getReqSize( procs.size() + 2 );
    // add required space for variables and clocks
    size = offVar + eval.getReqSize();

    if ( eval.error )
        throw std::runtime_error( "Error when processing declarations: " + errString( eval.error ) );
}

void TAGen::initial( char* d ) {
    memset( d, 0, stateSize() );
    setData( d );
    for ( auto proc = procs.begin(); proc != procs.end(); ++proc )
        locs.set( proc - procs.begin(), proc->initial );

    eval.initial();
    if ( !evalInv() ) {
        makeErrState( d, ERR_INVARIANT );
    } else {
        eval.extrapolate();
    }
}

// set _a_ to be (_a_ && _b_)
void addConj( UTAP::expression_t& a, const UTAP::expression_t& b ) {
    if ( a.empty() )
        a = b;
    else
        a = UTAP::expression_t::createBinary( UTAP::Constants::AND, a, b, UTAP::position_t(), b.getType() );
}

bool isClockExpr( const UTAP::expression_t& e ) {
    return e.getType().isGuard() && !e.getType().isBoolean();
}

bool isDiffExpr( const UTAP::expression_t& e ) {
    assert( isClockExpr( e ) );
    assert( e.getSize() == 2 );
    return e[ 0 ].getType().is( UTAP::Constants::DIFF ) || e[ 1 ].getType().is( UTAP::Constants::DIFF );
}

TAGen::PropGuard TAGen::buildPropGuard( const std::vector< std::pair< bool, std::string > >& clause, const std::map< std::string, std::string >& defs ) {
    PropGuard g;
    for ( auto lit = clause.begin(); lit != clause.end(); ++lit ) {
        auto found = defs.find( lit->second );
        const std::string& strLit = ( found == defs.end() ) ? lit->second : found->second;  // substitute definitions

        UTAP::expression_t tmp = parseExpression( strLit.c_str(), &sys, true );
        if ( tmp.getType().isBoolean() ) {  // predicate on variables
            if ( !lit->first )  // negated literal
                tmp = UTAP::expression_t::createUnary( UTAP::Constants::NOT, tmp, UTAP::position_t(), tmp.getType() );
            addConj( g.expr, tmp );
        } else if ( isClockExpr( tmp ) ) {  // clock comparison
            if ( isDiffExpr( tmp ) ) {  // evaluator manages its own collection of all difference constraints
                eval.setClockLimits( -1, tmp );
            } else {
                if ( addCut( tmp, -1, cutClocks ) ) {   // remember the expression for slicing
                    eval.setClockLimits( -1, cutClocks.back().pos );        // adjust clock limits if necessary
                    eval.setClockLimits( -1, cutClocks.back().neg );
                }
            }
            addConj( g.expr, lit->first ? tmp : negIneq( tmp ) );
        } else {
            throw std::runtime_error( "Unsupported expression type: \"" + tmp.toString() + "\"" );
        }
    }

    if ( sys.hasErrors() ) {
        std::cerr << "Errors in LTL proposition:" << std::endl;
        for ( auto err = sys.getErrors().begin(); err != sys.getErrors().end(); ++err )
            std::cerr << *err << std::endl;
        throw std::runtime_error( "Error reading LTL formula." );
    }
    return g;
}

bool TAGen::evalPropGuard( char* d, const TAGen::PropGuard& g ) {
    setData( d );
    if ( g.expr.empty() )
        return true;
    bool ret = eval.evalBool( -1, g.expr );
    if ( eval.error )
        throw std::runtime_error( "Error while evaluating property: " + errString( eval.error ) );
    return ret;
}

std::string TAGen::showNode( char* d ) {
    int err = isErrState( d );
    if ( err ) {
        return "Error:" + errString( err );
    } else {
        std::stringstream str;
        setData( d );
        for ( unsigned int inst = 0; inst < procs.size(); ++inst )
            str << eval.getProcessName( inst ) << " = " << states[ inst ][ locs.get( inst ) ].name << ", ";
        str << "prop = " << getPropLoc( d ) << "\n";
        str << eval;
        return str.str();
    }
}

void TAGen::makeErrState( char* d, int err ) {
    assert( err != 0 );
    memset( d, 0, stateSize() );
    locs.setSource( d );
    locs.set( propertyId+1, err );
}

int TAGen::isErrState( char* d ) {
    locs.setSource( d );
    return locs.get( propertyId+1 );
}
