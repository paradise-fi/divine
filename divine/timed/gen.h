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

    typedef SuccessorList< EdgeInfo* > SuccList;

    Evaluator eval;
    UTAP::TimedAutomataSystem sys;
    unsigned int offVar, size, propertyId;
    std::vector< std::vector< StateInfo > > states;
    std::vector< ProcessInfo > procs;
    Locations locs;

    // vector used for zone slicing that is needed for deciding properties involving clocks and difference constraints
    // _cutClocks_ contains expressions involving single clock and is applied after the up operation
    // _eval.clockDiffrenceExprs_ contains clock differences and is applied after clock updates
    std::vector< Cut > cutClocks;

    // set node to work on
    void setData( char *d ) {
        locs.setSource( d );
        eval.setData( reinterpret_cast< int32_t* >( d + offVar ), locs );
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

    void listSuccessors( char* source, SuccList& succs, unsigned int& begin );

    // Slices a zone.
    // The source is read from the top of the blocklist and it is assumed that setData was called on in previously
    // Resulting sucessors are pushed into the blocklist if their invariant holds
    void slice( SuccList& list, const std::vector< Cut >& cuts );

    // Assumes the current working state is list.back() and creates one or more successors from it
    // if the error flag is set, single error state is produced instead
    void makeSucc( SuccList& list ) {
        if ( eval.error ) {
            list.setInfo( list.getLastEdge(), std::numeric_limits< int >::max(), 0 );   // maximum priority
            makeErrState( list.back(), eval.error );
            list.push_back();
        } else {
            slice( list, eval.clockDiffrenceExprs );
        }
    }

    // Assumes the current working state is list.back() and creates one or more successors from it
    void makeTimeSucc( SuccList& list ) {
        assert( !eval.error );
        slice( list, cutClocks );
    }

    int edgePrio( const EdgeInfo* e ) {
        return procs[ e->procId ].prio;
    }

public:
    class PropGuard {
        UTAP::expression_t expr;
        friend class TAGen;
    };

    TAGen() : offVar( 0 ), size( 0 ), propertyId( 0 ) {}

    unsigned int stateSize() const {
        return size;
    }

    template < typename Func >
    void genSuccs( char* source, Func callback ) {
        SuccList succs( stateSize(), sys.hasPriorityDeclaration() );
        unsigned int begin;
        listSuccessors( source, succs, begin );

        succs.for_each( callback, begin );
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

    // returns nonzero if in error state
    int isErrState( char* d );

    enum Err {
        ERR_INVARIANT = 200
    };
};
