#include "eval.h"
#include "utils.h"
#include "successorlist.h"
#include <utap/utap.h>

class TAGen {
    typedef uint16_t state_id;
    typedef int chan_id;

public:
    struct EdgeInfo {
        UTAP::expression_t guard, assign, sync;
        int syncType;   // -1 or SYNC_QUE or SYNC_BANG
        unsigned int dstId; // destination location
        int procId;

        // assign from another instance and substitute a symbol for expression in all attributes
        void assignSubst( const EdgeInfo& ei, const UTAP::symbol_t& sym, const UTAP::expression_t& expr );
    };

private:
    struct StateInfo {
        bool urgent, commit;
        UTAP::expression_t inv;
        std::vector< EdgeInfo > edges;
        std::string name;
    };

    struct ProcessInfo {
        int prio;
        unsigned int initial;
    };

    typedef std::vector< EnabledInfo< const EdgeInfo* > > EnabledList;

    Evaluator eval;
    UTAP::TimedAutomataSystem sys;
    unsigned int offVar, size, propertyId;
    std::vector< std::vector< StateInfo > > states;
    std::vector< ProcessInfo > procs;
    Locations locs;
    UTAP::expression_t auxResetExpr;

    // set node to work on
    void setData( char *d ) {
        locs.setSource( d );
        eval.setData( d + offVar, locs );
    }

    // copy given state to _succ_ and set _succ_ as the current working state
    void newSucc( char* succ, const char* source ) {
        memcpy( succ, source, stateSize() );
        setData( succ );
    }

    // applies effects of an edge to current working state
    void applyEdge( const EdgeInfo* e ) {
        locs.set( e->procId, e->dstId );
        eval.evalCmd( e->procId, e->assign );
    }

    bool evalGuard( const EdgeInfo* e ) {
        return eval.evalBool( e->procId, e->guard );
    }

    void makeErrState( char* d, int err );

    // parses edge description, produces one or more actual edges (in case of select)
    void processEdge( int procId, const UTAP::edge_t& e, std::vector< EdgeInfo >& out );

    // parses instance definition, produces one or more actual instances (in case of unbound parameters)
    void processInstance( const UTAP::instance_t& inst, std::vector< UTAP::instance_t >& out );

    bool evalInv();

    bool isUrgent( char* source );

    void listEnabled( char* source, BlockList &bl, EnabledList &einf, bool &urgent );

    /* Extrapolation correct for clock differences.
     * The idea is to cut the source zone by all clock difference constraints, extrapolate resulting zones
     * and then make sure they satsfy only those constraints they satisfied before the extrapolation.
     * _cuts_ has to contain all clock difference constraints used by the automaton, if it is empty, simple extrapolation is done
     *
     * Source:
     * Bengtsson, Johan and Yi, Wang. On Clock Difference Constraints and Termination in Reachability Analysis of Timed Automata.
     * In Formal Methods and Software Engineering. Springer Springer 2003.
     */
    template < typename Func >
    void extrapolateDiff( char* source, const std::vector< Cut >& cuts, Func fun ) {
        if ( cuts.empty() ) {   // avoid unnecessary computations when no slicing is needed
            setData( source );
            eval.extrapolate();
            fun( source );
            return;
        }

        // For performace reasons, slicing is done by traversing a tree
        // _bl_ stores our trace, _next_ tells us which branch to take
        BlockList bl( stateSize() );
        bl.push_back( source );
        std::vector< int > next({ 0 });

        while ( !next.empty() ) {
            assert( next.size() == bl.size() );
            if ( next.back() == 2 ) { // if we already completed both branches, go up
                bl.pop_back();
                next.pop_back();
            } else {
                if ( next.size() - 1 < cuts.size() ) { // process inner nodes
                    bl.duplicate_back();
                    setData( bl.back() );
                    const Cut& cut = cuts[ next.size() - 1 ];
                    next.back()++;
                    if ( eval.evalBool( cut.pId, next.back() == 1 ? cut.pos : cut.neg ) ) {
                        next.push_back( 0 );
                    } else { // if the result is empty, abandon this branch
                        bl.pop_back();
                    }
                } else { // process leaf nodes
                    setData( bl.back() );
                    eval.extrapolate();
                    // constrain this zone by all constraints it was cut by
                    for ( unsigned int i = 0; i < cuts.size(); i++ ) {
                        const Cut& cut = cuts[ i ];
                        eval.evalBool( cut.pId, next[ i ] == 1 ? cut.pos : cut.neg );
                    }
                    // emit state and go up
                    fun( bl.back() );
                    bl.pop_back();
                    next.pop_back();
                }
            }
        }
    }

    int edgePrio( const EdgeInfo* e ) {
        return procs[ e->procId ].prio;
    }

public:
    class PropGuard {
        UTAP::expression_t expr;
        friend class TAGen;

    public:
        std::string toString() const {
            return expr.toString();
        }
    };

    TAGen() : offVar( 0 ), size( 0 ), propertyId( 0 ) {}

    unsigned int stateSize() const {
        return size;
    }

    void enableLU( bool enable ) {
        eval.enableLU( enable );
    }

    template < typename Func >
    void genSuccs( char* source, Func callback ) {
        // error states and timelocks do not have any successors
        if ( getError( source ) )
            return;

        bool urgent;
        BlockList bl( stateSize(), 1 );
        EnabledList einf;
        try {
            listEnabled( source, bl, einf, urgent );
        } catch ( EvalError& e ) {
            makeErrState( bl.back(), e.getErr() ); // create error state
            callback( bl.back(), nullptr );
        }

        // finalize priorities
        PrioVal nextPrio( sys.hasPriorityDeclaration() );
        PrioVal maxPrio( false );
        if ( sys.hasPriorityDeclaration() ) { // if priorities are used, compute the highest priority
            for ( auto it = einf.begin(); it != einf.end(); it++ ) {
                assert( it->valid );
                it->prio.finalize();
                it->prio.updateMax( nextPrio );
            }
        }

        setData( source );
        Federation fed = eval.getFederation();  // create federation from source zone
        unsigned int remaining;

        // generate edge successors
        do {
            Federation current = fed;
            remaining = 0;
            maxPrio = nextPrio; // stores current maximum priority
            nextPrio = PrioVal( sys.hasPriorityDeclaration() ); // stores the highest priority lower than maxPrio
            for ( unsigned int i = 0; i < einf.size(); i++ ) {
                if ( !einf[ i ].valid ) continue;
                const EdgeInfo *edge = einf[ i ].edges[ 0 ];

                try {
                    int err = getError( bl[ i ] );
                    if ( err ) // if this is an error successor, rethrow the exception
                        throw EvalError( err );

                    if ( !einf[ i ].prio.equal( maxPrio ) ) { // if successor has lower priority
                        remaining++;
                        einf[ i ].prio.updateMax( nextPrio );
                        continue;
                    }
                    setData( bl[ i ] );
                    Federation inter = eval.zoneIntersection( current );
                    if ( inter.isEmpty() )
                        continue;
                    fed -= inter;

                    for ( auto dbm = inter.begin(); dbm != inter.end(); ++dbm ) {
                        newSucc( bl.back(), bl[ i ] );
                        eval.assignZone( dbm() );

                        for ( auto &e : einf[ i ].edges )
                            applyEdge( e );

                        makeSucc( bl.back(), [ &callback, edge ] ( const char* s ) {
                            callback( s, edge );
                        });
                    }
                } catch ( EvalError& e ) {
                    makeErrState( bl.back(), e.getErr() ); // create an error state
                    callback( bl.back(), edge );
                }
                einf[ i ].valid = false;
            }
        } while ( remaining > 0 && !fed.isEmpty() ); // quit if there is no remaining state and we still have a non-empty federation

        if ( !fed.isEmpty() ) { // if some valutions do not have outcomming edges
            if ( urgent || fed.isUnbounded() ) {
                makeErrState( bl.back(), EvalError::TIMELOCK ); // create artifical deadlock state
                callback( bl.back(), nullptr );
            }
        }
    }

    template < typename Func >
    void makeSucc( char* succ, Func callback ) {
        try {
            bool urgent = isUrgent( succ );
            setData( succ );

            if ( !evalInv() )
                return;

            if ( !urgent ) {
                eval.up();
                evalInv();
            }
        } catch ( EvalError& e ) {
            makeErrState( succ, e.getErr() ); // create error state
            callback( succ );
        }

        extrapolateDiff( succ, eval.clockDiffrenceExprs, callback );
    }

    // get location of the property automaton
    int getPropLoc( char* d ) {
        locs.setSource( d );
        return locs.get( propertyId );
    }

    // set location of the property automaton
    void setPropLoc( char* d, int loc ) {
        locs.setSource( d );
        locs.set( propertyId, loc );
    }

    // copies initial state into given buffer, the buffer has to be at least stateSize() long
    void initial( char* d );

    // reads input model
    void read( const std::string& path );

    // builds representaion of the given conjunction of literals that can be later evaluated by evalPropGuard
    PropGuard buildPropGuard( const std::vector< std::pair< bool, std::string > >& clause, const std::map< std::string, std::string >& defs );

    // evaluates guard constructed by buildPropGuard
    // if it holds, given state is not changed and true is returned, otherwise false is returned and state may have been altered
    bool evalPropGuard( char* d, const PropGuard& g );

    // returns human-readable represenation of the current state
    std::string showNode( char* d );

    // returns true for error states
    bool isErrState( char* d );

    // returns nonzero for error states or timelocks
    int getError( char* d );

    void addAuxClock();

    void resetAuxClock();

    std::pair< PropGuard, PropGuard > addAuxToGuard( const PropGuard& guard );
};
