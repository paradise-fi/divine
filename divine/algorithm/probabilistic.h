// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h> // for stats
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>
#include <divine/probabilistictransition.h>
#include <algorithm>

#include <climits>

#include <lpsolve/lp_lib.h>

#ifndef DIVINE_ALGORITHM_PROBABILISTIC_H
#define DIVINE_ALGORITHM_PROBABILISTIC_H

namespace divine {
namespace algorithm {

template< typename > struct Probabilistic;
typedef unsigned SCCId;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G >
struct _MpiId< Probabilistic< G > >
{
    typedef Probabilistic< G > A;

    static int to_id( void (A::*f)() ) {
        if ( f == &A::_fwd ) return 0;
        if ( f == &A::_init ) return 1;
        if ( f == &A::_pivot ) return 2;
        if ( f == &A::_bwd ) return 3;
        if ( f == &A::_originalRange ) return 4;
        if ( f == &A::_rangeEmpty ) return 5;
        if ( f == &A::_owcty ) return 6;
        if ( f == &A::_fwdseeds ) return 7;
        if ( f == &A::_contrafwdseeds ) return 8;
        if ( f == &A::_rangeNotChanged ) return 9;

        if ( f == &A::_initAEC ) return 10;
        if ( f == &A::_closure ) return 11;
        if ( f == &A::_isSame ) return 12;
        if ( f == &A::_isEmpty ) return 13;
        if ( f == &A::_lreachability ) return 14;

        if ( f == &A::_appendToPOne ) return 15;
        if ( f == &A::_minimalGraphIdentification ) return 16;
        if ( f == &A::_finalSeeds ) return 17;
        if ( f == &A::_collect ) return 18;

        if ( f == &A::_setIdOfSCC ) return 19;
        if ( f == &A::_collectSCCs ) return 20;
        if ( f == &A::_computeProbability ) return 21;
        if ( f == &A::_collectEmptySCCs ) return 22;
        if ( f == &A::_constraintSCCs ) return 26;

        if ( f == &A::_reinitOBF ) return 27;
        if ( f == &A::_postOBF ) return 28;
        if ( f == &A::_poneSize ) return 29;
        if ( f == &A::_markBorderStates ) return 30;

        if ( f == &A::_removeSCCs ) return 31;
        if ( f == &A::_restoreSeeds ) return 32;
        if ( f == &A::_addReadySlices ) return 33;
        if ( f == &A::_prepareNewSlices ) return 34;

        assert_die();
    }

    static void (A::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_fwd;
            case 1: return &A::_init;
            case 2: return &A::_pivot;
            case 3: return &A::_bwd;
            case 4: return &A::_originalRange;
            case 5: return &A::_rangeEmpty;
            case 6: return &A::_owcty;
            case 7: return &A::_fwdseeds;
            case 8: return &A::_contrafwdseeds;
            case 9: return &A::_rangeNotChanged;

            case 10: return &A::_initAEC;
            case 11: return &A::_closure;
            case 12: return &A::_isSame;
            case 13: return &A::_isEmpty;
            case 14: return &A::_lreachability;

            case 15: return &A::_appendToPOne;
            case 16: return &A::_minimalGraphIdentification;
            case 17: return &A::_finalSeeds;
            case 18: return &A::_collect;

            case 19: return &A::_setIdOfSCC;
            case 20: return &A::_collectSCCs;
            case 21: return &A::_computeProbability;
            case 22: return &A::_collectEmptySCCs;
            case 26: return &A::_constraintSCCs;

            case 27: return &A::_reinitOBF;
            case 28: return &A::_postOBF;
            case 29: return &A::_poneSize;
            case 30: return &A::_markBorderStates;

            case 31: return &A::_removeSCCs;
            case 32: return &A::_restoreSeeds;
            case 33: return &A::_addReadySlices;
            case 34: return &A::_prepareNewSlices;

            default: assert_die();
        }
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        *o++ = s.initialTable;
        *o++ = s.offset;
        *o++ = s.pivotId;
        *o++ = s.flag;
        *o++ = s.size;
        *o++ = s.testGroup;
        *o++ = s.iterativeOptimization;

        *o++ = s.visitedSCCs.size();
        for ( std::map< SCCId, unsigned >::const_iterator it = s.visitedSCCs.begin(); it != s.visitedSCCs.end(); ++it ) {
            *o++ = it->first;
            *o++ = it->second;
        }

        *o++ = s.emptySCCs.size();
        for ( std::set< SCCId >::const_iterator it = s.emptySCCs.begin(); it != s.emptySCCs.end(); ++it )
            *o++ = *it;

        *o++ = s.toProcessSCCs.size();
        for ( std::vector< std::pair< SCCId, unsigned > >::const_iterator it = s.toProcessSCCs.begin(); it != s.toProcessSCCs.end(); ++it ) {
            *o++ = it->first;
            *o++ = it->second;
        }

        assert( sizeof( unsigned ) == 4 && sizeof( REAL ) == 8 ); // FIXME make this more universal
        *o++ = *reinterpret_cast< unsigned* >( &s.probability );
        *o++ = *( reinterpret_cast< unsigned* >( &s.probability ) + 1 );

        s.stats.write( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        s.initialTable = *i++;
        s.offset = *i++;
        s.pivotId = *i++;
        s.flag = *i++;
        s.size = *i++;
        s.testGroup = *i++;
        s.iterativeOptimization = *i++;

        unsigned visitedSCCsSize = *i++;
        s.visitedSCCs.clear();
        for ( unsigned j = 0; j < visitedSCCsSize; j++ ) {
            const SCCId id = *i++;
            s.visitedSCCs[ id ] = *i++;
        }

        unsigned emptySCCsSize = *i++;
        s.emptySCCs.clear();
        for ( unsigned j = 0; j < emptySCCsSize; j++ )
            s.emptySCCs.insert( *i++ );

        unsigned toProcessSCCsSize = *i++;
        s.toProcessSCCs.clear();
        for ( unsigned j = 0; j < toProcessSCCsSize; j++ ) {
            const SCCId id = *i++;
            s.toProcessSCCs.push_back( std::make_pair( id, *i++ ) );
        }

        assert( sizeof( unsigned ) == 4 && sizeof( REAL ) == 8 ); // FIXME make this more universal
        *reinterpret_cast< unsigned* >( &s.probability ) = *i++;
        *( reinterpret_cast< unsigned* >( &s.probability ) + 1 ) = *i++;

        return s.stats.read( i );
    }
};
// END MPI drudgery

/**
 * Implements OWCTY-BWD-FWD -- OBF algorithm (OBFR-P variant) published in 
 * Distributed Algorithms for SCC Decomposition (by Barnat, Chaloupka, van de Pol) 
 * with extension for AEC detection based on Approximation Set Algorithm 
 * as published in Distributed Qualitative LTL Model Checking of Markov Decision Processes 
 * (by Barnat, Brim, Cerna, Ceska, Tumova).
 */
template< typename G >
struct Probabilistic : virtual Algorithm, AlgorithmUtils< G >, DomainWorker< Probabilistic< G > >
{
    typedef Probabilistic< G > This;
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;
    typedef typename G::Graph::ProbabilisticTransitions ProbabilisticTransitions;
    typedef typename G::Graph::Table Table;

    struct Shared {
        algorithm::Statistics< G > stats;
        G g;
        unsigned offset; // bitset tables offset
        unsigned pivotId; // current pivot
        int initialTable; 
        bool flag; // bit flag
        unsigned size; // accumulative sum of sizes
        unsigned testGroup; // current pair/set of acceptance condition
        std::map< SCCId, unsigned > visitedSCCs; // map of sizes of visited SCCs
        std::set< SCCId > emptySCCs; // set of empty SCCs
        std::vector< std::pair< SCCId, unsigned > > toProcessSCCs; // vector of SCCs (and sizes) ready to be solved
        REAL probability; // final probability
        bool iterativeOptimization; // use iterative optimization?
        bool onlyQualitative; // do only qualitative analysis
    } shared;

    /// Represents a SCC: stores a representant state id and number of unsolved outgoing transitions
    struct SCC {
        unsigned states;
        StateId representant;
        unsigned outTransitions;

        SCC( const StateId id ) : states( 0 ), representant( id ), outTransitions( 0 ) {}

        SCC() {} // default constructor for map
    };

    /// Reindexes states for LP_SOLVE so that they are consecutively numbered
    struct StateIdTranslator {
        bool wholeGraph; // whole Minimal Graph, ignore SCCs
        This* parent;
        std::map< SCCId, std::map< StateId, unsigned > > stateIdTranslation; // maps state ids of subset of states to consecutive numbers
        std::map< SCCId, unsigned > lastUsedIds; // helps to assign new state ids

        StateIdTranslator() : wholeGraph( true ), parent( NULL ) {}

        void useSCCs( bool use = true ) { wholeGraph = !use; }

        void setParent( This* parent ) { this->parent = parent; }

        bool empty() {
            return stateIdTranslation.size() == 0 && lastUsedIds.size() == 0;
        }

        void clear() {
            stateIdTranslation.clear();
            lastUsedIds.clear();
        }

        /// Main translation method
        unsigned translate( const StateId id, SCCId compId = 0 ) {
            if ( wholeGraph ) compId = 0;
            assert( wholeGraph || compId );
            std::map< StateId, unsigned >& sccMap = stateIdTranslation[ compId ];
            const std::map< StateId, unsigned >::const_iterator mit = sccMap.find( id );
            if ( mit != sccMap.end() )
                return mit->second;
            return sccMap[ id ] = ++lastUsedIds[ compId ];
        }

        unsigned translate( const Node n, SCCId compId = 0 ) { 
            assert( parent != NULL );

            return translate( parent->shared.g.base().getStateId( n ), compId ? compId : parent->extension( n ).componentId );
        }
    };

    std::deque< Successors > localqueue; // queue holding unprocesses states
    map< unsigned, unsigned > originalRangeSize; // original size of set RANGE
    map< unsigned, unsigned > oldSizes; // original size of Approximation graph
    std::vector< Table* > tables; // different sets of vertices
    lprec **lp; // list of linear programming handles
    SCCId lastId; // last used SCC id
    std::map< SCCId, SCC > visitedSCCs; // map of visited SCCs
    std::map< SCCId, unsigned > sizeSCCs; // map of SCCs sizes
    std::map< SCCId, std::vector< StateId > > minimizeStates; // minimize probability of which states (these states have incoming transitions coming from outside the SCC)
    std::set< SCCId > simpleSCCs; // set of SCCs having only one state
    bool lp_started; // linear programming task started
    bool foundAEC; // at least one AEC has been found
    bool useInitial; // use initial state for forward reachability
    bool initialInsideAEC; // initial state belongs to some AEC
    StateIdTranslator translator; // remaps state ids
    unsigned lastSliceId; // holds last assigned slice id
    std::set< unsigned > readySlices; // new unprocessed slices created by OBF
    std::map< unsigned, unsigned > originalRangeSliceCount; // original sizes of RANGE sets indexed by slice ids
    std::set< unsigned > activeSlices; // set of active slices in OBF
    bool simpleOutput; // !verbose output

    /// Every OBFR call requires additional OFFSETDELTA sets of states
    #define OFFSETDELTA 3

    #define STARTINGOFFSET 5

// #define OBFFIRST // TODO enable this if you want the OBF algorithm to run first

    enum TableId {
        V, RANGE, SEEDS, B, // Every OBFR call requires these sets (V corresponds to B of previous call)
        TEMP, REACHED, ELIMINATE, // These tables are shared across all calls
        PONE, // This set stores states with probability 1 (they belong to some AEC)
        READY // This set holds states which are in unprocessed slices
    };

    /// Stores either partial probabilities of successors or final probability of a state
    union Probability {
        REAL *list;
        REAL final;
    };

    /// Algorithm Extension structure
    struct Extension {
        unsigned sliceId:29; // id of OBF slice this state belongs to
        bool hasSelfLoop:1; // self loop on the state has been found
        unsigned eliminateState:2; // state elimination state (AEC closure operation)
        unsigned count:30; // how many predecessors/successors have been found/enabled
        bool hasBackwardTransitionOutsideSCC:1; // state has backward transition to some other state outside of his SCC
        bool hasFinalProbability:1; // final probability of this state has been determined
        ProbabilisticTransition probabilisticTransition; // when state represents a particular successor then this holds information about the transition
        SCCId componentId; // id of SCC this state belongs to, 0 if this state forms only a trivial SCC
        Probability probability; // calculated probability
    };

    /// Returns algorithm's extension of state
    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

/****************************************************************/
/* Common functions                                             */
/****************************************************************/

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

    int owner( Node n, hash_t hint = 0 ) {
        return shared.g.owner( hasher, *this, n, hint );
    }

    Successors predecessors( Node n ) {
        return shared.g.base().predecessors( n );
    }

    Successors successors( Node n ) {
        return shared.g.successors( n );
    }

    ProbabilisticTransitions probabilisticTransitions( Node n ) {
        return shared.g.base().probabilisticTransitions( n );
    }

	/// Creates a half empty pair FIXME this is just a temporary hack for merging
	static BlobPair createPair( Node n ) {
		return std::make_pair( n, Blob() );
	}

	static Node extractFromPair( BlobPair pair ) {
		return pair.first;
	}

    /// Queue state n to be processed by peer targetPeer
    void queueAny( const unsigned targetPeer, Node n ) {
        this->comms().submit( this->globalId(), targetPeer, createPair( n ) );
    }

    /// Sends edge to be processed by to owner
    void edge( Node from, Node to, bool ( This::*handleEdge )( Node, Node ) ) {
        int owner = this->owner( to );

        if ( owner != this->globalId() ) { // send to remote
            queueAny( owner, shared.g.copyState( from ) );
            queueAny( owner, to );
        } else { // we own this node, so let's process it
            assert( from.valid() );
            ( this->*handleEdge )( from, to );
        }
    }

    /// Retrieves an expected state from from's queue
    Node retrieve( int from ) {
        while ( !this->comms().pending( from, this->globalId() ) );
        return extractFromPair( this->comms().take( from, this->globalId() ) );
    }

    /// Who is supposed to solve LP task containing state n
    unsigned collectOwner( Node n ) {
        if ( !shared.iterativeOptimization ) return 0;

        // when iterative optimization is on, the aggregation of states is based on componentId
        unsigned sccsPerPeer = shared.toProcessSCCs.size() / this->peers();
        if ( sccsPerPeer == 0 ) return this->peers() - 1;
        for ( unsigned c = 0; c < shared.toProcessSCCs.size(); c++ ) {
            if ( shared.toProcessSCCs[ c ].first == extension( n ).componentId ) {
                unsigned peerId = c / sccsPerPeer;
                if ( peerId > this->peers() - 1 ) peerId = this->peers() - 1;
                return peerId;
            }
        }
        assert_die();
    }

    /**
     * Collects several states from from's queue. This method is used for aggregating 
     * information about forward transitions to a single peer so that LP task can be solved.
     */
    void visitCollect( int from, Node f ) {
        assert_eq( shared.g.base().getStateId( f ), 0 ); // 0 prefix signifies this is not a normal edge
        shared.g.release( f );

        f = retrieve( from );
        unsigned owner = collectOwner( f );
        assert( this->globalId() == owner );
        if ( extension( f ).count ) {
            std::vector< Node > succs( extension( f ).count );
            for ( unsigned i = 0; i < extension( f ).count; i++ )
                succs[ i ] = retrieve( from );
            lp_addUncertain( f, succs );
        } else
            lp_addCertain( f );
        shared.g.release( f );
    }

    /// Count of SCCs solved by this peer
    unsigned peerCountSCC() {
        const unsigned sccsPerPeer = shared.toProcessSCCs.size() / this->peers();
        unsigned count = sccsPerPeer;
        if ( this->globalId() == this->peers() - 1 )
            count += shared.toProcessSCCs.size() - this->peers() * sccsPerPeer;
        return count;
    }

    /// Starting index of SCC solved by this peer (in vector shared.toProcessSCCs)
    unsigned peerFromSCC() {
        const unsigned sccsPerPeer = shared.toProcessSCCs.size() / this->peers();
        return this->globalId() * sccsPerPeer;
    }

    /// Index of SCC of state n in the SCC vector shared.toProcessSCCs
    unsigned lp_sccIndex( Node n ) {
        if ( !shared.iterativeOptimization ) return 0;

        const unsigned compId = extension( n ).componentId;
        const unsigned sccsPerPeer = shared.toProcessSCCs.size() / this->peers();
        const unsigned from = this->globalId() * sccsPerPeer;
        const unsigned to = from + peerCountSCC();
        for ( unsigned i = from; i < to; i++ )
            if ( compId == shared.toProcessSCCs[ i ].first )
                return i - from;

        assert_die();
    }

    /// Enqueues state with id 0 (a nonexistent state) to let the recepient of the queue know that all expanded successors will follow.
    void queueCollectPrefix( const unsigned targetPeer ) {
        queueAny( targetPeer, shared.g.base().getAnyState( 0 ) ); // 0 prefix signifies this is not a normal edge
    }

    /**
     * Aggregates forward transitions of succs.from() and sends them to the peer owner of its SCC,
     * LP task is solved afterwards.
     */
    void visitLocalCollect( Successors succs ) {
        std::vector< Node > succsv;
        const Node from = succs.from();
        if ( getTable( PONE ).has( from ) ) extension( from ).count = 0;
        assert( getTable( ELIMINATE ).has( from ) ); // from should be in the Minimal Graph

        const unsigned owner = collectOwner( from );
        if ( owner != this->globalId() ) {
            queueCollectPrefix( owner );
            queueAny( owner, shared.g.copyState( from ) );
        }
        unsigned foundCount = 0;
        if ( !getTable( PONE ).has( from ) ) {
            ProbabilisticTransitions probTrans = probabilisticTransitions( from );
            while ( !succs.empty() ) {
                assert( !probTrans.empty() );
                if ( succs.headTransition().flag ) { // Transition is supposed to be followed
                    foundCount++;
                    Node succ = succs.head();
                    Node copy = shared.g.copyState( succ );
                    shared.g.release( succ );
                    extension( copy ).probabilisticTransition = probTrans.head();
                    if ( owner != this->globalId() )
                        queueAny( owner, copy );
                    else
                        succsv.push_back( copy );
                }
                succs = succs.tail();
                probTrans = probTrans.tail();
            }
            assert( probTrans.empty() );
        }
        assert_eq( foundCount, extension( from ).count );

        // append to the LP task
        if ( owner == this->globalId() ) {
            if ( succsv.size() )
                lp_addUncertain( from, succsv );
            else {
                assert( getTable( PONE ).has( from ) );
                lp_addCertain( from );
            }
        }
    }

    /// Iteratively process incoming edges and submit unprocessed states from localqueue
    void visit( bool ( This::*handleEdge )( Node, Node ), bool collect = false ) {
        do {
            // process incoming stuff from other workers
            for ( int from = 0; from < this->peers(); ++from ) {
                while ( this->comms().pending( from, this->globalId() ) ) {
                    Node f = extractFromPair( this->comms().take( from, this->globalId() ) );
                    assert( f.valid() );

                    if ( collect && shared.g.base().getStateId( f ) == 0 ) // 0 prefix signifies this is not a normal edge
                        visitCollect( from, f );
                    else {
                        bool updateProbability = false;
                        if ( shared.g.base().getStateId( f ) == static_cast< unsigned >( -1 ) ) { // FIXME hack
                            shared.g.release( f );
                            f = Node();
                            updateProbability = true;
                        }
                        Node to = retrieve( from );
                        assert_eq( owner( to ), this->globalId() );
                        Node t = shared.g.base().getState( shared.g.base().getStateId( to ) );
                        if ( updateProbability ) {
                            assert( extension( to ).probability.final != 0 );
                            assert( extension( t ).probability.final == 0 || extension( t ).probability.final == extension( to ).probability.final ); 
                            extension( t ).probability.final = extension( to ).probability.final;
                        }
                        shared.g.release( to );
                        ( this->*handleEdge )( f, t );
                        if ( f.valid() ) shared.g.release( f );
                    }
                }
            }

            // process local queue
            while ( !localqueue.empty() ) {
                Successors &succ = localqueue.front();
                if ( collect ) visitLocalCollect( succ );
                while ( !succ.empty() ) {
                    edge( succ.from(), succ.head(), handleEdge );
                    succ = succ.tail();
                }
                shared.g.release( succ.from() );
                localqueue.pop_front();
            }
        } while ( !this->idle() ); // until termination detection succeeds
    }

    /// Process states from set fromTable
    bool processInitials( TableId fromTable, bool ( This::*handleEdge )( Node, Node ), bool useCopy = false ) {
        Table *table, t;
        if ( useCopy ) {
            t = getTable( fromTable );
            table = &t;
        } else
            table = &getTable( fromTable );
        while ( table->statesCount() ) {
            Node seed = table->get( table->lowestState() ); 
            table->remove( table->lowestState() );
            assert( owner( seed ) == this->globalId() );
            if ( ( this->*handleEdge )( Node(), seed ) ) return false; // we can stop processing
        }
        return true;
    }

    /// If we own the initial state, let's process it
    void processInitial( bool ( This::*handleEdge )( Node, Node ) ) {
        Node initial = shared.g.base().initial();
        if ( shared.g.base().owner( initial ) == this->globalId() )
            ( this->*handleEdge )( Node(), initial );
        else
            shared.g.release( initial );
    }

    /// Returns reference to a table
    Table& getTable( TableId tid ) {
        return tid >= TEMP ? *tables[ tid - OFFSETDELTA - 1 ] : *tables[ shared.offset + tid ];
    }

    /// Copies current table tid2 to current table tid1
    void setTableCopy( TableId tid1, TableId tid2 ) { setTableCopy( tid1, &getTable( tid2 ) ); }

    /// Copies table to current table tid
    void setTableCopy( TableId tid, Table* table ) {
        assert( table );
        Table* t = new Table( *table );
        setTable( tid, t );
    }

    /// Sets current table tid2 to be current table tid1
    void setTable( TableId tid1, TableId tid2 ) { 
        setTable( tid1, &getTable( tid2 ) ); 
        nullTable( tid2 );
    }

    /// Sets table to be current table tid
    void setTable( TableId tid, Table* table ) {
        unsigned offset;
        if ( tid >= TEMP )
            offset = tid - OFFSETDELTA - 1;
        else
            offset = shared.offset + tid;
        if ( tables[ offset ] != NULL ) delete tables[ offset ];
        tables[ offset ] = table;
    }

    /// Removes pointer to current table tid
    void nullTable( TableId tid ) {
        unsigned offset;
        if ( tid >= TEMP )
            offset = tid - OFFSETDELTA - 1;
        else
            offset = shared.offset + tid;
        tables[ offset ] = NULL;
    }

    /// Switches two tables pointers
    void switchTables( TableId table1, TableId table2 ) {
        Table* t = &getTable( table1 );
        nullTable( table1 );
        setTable( table1, table2 );
        nullTable( table2 );
        setTable( table2, t );
    }

    /// Sets table t to be used by the current distributed function
    void setAlgorithmTable( Table* t ) {
        this->m_table = t;
    }

    /// Prints current statistics
    void printStats() {
        shared.stats = algorithm::Statistics< G >();
        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            shared.stats.merge( s.stats );
        }

        shared.stats.print( progress() );
    }

/****************************************************************/
/* OBFR functions                                               */
/****************************************************************/

    /// Handles processing of transition during forward reachability
    bool fwdEdge( Node from, Node to ) {
        reachabilityEdge( from, to, V, &This::successors, true );
        return false;
    }

    /// Handles processing of transition during backward reachability
    bool bwdEdge( Node from, Node to ) {
        reachabilityEdge( from, to, RANGE, &This::predecessors );
        return false;
    }

    /// Handles processing of new state during reachability
    void reachabilityEdge( Node from, Node to, TableId scopeTable, Successors ( This::*succs )( Node ), bool countPreds = false ) {
        if ( !getTable( scopeTable ).has( to ) ) return;

        bool had = this->table().has( to );

        if ( countPreds && !had ) { // forward reachability & to has not been visited
            if ( from.valid() && extension( from ).sliceId != shared.visitedSCCs[ extension( to ).sliceId ] ) return; // out of slice
            getTable( TEMP ).remove( to, extension( to ).sliceId ); // mark it visited
            extension( to ).sliceId = shared.visitedSCCs[ extension( to ).sliceId ]; // update slice id
        } else { // ( !countPreds || had )
            if ( from.valid() && extension( from ).sliceId != extension( to ).sliceId ) return; // out of slice
            if ( !had ) getTable( TEMP ).remove( to, extension( to ).sliceId ); // mark it visited
        }

        if ( countPreds ) { // forward reachability
            if ( !from.valid() )
                extension( to ).count = 0; // reset number of predecessors
            else {
                if ( !had ) extension( to ).count = 0;
                if ( shared.g.base().getStateId( to ) == shared.g.base().getStateId( from ) )
                    extension( to ).hasSelfLoop = true; // self loops do not count into number of predecessors
                else
                    extension( to ).count++;
            }
        }

        if ( !had ) { // store it and continue with reachability
            this->table().insert( to, extension( to ).sliceId );
            localqueue.push_back( ( this->*succs )( to ) );
        }
    }

    /** 
     * OBFR function
     *
     * Forward reachability:
     * 
     * From pivots in the set SEEDS starts a forward reachability within set V.
     * Discovered states are stored in the set RANGE. RANGE is subtracted from 
     * the final set V. States discovered in one forward reachability form a new
     * OBF slice.
     */
    void _fwd() { // parallel: RANGE = FWD( pivot, V ), V = TEMP = V \ RANGE, SEEDS = pivot
        this->restart();

        shared.stats = algorithm::Statistics< G >();

        setAlgorithmTable( &getTable( RANGE ) );
        setTableCopy( TEMP, V );
        
        processInitials( SEEDS, &This::fwdEdge, true );
        visit( &This::fwdEdge );

        setTable( V, TEMP );
        setAlgorithmTable( NULL );
    }

    /**
     * OBFR function
     *
     * Backward reachability:
     * 
     * From states in the set REACHED starts a backward reachability within set RANGE.
     * Discovered states are stored in set B. B is subtracted from the final set RANGE.
     */
    void _bwd() { // parallel, B = BWD( REACHED, RANGE ), RANGE = TEMP = RANGE \ B
        this->restart();

        getTable( B ).clear();
        setAlgorithmTable( &getTable( B ) );
        setTableCopy( TEMP, RANGE );

        processInitials( REACHED, &This::bwdEdge );
        visit( &This::bwdEdge );

        setTable( RANGE, TEMP );
        setAlgorithmTable( NULL );
    }
    
    /// Forward reachability
    void fwd( const unsigned iteration ) {
        assert( !shared.visitedSCCs.size() );

        for ( std::set< unsigned >::const_iterator it = readySlices.begin(); it != readySlices.end(); ++it ) { // for all prepared slices
            if ( shared.emptySCCs.count( *it ) ) continue; // if no pivot has been found, we do not need a new id
            const unsigned newSliceId = iteration ? ++lastSliceId : *it;
            activeSlices.insert( newSliceId );
            shared.visitedSCCs[ *it ] = newSliceId; // assign new ids to new slices
        }
        domain().parallel().run( shared, &This::_fwd );

        shared.visitedSCCs.clear();
    }
    
    /// Backward reachability
    void bwd() { domain().parallel().run( shared, &This::_bwd ); }

    //-------------------------

    /// Handles processing of transition during forward seeds 
    bool fwdSeedsEdge( Node from, Node to ) {
        seedsEdge( from, to, B, RANGE );
        return false;
    }

    /// Handles processing of transition during forward seeds in opposite direction
    bool contraFwdSeedsEdge( Node from, Node to ) {
        seedsEdge( from, to, RANGE, B );
        return false;
    }

    /// Handles processing of new state during seeds
    void seedsEdge( Node from, Node to, TableId fromTable, TableId toTable ) {
        if ( !from.valid() )
            localqueue.push_back( shared.g.successors( to ) );
        else {
            if ( extension( from ).sliceId != extension( to ).sliceId ) return; // different obf slices
            if ( !getTable( toTable ).has( to ) || getTable( fromTable ).has( to ) ) return; // out of scope
            extension( to ).count--;

            if ( this->m_table ) this->table().insert( to );
        }
    }

    /**
     * OBFR function
     *
     * Forward seeds:
     *
     * Finds direct successors of states in B not in B and stores them
     * in the set SEEDS. Updates predecessor count.
     */
    void _fwdseeds() {
        this->restart();

        getTable( SEEDS ).clear();
        setAlgorithmTable( &getTable( SEEDS ) );

        processInitials( B, &This::fwdSeedsEdge, true );
        visit( &This::fwdSeedsEdge );

        setAlgorithmTable( NULL );
    }

    /**
     * OBFR function
     *
     * Forward seeds in opposite direction:
     *
     * Finds direct successors of states in RANGE that are in B and 
     * updates their predecessor count.
     */
    void _contrafwdseeds() {
        this->restart();

        assert( this->m_table == NULL );

        processInitials( RANGE, &This::contraFwdSeedsEdge, true );
        visit( &This::contraFwdSeedsEdge );
    }
    
    /// Seeds
    void seeds() {
        domain().parallel().run( shared, &This::_fwdseeds );
        domain().parallel().run( shared, &This::_contrafwdseeds );
    }

    //------------------------------

    /// Handles processing of edge during OWCTY elimination.
    bool owctyEdge( Node from, Node to ) {
        if ( ( from.valid() && extension( from ).sliceId != extension( to ).sliceId ) || // out of slice
             !getTable( RANGE ).has( to ) || // out of scope
             shared.flag // we no longer need to continue in this case (AEC or initial inside AEC has been found)
           ) return false;

        if ( !getTable( REACHED ).has( to ) ) getTable( REACHED ).insert( to ); // state to has been reached

        if ( from.valid() ) { // decrease predecessors (we have proceeded from the predecessor which means it has been eliminated)
            assert( extension( to ).count > 0 );
            extension( to ).count--;
        }

        if ( extension( to ).count == 0 ) { // eliminate to
            if ( extension( to ).hasSelfLoop ) { // oh, we are a nontrivial SCC
                // hack to make it harder for ids to overlap without a need for synchronization
                extension( to ).componentId = ++lastId + ( 1 + this->globalId() ) * ( UINT_MAX / ( 1 + this->peers() ) );
                simpleSCCs.insert( extension( to ).componentId ); // just one state SCC

                // now we should determine if we are closed on forward probabilistic transitions (could we be an AEC?), that is can we force a return to state to
                Successors succs = successors( to );
                ProbabilisticTransitions probTrans = probabilisticTransitions( to );
                const StateId toId = shared.g.base().getStateId( to );
                unsigned actionId;
                bool probActionStarted = false;
                bool found = false;
                bool foundOther = false;
                bool foundMe = false;
                while ( !succs.empty() ) {
                    assert( !probTrans.empty() );

                    if ( probTrans.head().isProbabilistic ) {
                        if ( !probActionStarted ) {
                            foundOther = false;
                            foundMe = false;
                            probActionStarted = true;
                            actionId = probTrans.head().id;
                        }
                        if ( probTrans.head().id != actionId ) {
                            if ( !foundOther && foundMe ) {
                                found = true;
                                break;
                            } else {
                                foundOther = false;
                                foundMe = false;
                                actionId = probTrans.head().id;
                            }
                        }
                        if ( !foundOther && probTrans.head().id == actionId ) {
                            if ( probTrans.head().weight > 0 ) {
                                if ( succs.headTransition().id == toId )
                                    foundMe = true;
                                else
                                    foundOther = true;
                            }
                        }
                    } else {
                        if ( probActionStarted ) {
                            if ( !foundOther && foundMe ) {
                                found = true;
                                break;
                            }
                            probActionStarted = false;
                        }
                        if ( succs.headTransition().id == toId ) {
                            found = true;
                            break;
                        }
                    }
                    succs = succs.tail();
                    probTrans = probTrans.tail();
                }
                if ( found ) { // we are closed under forward probabilistic transitions
                    for ( unsigned g = 0; g < shared.g.acceptingGroupCount(); g++ ) { // are we an accepting non-rejecting state?
                        if ( shared.g.isInAccepting( to, g ) && !shared.g.isInRejecting( to, g ) ) { // we are an AEC
                            appendToPOneEdge( Node(), to ); // it has probability == 1
                            
                            // eliminate to
                            getTable( REACHED ).remove( to );
                            getTable( RANGE ).remove( to, extension( to ).sliceId );
                            
                            shared.flag = shared.onlyQualitative || shared.g.base().getStateId( to ) == shared.g.base().initialState;
                            return shared.flag;
                        }
                    }
                }
            }
            // eliminate to
            getTable( REACHED ).remove( to );
            getTable( RANGE ).remove( to, extension( to ).sliceId );
            localqueue.push_back( shared.g.successors( to ) ); // continue with owcty elimination
        }
        return false;
    }

    /**
     * OBFR function
     *
     * OWCTY:
     *
     * From states of SEEDS starts OWCTY elimination of states -- eliminates
     * states with no predecessors and updates predecessors count. Stores direct
     * successors of eliminated states (which were not eliminated too) in the set
     * REACHED.
     */
    void _owcty() {
        this->restart();

        getTable( REACHED ).clear();
        // if we discover AEC (qualitative) or the initial state inside an AEC (quantitative), we can stop sooner
        if ( processInitials( SEEDS, &This::owctyEdge ) )
            visit( &This::owctyEdge );
    }

    /// OWCTY elimination
    bool owctyElimination() {
        shared.flag = false;
        domain().parallel().run( shared, &This::_owcty );
        for ( unsigned p = 0; p < this->peers(); p++ )
            if ( domain().shared( p ).flag ) return true;
        return false; // is true if we found an AEC (qualitative analysis) or the initial state inside an AEC (quantitative analysis)
    }

    //-------------------

    /**
     * OBFR function
     *
     * Initializes some sets.
     */
    void _init() {
        assert( shared.offset == STARTINGOFFSET );
        this->initPeer( &shared.g, &shared.initialTable, this->globalId() );
        this->table().visitAll();

        tables.resize( STARTINGOFFSET + 1 + OFFSETDELTA );
        setTable( V, &this->table() );

        setTable( REACHED, this->makeTable() );
        setTable( ELIMINATE, this->makeTable() );
        nullTable( TEMP ); //xx
        setTable( PONE, this->makeTable() );
        setTable( READY, this->makeTable() );

        setTable( RANGE, this->makeTable() );
        setTable( SEEDS, this->makeTable() );
        setTable( B, this->makeTable() );
    }

    //-------------------

    /// Finds pivots for active slices inside the set V.
    bool findPivot() {
        shared.emptySCCs = activeSlices;
        domain().parallel().runInRing( shared, &This::_pivot );
        const bool found = shared.emptySCCs.size() != activeSlices.size();

        if ( !found ) shared.emptySCCs.clear(); // cleanup, if we haven't found any pivot
        // shared.emptySCCs is empty or contains ids of slices without pivot, used in fwd()
        return found;
    }

    /**
     * OBFR function
     *
     * Looks up a pivots.
     */
    void _pivot() {
        getTable( SEEDS ).clear();

        if ( shared.emptySCCs.size() == 1 && shared.emptySCCs.count( 0 ) && getTable( V ).statesCount() )
            processInitial( &This::pivotEdge );
        else if ( shared.emptySCCs.size() )
            processInitials( V, &This::pivotEdge, true );
    }

    bool pivotEdge( Node from, Node to ) {
        assert( !from.valid() );

        if ( shared.emptySCCs.count( extension( to ).sliceId ) ) { // pivot for this slice hasn't been found yet
            shared.emptySCCs.erase( extension( to ).sliceId ); // well, found one
            getTable( SEEDS ).insert( to ); // seeds for following forward reachability
            getTable( READY ).insert( to ); // backup all found pivots to READY, this will be later copies back to SEEDS for OWCTY
        }

        return shared.emptySCCs.size() == 0; // if we found pivots for all active slices, we can stop sooner
    }

    //-------------------

    /// Saves a size of the RANGE set
    void storeRangeSize() { domain().parallel().runInRing( shared, &This::_originalRange ); }

    /**
     * OBFR function
     *
     * Stores sizes of slices from the original RANGE set
     */
    void _originalRange() {
        for ( std::map< unsigned, unsigned >::const_iterator mit = getTable( RANGE ).sliceCount.begin(); mit != getTable( RANGE ).sliceCount.end(); ++mit ) {
            if ( originalRangeSliceCount[ mit->first ] == 0 ) { // this is a new slice
                originalRangeSliceCount[ mit->first ] = mit->second;
//                 else
//                     rangeSliceCount.erase( mit->first ); // TODO we might want to erase old slices
            }
//         originalRangeSize[ shared.offset ] = getTable( RANGE ).statesCount();
        }
    }

    //-------------------

    /**
     * OBFR function
     *
     * Tests if all slices from the current RANGE set are empty
     */
    void _rangeEmpty() {
        for ( std::map< unsigned, unsigned >::const_iterator mit = getTable( RANGE ).sliceCount.begin(); mit != getTable( RANGE ).sliceCount.end(); ++mit )
            if ( mit->second ) shared.emptySCCs.erase( mit->first ); // mit->first slice is nonempty
        shared.flag &= getTable( RANGE ).statesCount() == 0;
    }

    /// Returns true iff all slices from the current RANGE set are empty
    bool rangeEmpty() {
        shared.flag = true;
        shared.emptySCCs = activeSlices; // test all active slices
        domain().parallel().runInRing( shared, &This::_rangeEmpty );
        for ( std::set< unsigned >::const_iterator it = shared.emptySCCs.begin(); it != shared.emptySCCs.end(); ++it )
            activeSlices.erase( *it ); // *it is empty
        // TODO push down activeSlices so that we can update rangeSliceCount and remove unnecessary; actually push down emptySCCs
        shared.emptySCCs.clear();
        assert( !shared.flag || !activeSlices.size() );
        return shared.flag;
    }

    //-------------------

    /**
     * OBFR function
     *
     * Tests if all slices from the current RANGE set are same as slices from the RANGE set from the previous iteration.
     */
    void _rangeNotChanged() {
        std::map< unsigned, unsigned > unchangedSlices = shared.visitedSCCs;
        for ( std::map< unsigned, unsigned >::iterator mit = shared.visitedSCCs.begin(); mit != shared.visitedSCCs.end(); ++mit ) {
            const unsigned originalSize = originalRangeSliceCount[ mit->first ];
            if ( getTable( B ).sliceCount[ mit->first ] != originalSize )
                unchangedSlices.erase( mit->first ); // not same
            else
                unchangedSlices[ mit->first ] += originalSize; // same, update size
        }
        shared.visitedSCCs = unchangedSlices; // update size to current iteration
    }

    /// Returns true iff at least one slice in the current set B is a SCC
    std::map< unsigned, unsigned > isSCC() {
        assert( !shared.visitedSCCs.size() );
        for ( std::set< unsigned >::const_iterator it = activeSlices.begin(); it != activeSlices.end(); ++it )
            shared.visitedSCCs[ *it ] = 0; // any active slice can form a SCC
        domain().parallel().runInRing( shared, &This::_rangeNotChanged );
        std::map< unsigned, unsigned > sccSlices = shared.visitedSCCs; // unchanged slices are SCCs
        shared.visitedSCCs.clear();
        return sccSlices;
    }

    //-------------------

    /// Assings new component ids to states in B, removes nonSCC states from B
    void setIdOfSCC( const std::map< unsigned, unsigned >& sccSlices ) {
        assert( !shared.visitedSCCs.size() );
        assert( !shared.emptySCCs.size() );
        assert( activeSlices.size() );
        for ( std::map< unsigned, unsigned >::const_iterator mit = sccSlices.begin(); mit != sccSlices.end(); ++mit ) {
            shared.visitedSCCs[ mit->first ] = ++lastId; // sliceId -> new componentId map
            assert( mit->second );
            if ( mit->second == 1 ) shared.emptySCCs.insert( mit->first ); // one state nontrivial SCC
        }
        domain().parallel().run( shared, &This::_setIdOfSCC );
        shared.visitedSCCs.clear();
        shared.emptySCCs.clear();
    }

    /**
     * OBFR function, however required just for the following processing
     *
     * Assigns new component ids to states in B. Removes nonSCC states from B.
     */
    void _setIdOfSCC() {
        this->restart();

        assert( this->m_table == NULL );
        setAlgorithmTable( new Table( getTable( B ) ) ); // backup of B, restored in removeSCCs

        processInitials( B, &This::setIdOfSCCEdge, true );

        while ( !this->idle() );
    }

    /// Assigns component id to to state, removes nonSCC states.
    bool setIdOfSCCEdge( Node, Node to ) {
        std::map< unsigned, unsigned >::const_iterator mit = shared.visitedSCCs.find( extension( to ).sliceId );
        if ( mit != shared.visitedSCCs.end() ) {
            extension( to ).componentId = mit->second; // new id
            if ( shared.emptySCCs.count( mit->first ) ) // is it only one state
                simpleSCCs.insert( mit->second );
        } else
            getTable( B ).remove( to, extension( to ).sliceId ); // remove nonSCC states
        return false;
    }

    //-------------------

    /// Takes new slices from the set B and stores them into the set READY.
    void prepareNewSlices() {
        assert( !shared.visitedSCCs.size() );
        for ( std::set< unsigned >::const_iterator it = activeSlices.begin(); it != activeSlices.end(); ++it ) {
            const unsigned newSliceId = ++lastSliceId; // remap ids
            readySlices.insert( newSliceId );
            shared.visitedSCCs[ *it ] = newSliceId;
        }
        domain().parallel().run( shared, &This::_prepareNewSlices );
        shared.visitedSCCs.clear();
    }

    void _prepareNewSlices() {
        this->restart();

        processInitials( B, &This::prepareNewSlicesEdge );

        while ( !this->idle() );
    }

    bool prepareNewSlicesEdge( Node from, Node to ) {
        assert( !from.valid() );

        const std::map< unsigned, unsigned >::const_iterator mit = shared.visitedSCCs.find( extension( to ).sliceId );
        assert( mit != shared.visitedSCCs.end() );
        assert( mit->second < 1000000 ); // too many slices, TODO FIXME
        extension( to ).sliceId = mit->second; // remap id

        assert( !getTable( READY ).has( to ) );
        getTable( READY ).insert( to, mit->second );
        return false;
    }

    //-------------------

    /// Takes prepared slices and makes them active
    void addReadySlices() {
        activeSlices.clear();
        activeSlices.insert( readySlices.begin(), readySlices.end() );
        domain().parallel().run( shared, &This::_addReadySlices );
    }

    void _addReadySlices() {
        this->restart();

        assert( getTable( SEEDS ).statesCount() == 0 );

        processInitials( READY, &This::addReadySlicesEdge );

        while ( !this->idle() );
    }

    bool addReadySlicesEdge( Node from, Node to ) {
        assert( !from.valid() );

        getTable( V ).insert( to, extension( to ).sliceId );

        return false;
    }

    //-------------------

    /// Restores all seeds from READY to SEEDS
    void restoreSeeds() {
        readySlices.clear();
        domain().parallel().run( shared, &This::_restoreSeeds );
        for ( unsigned p = 0; p < this->peers(); p++ )
            assert( !domain().shared( p ).flag );
    }

    void _restoreSeeds() {
        this->restart();

        processInitials( READY, &This::restoreSeedsEdge );
        shared.flag = getTable( V ).statesCount();

        while ( !this->idle() );
    }

    bool restoreSeedsEdge( Node from, Node to ) {
        assert( !from.valid() );

        getTable( SEEDS ).insert( to );
        return false;
    }

    //-------------------

    /// Removes SCC states from B, restores original B from backup
    void removeSCCs( std::map< unsigned, unsigned > sccSlices ) {
        assert( !shared.visitedSCCs.size() );
        shared.visitedSCCs = sccSlices;
        domain().parallel().run( shared, &This::_removeSCCs );
        shared.visitedSCCs.clear();
    }

    void _removeSCCs() {
        this->restart();

        if ( this->m_table != NULL ) {
            setTable( B, this->m_table ); // frees original B
            setAlgorithmTable( NULL );
        }

        processInitials( B, &This::removeSCCsEdge, true );
        visit( &This::removeSCCsEdge );
    }

    bool removeSCCsEdge( Node from, Node to ) {
        assert( !from.valid() );
        if ( shared.visitedSCCs.count( extension( to ).sliceId ) )
            getTable( B ).remove( to );
        return false;
    }

/****************************************************************/
/* Approximating graph identification functions                 */
/****************************************************************/

#define ELIMINATE_STATE_NONE 0
#define ELIMINATE_STATE_OUT 1
#define ELIMINATE_STATE_INOUT 2
#define ELIMINATE_OUTGOING_TO_FROM 3

    /// Reset n to do not eliminate state
    void resetEliminate( Node n ) {
        extension( n ).eliminateState = ELIMINATE_STATE_NONE;
    }

    /// Set n to eliminate at least from outside state
    void setEliminateOutside( Node n ) {
        if ( extension( n ).eliminateState == ELIMINATE_STATE_NONE )
            extension( n ).eliminateState = ELIMINATE_STATE_OUT;
    }

    /// Set n to eliminate completely state
    void setEliminateInsideOutside( Node n ) {
        extension( n ).eliminateState = ELIMINATE_STATE_INOUT;
    }

    //-------------------

    void initAEC() { domain().parallel().run( shared, &This::_initAEC ); }

    /// Mark state to to not to be eliminated
    bool resetAECEdge( Node from, Node to ) {
        assert( !from.valid() );
        resetEliminate( to );
        return false;
    }

    /// Handles a transition during initialization of AEC detection.
    bool initAECEdge( Node from, Node to ) {
        if ( !from.valid() ) {
            assert( getTable( B ).has( to ) );
            if ( shared.g.isInRejecting( to, shared.testGroup ) ) { // eliminate state if it is rejecting
                setEliminateInsideOutside( to );
                getTable( ELIMINATE ).insert( to );
            }
#ifdef OBFFIRST
            else
                localqueue.push_back( successors( to ) );
        } else {
            if ( extension( from ).sliceId != extension( to ).sliceId || !getTable( B ).has( to ) ) { // out of slice
                setEliminateOutside( to ); // eliminate incoming transitions
                getTable( ELIMINATE ).insert( to );
            }
#endif
        }
        return false;
    }

    /**
     * Approximating graph identification function
     *
     * Initialization of AEC detection:
     *
     * Removes rejecting states and outgoing transitions of current Approximation Sets
     * which are stored in B. From this point, current Approximation Sets will be stored
     * in the set TEMP.
     */
    void _initAEC() { // removes rejecting states and outgoing transitions
        this->restart();

        oldSizes.clear();
#ifdef OBFFIRST
        setTableCopy( TEMP, B ); // TEMP contains current approximating sets
#else
        setTableCopy( B, V );
        setTableCopy( TEMP, V );
#endif
        getTable( REACHED ).clear();
        getTable( ELIMINATE ).clear();

#ifndef OBFFIRST
        if ( shared.testGroup > 0 ) // second run and so on
#endif
        processInitials( B, &This::resetAECEdge, true ); // reset elimination states
        processInitials( B, &This::initAECEdge, true );
        visit( &This::initAECEdge );
    }

    //-------------------

    void closure( bool postReachability = true ) { 
        shared.flag = postReachability;
        domain().parallel().run( shared, &This::_closure ); 
    }

    /**
     * Approximating graph identification function
     *
     * Closure:
     *
     * Removes states in ELIMINATE from current Approximation Sets. Makes sure that resulting sets
     * are closed on forward probabilistic transitions.
     */
    void _closure() { // parallel
        this->restart();

#ifndef OBFFIRST // TODO fix for OBFFIRST
        if ( shared.flag )
            processInitials( ELIMINATE, &This::postReachabilityClosureEdge );
        else
#endif
            processInitials( ELIMINATE, &This::closureEdge );
        visit( &This::closureEdge );
    }
    
    bool postReachabilityClosureEdge( Node from, Node to ) {
        assert( !from.valid() );
        setEliminateOutside( to );
        return closureEdge( from, to );
    }

    /// Handles a transition during Closure.
    bool closureEdge( Node from, Node to ) {
        if ( from.valid() && extension( to ).sliceId != extension( from ).sliceId && extension( from ).eliminateState != ELIMINATE_OUTGOING_TO_FROM ) return false; // FIXME // out of slice unless we know about it

        if ( !getTable( TEMP ).has( to ) || ( !from.valid() && extension( to ).eliminateState == ELIMINATE_STATE_OUT ) ) { // removes outgoing edges of approximating graph
            if ( !from.valid() && ( !getTable( B ).has( to ) || extension( to ).eliminateState == ELIMINATE_STATE_OUT ) ) {
                Node t = shared.g.copyState( to );
                extension( t ).eliminateState = ELIMINATE_OUTGOING_TO_FROM; // mark it specially
                localqueue.push_back( predecessors( t ) );
            }
            return false;
        }
        if ( from.valid() ) {
            assert( extension( from ).eliminateState != ELIMINATE_STATE_OUT );
            const bool onlyOutside =  extension( from ).eliminateState == ELIMINATE_OUTGOING_TO_FROM; // TODO fix me
            const unsigned fromSliceId = extension( from ).sliceId;
            const unsigned toSliceId = extension( to ).sliceId;
#ifdef OBFFIRST // TODO fix for OBFFIRST, same slice, states removed by lreach
            if ( onlyOutside && fromSliceId == toSliceId ) return false; // we are inside a slice, however only outside transitions should be eliminated
#endif
            const StateId fromId = shared.g.base().getStateId( from );
            bool found = false;
            // remove all to->from forward transitions of to
            Successors succs = successors( to );
            ProbabilisticTransitions probSuccs = probabilisticTransitions( to );
            std::set< unsigned > removeIds; // ids of probabilistic transitions to be removed
            while ( !succs.empty() ) {
                assert( !probSuccs.empty() );
                if ( !succs.headTransition().flag ) { // if it isn't disabled yet
                    if ( succs.headTransition().id == fromId ) {
                        if ( probSuccs.head().isProbabilistic )
                            removeIds.insert( probSuccs.head().id ); // mark this id for disabling
                        else
                            succs.headTransition().flag = true; // disable this transition
                    }
                    else 
                        found = true; // found at least one nondisabled transition
                }
                succs = succs.tail();
                probSuccs = probSuccs.tail();
            }
            assert( probSuccs.empty() );

            // TODO if !found, can't we eliminate this state straight?
            // remove associated probabilistic transitions
            if ( !removeIds.empty() ) {
                succs = successors( to );
                probSuccs = probabilisticTransitions( to );
                found = false;
                while ( !succs.empty() ) {
                    assert( !probSuccs.empty() );
                    if ( !succs.headTransition().flag ) {
                        if ( probSuccs.head().isProbabilistic && removeIds.count( probSuccs.head().id ) )
                            succs.headTransition().flag = true;
                        else 
                            found = true;
                    }
                    succs = succs.tail();
                    probSuccs = probSuccs.tail();
                }
                assert( probSuccs.empty() );
            }

            if ( !found ) { // no remaining forward transition, eliminate to
                setEliminateInsideOutside( to );
                closureEdge( Node(), to );
            }
        } else { // !from.valid()
            // eliminate state to
            assert( extension( to ).eliminateState == ELIMINATE_STATE_INOUT );
            getTable( TEMP ).remove( to, extension( to ).sliceId );
            // notify predecessors
            Successors preds = predecessors( to );
            while ( !preds.empty() ) {
                if ( !preds.headTransition().flag ) {
                    preds.headTransition().flag = true;
                    localqueue.push_back( predecessors( to ) );
                }

                preds = preds.tail();
            }
        }
        return false;
    }

    //-------------------

    /**
     * Approximating graph identification function
     *
     * Tests if all current Approximation Sets stored in TEMP are empty.
     */
    void _isEmpty() {
        if ( shared.flag )
            for ( std::map< unsigned, unsigned >::const_iterator mit = getTable( TEMP ).sliceCount.begin(); mit != getTable( TEMP ).sliceCount.end(); ++mit )
                if ( mit->second ) { // nonempty
                    shared.flag = false;
                    break;
                }
    }

    /// Returns true iff all current Approximation Sets stored in TEMP are empty.
    bool isEmpty() { // are current approximating sets empty
        shared.flag = true;
        domain().parallel().runInRing( shared, &This::_isEmpty );
        return shared.flag;
    }

    //-------------------

    /**
     * Approximating graph identification function
     *
     * Tests if sizes of Approximation Sets have not changed during an iteration.
     */
    void _isSame() {
        if ( shared.flag )
            for ( std::map< unsigned, unsigned >::const_iterator mit = getTable( TEMP ).sliceCount.begin(); mit != getTable( TEMP ).sliceCount.end(); ++mit )
                if ( oldSizes[ mit->first ] != mit->second ) { // changed
                    shared.flag = false;
                    break;
                }
        oldSizes = getTable( TEMP ).sliceCount; // update sizes to sizes of current iteration
    }
    
    /// Returns true iff sizes of Approximation Sets have not changed during an iteration.
    bool isSame() { // are approximating sets unchanged after one iteration
        shared.flag = true;
        domain().parallel().runInRing( shared, &This::_isSame );
        return shared.flag;
    }

    //-------------------

    void lreachability() { domain().parallel().run( shared, &This::_lreachability ); }

    /// Handles a transition during L-Reachability.
    bool lreachabilityEdge( Node from, Node to ) {
        if ( !from.valid() ) {
            if ( !shared.g.isInAccepting( to, shared.testGroup ) ) return false; // start from accepting states
            localqueue.push_back( predecessors( to ) );
        } else {
            if ( !getTable( TEMP ).has( to ) || getTable( REACHED ).has( to ) || extension( to ).sliceId != extension( from ).sliceId ) return false; // out of slice
            getTable( REACHED ).insert( to, extension( to ).sliceId ); // reached to, do not eliminate it
            getTable( ELIMINATE ).remove( to );
            localqueue.push_back( predecessors( to ) );
        }
        return false;
    }

    /**
     * Approximating graph identification function
     *
     * L-Reachability:
     * 
     * Starts backward reachability from accepting states contained in current
     * Approximation Sets stored in TEMP. New Approximation Sets are formed by reached states.
     * Unreached states previously contained in Approximation Sets are marked to be eliminated 
     * (stored in set ELIMINATE).
     */
    void _lreachability() { // parallel
        this->restart();

        assert( getTable( REACHED ).statesCount() == 0 );
        setTableCopy( ELIMINATE, TEMP );

        processInitials( TEMP, &This::lreachabilityEdge, true );
        visit( &This::lreachabilityEdge );

        switchTables( TEMP, REACHED );

        getTable( REACHED ).clear();//x
    }

    //-------------------

    /// Appends AEC states to B. Returns true if the initial state is a part of SCC.
    bool appendToPOne() {
        shared.flag = false;
        domain().parallel().run( shared, &This::_appendToPOne ); 
        for ( unsigned p = 0; p < this->peers(); p++ )
            if ( domain().shared( p ).flag )
                return true;
        return false;
    }

    bool appendToPOneEdge( Node, Node n ) {
        getTable( PONE ).insert( n );
        extension( n ).probability.final = 1; // do not set hasFinalProbability here, later in computeProbability
        return false;
    }

    /**
     * Approximating graph identification function, however used only for quantitative analysis
     *
     * Stores all AEC states into the PONE (probability == 1) set.
     */
    void _appendToPOne() {
        this->restart();
        Node initial = shared.g.initial();
        if ( owner( initial ) == this->globalId() && getTable( TEMP ).has( initial ) ) {
            shared.flag = true; // found initial
        } else
            processInitials( TEMP, &This::appendToPOneEdge, true );
        shared.g.release( initial );

        while ( !this->idle() );
    }

    //-------------------

    /**
     * Approximating graph identification function. Main loop.
     *
     * Detects if current SCCs stored in B contain AECs.
     */
    bool detectAEC( /*std::map< unsigned, unsigned > sccSlices*/ ) {
        bool found = false; // no AEC found yet
        // at this point B has only states in SCCs
        for ( unsigned g = 0; g < shared.g.acceptingGroupCount(); g++ ) { // for every pair in acceptance condition
            shared.testGroup = g;
            initAEC();
            closure( false );
            while ( !isEmpty() && !isSame() ) {
                lreachability();
                closure();
            }
            if ( !isEmpty() ) { // found AEC
                found = true;
                if ( shared.onlyQualitative || ( initialInsideAEC = appendToPOne() ) ) return true;
            }
        }
        return found;
    }

/****************************************************************/
/* Minimal Graph identification functions                       */
/****************************************************************/

    void minimalGraphIdentification() {
        domain().parallel().run( shared, &This::_minimalGraphIdentification );
    }

    /// Forms a set of states correspoding to the Minimal Graph. Resulting set is stored in ELIMINATE.
    void _minimalGraphIdentification() {
        this->restart();

        assert_eq( shared.offset, STARTINGOFFSET );

        translator.setParent( this );
        translator.useSCCs( shared.iterativeOptimization );

        getTable( ELIMINATE ).clear(); // store Minimal Graph here

        processInitials( SEEDS, &This::minimalGraphIdentificationEdge, true ); // start from SEEDS identified by _finalSeeds

        visit( &This::minimalGraphIdentificationEdge );
    }

    /// Handles a transition during _minimalGraphIdentification
    bool minimalGraphIdentificationEdge( Node from, Node to ) {
        // hasn't been explored during _finalSeeds or is a part of PONE
        if ( !getTable( REACHED ).has( to ) || ( from.valid() && getTable( PONE ).has( to ) ) ) return false;
        assert( from.valid() || getTable( PONE ).has( to ) );

        if ( from.valid() ) {
// #ifndef OBFFIRST BEWARE FIXME changes the original
            if ( !getTable( ELIMINATE ).has( to ) ) extension( to ).count = 0;
// #endif
            extension( to ).count++; // update successors count
            enableSuccessor( to, from ); // unable this successor
            if ( extension( to ).componentId ) { // nontrivial component
                if ( visitedSCCs.count( extension( to ).componentId ) == 0 ) // add representant
                    visitedSCCs[ extension( to ).componentId ] = SCC( shared.g.base().getStateId( to ) );
                if ( extension( from ).componentId != extension( to ).componentId ) // out of component
                    visitedSCCs[ extension( to ).componentId ].outTransitions++;
            }
        }
        if ( getTable( ELIMINATE ).has( to ) ) return false; // if it is already a part of Minimal Graph, then we can abort

        if ( extension( to ).componentId ) // increase count of component states
            visitedSCCs[ extension( to ).componentId ].states++;

        getTable( ELIMINATE ).insert( to ); // make it a part of Minimal Graph
        localqueue.push_back( predecessors( to ) ); // continue with predecessors

        return false;
    }

    //-------------------

    void finalSeeds() {
        shared.offset = STARTINGOFFSET; // TODO just use different tables
        domain().parallel().run( shared, &This::_finalSeeds );
    }

    /// Forward reachability until states inside some AEC, these states are stored inside SEEDS.
    void _finalSeeds() {
        this->restart();

        assert_eq( shared.offset, STARTINGOFFSET );

        getTable( REACHED ).clear(); // forward-visited
        getTable( SEEDS ).clear(); // seeds

        processInitial( &This::finalSeedsEdge );
        visit( &This::finalSeedsEdge );
    }

    /// Handles a transition during _finalSeeds
    bool finalSeedsEdge( Node from, Node to ) {
        if ( getTable( REACHED ).has( to ) ) return false; // it has been already processed

        getTable( REACHED ).insert( to ); // mark it processed

        if ( getTable( PONE ).has( to ) )
             getTable( SEEDS ).insert( to ); // it is a final seed
        else
            localqueue.push_back( successors( to ) );

        extension( to ).count = 0; // reset successors, we will count them during backward reachability inside _minimalGraphIdentification
        Successors succs = successors( to );
        while ( !succs.empty() ) {
            succs.headTransition().flag = false;
            succs.headTransition().flagDisabled = false;
            succs = succs.tail();
        }
        return false;
    }

/****************************************************************/
/* Quantitative analysis with iterative optimization functions  */
/****************************************************************/

    /**
     * Finds first transition from current to next which is not disabled yet and returns its stats.
     */
    unsigned selectedSuccessorProb( const Node current, const Node next, /* out */ ProbabilisticTransition* probTrans )
    {
        const StateId nextId = shared.g.base().getStateId( next );
        Successors succ = successors( current );
        ProbabilisticTransitions prob = probabilisticTransitions( current );
        bool probGroupStarted = false;
        unsigned probGroupStartedIndex;
        unsigned probGroupId;
        unsigned index = 0;
        bool found = false;
        while ( !succ.empty() ) {
            assert( !prob.empty() );

            if ( succ.headTransition().flag ) { // transition is not disabled
                if ( prob.head().isProbabilistic ) {
                    if ( probGroupStarted ) {
                        if ( prob.head().id != probGroupId ) {
                            index++;
                            probGroupId = prob.head().id;
                        }
                    } else {
                        probGroupStarted = true;
                        index++;
                        probGroupId = prob.head().id;
                    }
                } else {
                    probGroupStarted = false;
                    index++;
                }
                if ( succ.headTransition().id == nextId && !succ.headTransition().flagDisabled ) { // this is what we've been looking for
                    succ.headTransition().flagDisabled = true; // this takes care of multiple transitions to the same state
                    assert( probTrans != NULL );
                    *probTrans = prob.head();
                    found = true;
                    break;
                }
            }
            succ = succ.tail();
            prob = prob.tail();
            assert( !succ.empty() ); // we'd hope the node is actually there!
        }

        assert( found );
        assert( index > 0 );
        return index - 1;
    }

    /**
     * Disables first non-disabled successors next of current.
     * Signifies that next's probability is already calculated.
     * Used when current is a part of nontrivial SCC.
     */
    void disableSuccessor( const Node current, const Node next ) {
        const StateId nextId = shared.g.base().getStateId( next );
        Successors succ = successors( current );
        bool found = false;
        while ( !succ.empty() ) {
            if ( succ.headTransition().id == nextId && succ.headTransition().flag && !succ.headTransition().flagDisabled ) {
                succ.headTransition().flagDisabled = true;
                found = true;
                break;
            }
            succ = succ.tail();
        }
        assert( found );
    }

    /**
     * Enable transition from current to next. This is done during
     * backward reachability from seeds during minimal graph identification.
     */
    void enableSuccessor( const Node current, const Node next ) {
        Successors succs = successors( current );
        bool found = false;
        const StateId nextId = shared.g.base().getStateId( next );
        while ( !succs.empty() ) {
            if ( succs.headTransition().id == nextId && succs.headTransition().flag == false ) {
                found = true;
                succs.headTransition().flag = true;
                break;
            }
            succs = succs.tail();
        }

        assert( found );
    }

    /**
     * Returns a number of actions enabled from state current in Minimal graph.
     */
    unsigned selectedSuccessorsGroupCount( const Node current ) {
        Successors succs = successors( current );
        ProbabilisticTransitions probTrans = probabilisticTransitions( current );
        const StateId currentId = shared.g.base().getStateId( current );
        unsigned count = 0;
        bool probabilisticActionStarted = false;
        unsigned probabilisticActionId;
        while ( !succs.empty() ) {
            assert( !probTrans.empty() );
            if ( succs.headTransition().flag ) {
                count++;
                if ( probTrans.head().isProbabilistic ) {
                    if ( probabilisticActionStarted && probabilisticActionId == probTrans.head().id )
                        count--;
                    else {
                        probabilisticActionStarted = true;
                        probabilisticActionId = probTrans.head().id;
                    }
                }
            }
            probTrans = probTrans.tail();
            succs = succs.tail();
        }
        assert( probTrans.empty() );
        return count;
    }

    /// Updates probability of probabilistic action given the probability of self loop
    void probabilisticActionProbability( const bool hasSelfLoop, const REAL selfLoopProbability, REAL* currentProbability ) {
        if ( hasSelfLoop ) {
            if ( selfLoopProbability < 0.999999 ) // if loop probability is not 1
                *currentProbability *=  1 / ( 1 - selfLoopProbability );
            else
                *currentProbability = 0;
        }
    }

    /**
     * Updates probabilities of actions which might have a self loop enabled
     */
    void fixLoopProbability( Node current ) {
        Successors succs = successors( current );
        ProbabilisticTransitions probTrans = probabilisticTransitions( current );
        const StateId currentId = shared.g.base().getStateId( current );
        bool probabilisticActionStarted = false;
        int actionIndex = -1;
        bool hasSelfLoop = false;
        REAL selfLoopProbability = 0;
        unsigned probabilisticActionId;
        while ( !succs.empty() ) {
            assert( !probTrans.empty() );
            if ( succs.headTransition().flag ) {
                Node succ = succs.head();
                const StateId succId = succs.headTransition().id;

                if ( probTrans.head().isProbabilistic ) {
                    if ( !probabilisticActionStarted ) {
                        probabilisticActionStarted = true;
                        probabilisticActionId = probTrans.head().id;
                        hasSelfLoop = false;
                        actionIndex++;
                    }

                    if ( probabilisticActionId != probTrans.head().id ) { // assumption: no multiple transitions under single action
                        probabilisticActionProbability( hasSelfLoop, selfLoopProbability, &extension( current ).probability.list[ actionIndex ] );

                        actionIndex++;
                        probabilisticActionId = probTrans.head().id;
                        hasSelfLoop = false;
                    }

                    if ( succId == currentId ) {
                        hasSelfLoop = true;
                        selfLoopProbability = ( REAL ) probTrans.head().weight / probTrans.head().sum;
                    }
                } else {
                    if ( probabilisticActionStarted ) {
                        probabilisticActionProbability( hasSelfLoop, selfLoopProbability, &extension( current ).probability.list[ actionIndex ] );

                        probabilisticActionStarted = false;
                        hasSelfLoop = false;
                    }
                    actionIndex++;
                    if ( succId == currentId )
                        extension( current ).probability.list[ actionIndex ] = 0;
                }
                shared.g.release( succ );
            }
            probTrans = probTrans.tail();
            succs = succs.tail();
        }
        assert( probTrans.empty() );

        if ( probabilisticActionStarted ) // assumption: no multiple transitions under single action
            probabilisticActionProbability( hasSelfLoop, selfLoopProbability, &extension( current ).probability.list[ actionIndex ] );
    }

    //-------------------

    /// Has the final probability been determined? If so, it is stored inside shared.probability.
    bool doneProbab() {
        bool done = false;
        for ( unsigned i = 0; i < this->peers(); i++ )
            if ( domain().shared( i ).flag ) {
                shared.probability = domain().shared( i ).probability;
                done = true;
                break;
            }
        return done;
    }

    /// Mark state n as processed. Finish reachability if this is the initial state.
    void finishIfInitial( Node n ) {
        //getTable( ELIMINATE ).remove( n );//x
        extension( n ).count = 0;
        assert( !getTable( REACHED ).has( n ) );
        getTable( REACHED ).insert( n ); // TODO use count = 0 instead

        if ( shared.g.base().getStateId( n ) == shared.g.base().initialState ) {
            shared.flag = true;
            shared.probability = extension( n ).probability.final;
        } else
            localqueue.push_back( predecessors( n ) );
    }

    /// Finish processing state to if the given probability is 1
    bool finishIfReady( Node to, const REAL probability ) {
        if ( probability > 0.99999 ) {
            if ( extension( to ).probability.list != NULL ) 
                delete[] extension( to ).probability.list;
            
            simpleSCCs.erase( extension( to ).componentId );
            visitedSCCs.erase( extension( to ).componentId );
            extension( to ).count = 0;
            extension( to ).componentId = 0;
            
            extension( to ).probability.final = 1;
            extension( to ).hasFinalProbability = true;
            finishIfInitial( to );
            return true;
        }
        return false;
    }

    //-------------------

    /// Calculates probabilities of states. Returns true if the probability of the initial state has been calculated (stored inside shared.probability).
    bool computeProbability() {
        shared.flag = false;
        domain().parallel().run( shared, &This::_computeProbability ); // calculates probabilities directly where it can
        shared.toProcessSCCs.clear();
        
        return doneProbab();
    }

    /**
     * Computes probability of states outside SCCs with two or more states.
     */
    void _computeProbability() {
        this->restart();

        assert_eq( shared.offset, STARTINGOFFSET );

        // if some LP task is ready...
        const unsigned from = peerFromSCC();
        const unsigned count = peerCountSCC();

        std::map< StateId, REAL > results = lp_finish( from, count );
        assert( !count || results.size() );
        shared.toProcessSCCs.clear();

        // if probability of initial state has been resolved, then we are finished
        std::map< StateId, REAL >::const_iterator mit = results.find( shared.g.base().initialState );
        if ( mit != results.end() ) {
            shared.probability = mit->second;
            shared.flag = true;
            while ( !this->idle() );
        } else { // distribute probabilities to state owners
            for ( std::map< StateId, REAL >::const_iterator mit = results.begin(); mit != results.end(); ++mit ) {
                assert( mit->first );
                Node n = shared.g.base().getAnyState( mit->first );
                extension( n ).probability.final = mit->second;
                queueAny( owner( n ), shared.g.base().getAnyState( -1 ) ); // stands for an invalid state, conversion inside the visit method
                queueAny( owner( n ), n );
            }

            processInitials( SEEDS, &This::computeProbabilityEdge );
            visit( &This::computeProbabilityEdge );
        }
    }

    /// Handles a transition during _computeProbability
    bool computeProbabilityEdge( const Node from, const Node to ) {
        if ( !getTable( ELIMINATE ).has( to ) || // not inside Minimal Graph
            getTable( REACHED ).has( to ) || // probability has been determined??? TODO FIXME
            extension( to ).hasFinalProbability ||
            ( from.valid() && ( getTable( PONE ).has( to ) || // probability has been determined
            shared.g.base().getStateId( from ) == shared.g.base().getStateId( to ) ) ) ) // no looping
            return false;

        assert( !extension( to ).hasFinalProbability );
        if ( from.valid() ) {
            if ( extension( to ).componentId && !simpleSCCs.count( extension( to ).componentId ) ) { // to is in SCC with two or more states
                if ( visitedSCCs.find( extension( to ).componentId ) == visitedSCCs.end() ) return false; // this component has been already solved
                assert( extension( from ).componentId == 0 ); // coming to SCC from outside
                disableSuccessor( to, from );
                assert( visitedSCCs.find( extension( to ).componentId )->second.outTransitions > 0 );
                visitedSCCs.find( extension( to ).componentId )->second.outTransitions--; // decrease number of unresolved outgoing transitions
            } else { // to forms a trivial SCC or non-trivial SCC with one state
                ProbabilisticTransition prob;
                unsigned index = selectedSuccessorProb( to, from, &prob );
                const REAL fromProbability = extension( from ).probability.final * ( prob.isProbabilistic ? ( REAL ) prob.weight / prob.sum : 1 );
                if ( finishIfReady( to, fromProbability ) ) return false; // if probability is at least 1, we can stop processing to
                if ( extension( to ).probability.list == NULL ) { // holds calculated probabilities
                    const unsigned succsCount = selectedSuccessorsGroupCount( to );
                    assert( succsCount > index );
                    extension( to ).probability.list = new REAL[ succsCount ]; // 1 per enabled action, possibly smaller than extension( to ).count
                    memset( extension( to ).probability.list, 0, succsCount * sizeof( REAL ) );
                }
                if ( extension( to ).componentId ) { // nontrivial SCC
                    assert( simpleSCCs.count( extension( to ).componentId ) );
                    assert( visitedSCCs.find( extension( to ).componentId ) != visitedSCCs.end() );
                    visitedSCCs.find( extension( to ).componentId )->second.outTransitions--;
                }
                assert( selectedSuccessorsGroupCount( to ) > index );
                assert( extension( to ).count > 0 );
                extension( to ).count--; // decrease number of unresolved successors
                assert( prob.isProbabilistic || extension( to ).probability.list[ index ] == 0 );
                extension( to ).probability.list[ index ] += fromProbability; // add new probability
                if ( finishIfReady( to, extension( to ).probability.list[ index ] ) ) return false; // if probability is at least 1, we can stop processing to

                assert( extension( to ).probability.list[ index ] >= 0 && extension( to ).probability.list[ index ] <= 1.000001 );
                if ( extension( to ).componentId != 0 && visitedSCCs.find( extension( to ).componentId )->second.outTransitions == 0 ) { // TODO are we removing simple sccs from simplesccs and visitedsccs?
                    assert( simpleSCCs.count( extension( to ).componentId ) );

                    // if this SCC has only one state and yet it is nontrivial and has no unresolved outgoing transitions
                    fixLoopProbability( to );
                    simpleSCCs.erase( extension( to ).componentId );
                    visitedSCCs.erase( extension( to ).componentId );
                    extension( to ).count = 0;
                    extension( to ).componentId = 0;
                }
                if ( extension( to ).count == 0 ) { // no more unresolved transitions
                    assert( extension( to ).componentId == 0 );

                    REAL finalProbability = *std::max_element( extension( to ).probability.list, extension( to ).probability.list + selectedSuccessorsGroupCount( to ) );
                    assert( finalProbability >= 0 && finalProbability <= 1.000001 );
                    delete[] extension( to ).probability.list;
                    extension( to ).probability.final = finalProbability; // set the probability...
                    extension( to ).hasFinalProbability = true;
                    assert( !getTable( SEEDS ).has( to ) );
                    finishIfInitial( to ); // ...and continue
                }
            }
        } else { // probability of to has been resolved by some other method
            extension( to ).componentId = 0;
            extension( to ).hasFinalProbability = true;
            finishIfInitial( to ); // continue
        }
        return false;
    }

    //-------------------

    /// Stores SCCs to be processed by external LP solver to shared.toProcessSCCs. Selected are SCCs with resolved outgoing transitions.
    void prepareReadySCCs( std::set< SCCId >& tempVisitedSCCs ) {
        shared.emptySCCs = tempVisitedSCCs;
        domain().parallel().runInRing( shared, &This::_collectEmptySCCs ); // find SCCs ready to be solved by lp_solve

        assert_eq( shared.toProcessSCCs.size(), 0 );

        for ( std::set< SCCId >::const_iterator it = shared.emptySCCs.begin(); it != shared.emptySCCs.end(); ++it ) {
            tempVisitedSCCs.erase( *it );
            assert( sizeSCCs.count( *it ) );
            shared.toProcessSCCs.push_back( std::make_pair( *it, sizeSCCs[ *it ] ) );
        }

        shared.emptySCCs.clear();
    }

    /**
     * Collects ids of SCCs whose forward transitions probabilities have been already calculated.
     * Stores result in shared.emptySCCs
     */
    void _collectEmptySCCs() {
        for ( typename std::map< SCCId, SCC >::const_iterator it = visitedSCCs.begin(); it != visitedSCCs.end(); ++it ) {
            if ( it->second.outTransitions )
                shared.emptySCCs.erase( it->first ); // shared.emptySCCs contains only SCCs with resolved forward transitions
        }
    }

    //-------------------

    /// Retrieves SCCs from MG
    std::set< SCCId > collectSCCs() {
        assert( !shared.visitedSCCs.size() );
        domain().parallel().runInRing( shared, &This::_collectSCCs ); // gets all visited SCCs
        std::set< SCCId > tempVisitedSCCs; // set of ids of SCCs inside MG
        for ( std::map< SCCId, unsigned >::const_iterator mit = shared.visitedSCCs.begin(); mit != shared.visitedSCCs.end(); ++mit ) {
            tempVisitedSCCs.insert( mit->first );
            sizeSCCs[ mit->first ] = mit->second; // stores sizes of SCCs
        }
        shared.visitedSCCs.clear();
        return tempVisitedSCCs;
    }

    /**
     * Collecs ids of all visited SCCs which have at least two states.
     * Stores result in shared.visitedSCCs.
     */
    void _collectSCCs() {
        getTable( REACHED ).clear(); // used for states which probability has been determined
        for ( typename std::map< SCCId, SCC >::const_iterator it = visitedSCCs.begin(); it != visitedSCCs.end(); ++it )
            if ( simpleSCCs.count( it->first ) == 0 )
                shared.visitedSCCs[ it->first ] += it->second.states + it->second.outTransitions; // shared.visitedSCCs contains only visited SCCs with two and more states
    }

    //-------------------

    /// Iteratively find the probability of the initial state.
    REAL computeProbabilityIterative() {
        markBorderStates();
        std::set< SCCId > tempVisitedSCCs = collectSCCs();

        while ( !computeProbability() ) { // if probability of the initial state has not been found yet
            prepareReadySCCs( tempVisitedSCCs );
            constraintSCCs();
        }
        return shared.probability;
    }

    //-------------------

    /// For SCCs in shared.toProcessSCCs determines sets of linear inequalities for LP solver
    void constraintSCCs() {
        assert( shared.toProcessSCCs.size() > 0 );
        domain().parallel().run( shared, &This::_constraintSCCs ); // prepares LP handles, stores constraints
    }

    /**
     * Find all constraints characterizing assigned SCCs
     */
    void _constraintSCCs() {
        this->restart();

        assert( shared.toProcessSCCs.size() );

        // on this peer, we will be solving following SCCs
        std::vector< unsigned > sizes;
        const unsigned from = peerFromSCC();
        const unsigned count = from + peerCountSCC();
        for ( int i = from; i < count; i++ )
            sizes.push_back( shared.toProcessSCCs[ i ].second );

        lp_init( sizes ); // initializes lp solver

        getTable( RANGE ).clear(); // store explored states here
        minimizeStates.clear(); // store ids of states which probabilities should be minimized here

        typename std::map< SCCId, SCC>::const_iterator mit; // beware, working only in shared memory setting
        for ( unsigned c = 0; c < shared.toProcessSCCs.size(); c++ ) {
            if ( ( mit = visitedSCCs.find( shared.toProcessSCCs[ c ].first ) ) != visitedSCCs.end() ) {
                constraintSCCsEdge( Node(), getTable( RANGE ).get( mit->second.representant ) ); // send representants to owners of their component
                visitedSCCs.erase( shared.toProcessSCCs[ c ].first );
            }
        }
        visit( &This::constraintSCCsEdge, true ); // process selected SCCs, send states to owners of their SCC
    }

    /// Handles a transition during _constraintSCCs
    bool constraintSCCsEdge( Node from, Node to ) {
        if ( getTable( RANGE ).has( to ) ) return false; // has already been explored

        if ( !from.valid() || extension( from ).componentId == extension( to ).componentId ) { // from inside the component
            assert( extension( to ).probability.final == 0 );
            assert( !from.valid() || extension( from ).probability.final == 0 );
            assert( getTable( ELIMINATE ).has( to ) ); // it is inside the Minimal Graph
            getTable( RANGE ).insert( to );
            localqueue.push_back( successors( to ) );
        } else if ( getTable( ELIMINATE ).has( to ) ) { // from outside, let us know about already resolved probabilities
            assert( extension( to ).componentId == 0 );
            assert( getTable( REACHED ).has( to ) );
            Node copy = shared.g.copyState( to );
            extension( copy ).componentId = extension( from ).componentId;
            const unsigned owner = collectOwner( copy ); // SCC owner
            queueCollectPrefix( owner );
            queueAny( owner, copy );
        }
        return false;
    }

    //-------------------

    /// Marks states which have incoming transitions from outside of SCC.
    void markBorderStates() {
        domain().parallel().run( shared, &This::_markBorderStates );
    }

    /// Forward reachability from the initial state inside MG.
    void _markBorderStates() {
        this->restart();

        getTable( RANGE ).clear(); // store explored states here
        processInitial( &This::markBorderStatesEdge );
        visit( &This::markBorderStatesEdge );
    }

    bool markBorderStatesEdge( Node from, Node to ) {
        if ( !getTable( ELIMINATE ).has( to ) ) return false;

        // incoming transitions from outside of SCC
        if ( from.valid() && extension( from ).componentId != extension( to ).componentId ) 
            extension( to ).hasBackwardTransitionOutsideSCC = true;

        if ( getTable( RANGE ).has( to ) ) return false;

        getTable( RANGE ).insert( to );

        localqueue.push_back( successors( to ) );

        return false;
    }

    //-------------------

    /// Repartitions SCC which only partly belong to MG
    void fixSCCsInMG() {
        domain().parallel().run( shared, &This::_reinitOBF ); // initializes V
        shared.offset = STARTINGOFFSET + OFFSETDELTA;
        obfr( false );
        shared.offset = STARTINGOFFSET;
        domain().parallel().runInRing( shared, &This::_postOBF );
    }

    /// Handles a transition during _reinitOBF
    bool reinitOBFEdge( Node from, Node to ) {
        if ( getTable( B ).has( to ) || ( from.valid() && getTable( PONE ).has( to ) ) ) return false; // if it has been already processed or it has probability 1
#ifdef OBFFIRST
        if ( from.valid() && extension( from ).componentId != extension( to ).componentId ) return false; // if it goes out of the SCC
#else
        if ( !getTable( RANGE ).has( to ) ) return false;
#endif

        if ( !getTable( PONE ).has( to ) ) getTable( B ).insert( to ); // make it processed, if it is not a part of PONE

#ifdef OBFFIRST
        Node copy = shared.g.copyState( to );
        localqueue.push_back( predecessors( copy ) );
        extension( to ).componentId = 0; // reset component
#else
        localqueue.push_back( predecessors( to ) );
#endif
        return false;
    }

    /// Repartition a part of Minimal Graph into SCCs
    void _reinitOBF() {
        this->restart();

        getTable( B ).clear(); // store states here
        switchTables( RANGE, REACHED );

        assert( shared.offset == STARTINGOFFSET );

        processInitials( SEEDS, &This::reinitOBFEdge, true );
        visit( &This::reinitOBFEdge );

#ifndef OBFFIRST
        getTable( B ).sliceCount[ 0 ] = getTable( B ).statesCount();
#endif

        // prepare for run of OBFR
        shared.offset = STARTINGOFFSET + OFFSETDELTA;

        tables.resize( shared.offset + OFFSETDELTA + 1 );
        setTable( RANGE, this->makeTable() );
        setTable( SEEDS, this->makeTable() );
        setTable( B, this->makeTable() );

        getTable( REACHED ).clear();
        nullTable( TEMP ); //xx TODO arent we leaking TEMP here?
    }

    /// Fix tables after the repartitioning run of OBF
    void _postOBF() {
        assert( shared.offset == STARTINGOFFSET );
        switchTables( RANGE, REACHED );
    }

/******************************************************************/
/* Quantitative analysis without iterative optimization functions */
/******************************************************************/

    /// Returns probability of the initial state
    REAL computeProbabilityNoniterative() {
        domain().parallel().run( shared, &This::_collect );
        if ( !doneProbab() ) assert_die();
        return shared.probability;
    }

    /// Handles a transition during _collect
    bool collectEdge( Node from, Node to ) {
        if ( !getTable( ELIMINATE ).has( to ) || getTable( REACHED ).has( to ) ) return false;

        getTable( REACHED ).insert( to );

        localqueue.push_back( successors( to ) );
        return false;
    }

    /**
     * Aggregates all states on single peer to start LP task
     */
    void _collect() {
        this->restart();

        assert_eq( shared.offset, STARTINGOFFSET );

        getTable( REACHED ).clear();

        if ( this->globalId() == 0 ) { 
            std::vector< unsigned > all;
            all.push_back( shared.g.base().statesCount ); // TODO not all, just MG
            lp_init( all );
        }

        processInitial( &This::collectEdge );
        visit( &This::collectEdge, true );

        if ( this->globalId() == 0 ) { // peer 0 will do the heavy lifting
            shared.flag = true;
            progress() << " LP_SOLVE started...\t\t\t" << std::flush;
            shared.probability = lp_finish().begin()->second;
            progress() << "done." << std::endl;
        }
    }

/****************************************************************/
/* Linear programming encapsulating routines                    */
/****************************************************************/

    /// Initializes enough LP handles
    void lp_init( const std::vector< unsigned > sizes ) {
        assert( !lp_started );
        lp_started = true;

        assert( translator.empty() );

        if ( sizes.size() == 0 ) return;

        lp = new lprec*[ sizes.size() ];
        for ( unsigned i = 0; i < sizes.size(); i++ ) {
            lp[ i ] = make_lp( 0, sizes[ i ] );
            assert( lp[ i ] != NULL );
            set_add_rowmode( lp[ i ], TRUE );  /* makes building the model faster if it is done rows by row */
        }
    }

    /// Solves prepared LP tasks. Returns probabilities for requested states (stored in minimizeStates).
    std::map< StateId, REAL > lp_finish( const unsigned from = 0, const unsigned count = 1 ) {
        std::map< StateId, REAL > result;

        if ( !lp_started ) return result;
        lp_started = false;

        if ( count == 0 ) return result;

        for ( unsigned i = 0; i < count; i++ ) {
            set_add_rowmode( lp[ i ], FALSE ); /* rowmode should be turned off again when done building the model */

            SCCId compId = 0; // just not to have an uninitialized value
            typename std::map< SCCId, std::vector< StateId > >::const_iterator mit;
            bool minimizeInitial = true;
            if ( shared.toProcessSCCs.size() > 0 ) { // determine probabilities of which states should be minimized
                assert( shared.toProcessSCCs.size() > from + i );
                compId = shared.toProcessSCCs[ from + i ].first;
                mit = minimizeStates.find( compId );
                minimizeInitial = false;
            }
            std::vector< StateId > minimize;
            if ( !minimizeInitial && mit != minimizeStates.end() ) {
                minimize = mit->second;
                assert( minimize.size() );
                int colno[ minimize.size() ];
                REAL row[ minimize.size() ];
                for ( unsigned j = 0; j < minimize.size(); j++ ) {
                    colno[ j ] = translator.translate( minimize[ j ], compId );
                    row[ j ] = 1;
                }
                if ( !set_obj_fnex( lp[ i ], minimize.size(), row, colno ) )
                    assert_die();
            } else { // after all we are minimizing the initial state
                int colno = translator.translate( shared.g.base().initialState );
                REAL row = 1;
                if ( !set_obj_fnex( lp[ i ], 1, &row, &colno ) )
                    assert_die();
            }
            set_minim( lp[ i ] );

            /* I only want to see important messages on screen while solving */
            set_verbose( lp[ i ], IMPORTANT );

            /* Now let lpsolve calculate a solution */
            if ( solve( lp[ i ] ) == OPTIMAL ) {
                if ( minimize.size() ) {
                    REAL* results;
                    get_ptr_variables( lp[ i ], &results );
                    for ( unsigned j = 0; j < minimize.size(); j++ )
                        result[ minimize[ j ] ] = results[ translator.translate( minimize[ j ], compId ) - 1 ];
                } else
                    result[ shared.g.base().initialState ] = get_objective( lp[ i ] );
            } else {
                const char *errorMessage = "Beware, a partial solution is not optimal.";
                progress() << errorMessage << std::endl;
                write_LP( lp[ i ], stdout );
                throw errorMessage; // FIXME improve error handling
                assert_die();
            }
            delete_lp( lp[ i ] );
        }
        delete[] lp;

        translator.clear();

        return result;
    }

    /// Adds another inequality to an LP task lpId
    void lp_addRow( const StateId id, const SCCId compId, const unsigned lpId, const std::vector< Node >& succs, const unsigned from, const unsigned to ) {
        unsigned len = to - from + 1 + 1;
        int colno[ len ];
        REAL row[ len ];

        bool probabilistic = extension( succs[ from ] ).probabilisticTransition.isProbabilistic;
        assert( probabilistic && to >= from || !probabilistic && to == from );

        unsigned i = 0;
        REAL computed = 0;
        for ( unsigned j = from; j <= to; j++ ) {
            assert_eq( extension( succs[ j ] ).probabilisticTransition.isProbabilistic, probabilistic );
            if ( extension( succs[ j ] ).probability.final != 0 ) { // add solved probabilities
                if ( extension( succs[ j ] ).probabilisticTransition.isProbabilistic )
                    computed += ( ( REAL ) extension( succs[ j ] ).probabilisticTransition.weight / extension( succs[ j ] ).probabilisticTransition.sum ) * extension( succs[ j ] ).probability.final;
                else
                    computed = extension( succs[ j ] ).probability.final;
            } else { // add unsolved probabilites
                colno[ i ] = translator.translate( succs[ j ], compId );
                if ( extension( succs[ j ] ).probabilisticTransition.isProbabilistic ) {
                    row[ i ] = ( REAL ) -extension( succs[ j ] ).probabilisticTransition.weight / extension( succs[ j ] ).probabilisticTransition.sum;
                    assert( row[ i ] < 0 );
                } else
                    row[ i ] = -1;
                i++;
            }
            shared.g.release( succs[ j ] );
        }

        assert( i < len );

        colno[ i ] = translator.translate( id, compId );
        row[ i ] = 1;
        if ( !add_constraintex( lp[ lpId ], i + 1, row, colno, GE, computed ) )
            assert_die();
    }

    /// Adds constraints imposed by successors of state n
    void lp_addUncertain( const Node n, std::vector< Node >& succs ) {
        assert_eq( this->globalId(), collectOwner( n ) );

        const StateId id = shared.g.base().getStateId( n );
        const unsigned sccIndex = lp_sccIndex( n );
        const SCCId compId = extension( n ).componentId;

        bool probabilisticActionStarted = false;
        unsigned probabilisticActionStartedIndex;
        for ( unsigned i = 0; i < succs.size(); i++ ) { // assumption: prob trans with same id are consecutive
            if ( extension( succs[ i ] ).probabilisticTransition.isProbabilistic ) {
                if ( probabilisticActionStarted ) {
                    if ( extension( succs[ i ] ).probabilisticTransition.id != extension( succs[ i - 1 ] ).probabilisticTransition.id ) {
                        lp_addRow( id, compId, sccIndex, succs, probabilisticActionStartedIndex, i - 1 );
                        probabilisticActionStartedIndex = i;
                    }
                } else {
                    probabilisticActionStarted = true;
                    probabilisticActionStartedIndex = i;
                }
            } else {
                if ( probabilisticActionStarted )
                    lp_addRow( id, compId, sccIndex, succs, probabilisticActionStartedIndex, i - 1 );
                probabilisticActionStarted = false;
                lp_addRow( id, compId, sccIndex, succs, i, i );
            }
        }

        if ( probabilisticActionStarted )
            lp_addRow( id, compId, sccIndex, succs, probabilisticActionStartedIndex, succs.size() - 1 );

        if ( shared.iterativeOptimization ) { // determine if this state probability should be minimized
            assert( compId );

            if ( id == shared.g.base().initialState ) { // if it is an initial state, then we don't care about anything else
                minimizeStates[ compId ].clear();
                minimizeStates[ compId ].push_back( id );
                return;
            } else if ( minimizeStates[ compId ].size() == 1 && minimizeStates[ compId ][ 0 ] == shared.g.base().initialState )
                return;

            // test if it has a predecessor outside his component
            if ( extension( n ).hasBackwardTransitionOutsideSCC ) minimizeStates[ compId ].push_back( id );
        }
    }

    /// Adds solved state probability
    void lp_addCertain( const Node n ) {
        assert( extension( n ).componentId != 0 || !shared.iterativeOptimization );
        assert_eq( this->globalId(), collectOwner( n ) );
        int colno = translator.translate( n );
        REAL row = 1;
        if ( !add_constraintex( lp[ lp_sccIndex( n ) ], 1, &row, &colno, GE, extension( n ).probability.final ) )
            assert_die();
    }

    /**
     * Main quantitative analysis method.
     *
     * Creates the Minimal Graph and calculates maximal probability of the initial state.
     *
     * It either creates one big LP task and solves all constraints or iteratively calculates
     * probabilities directly and uses external LP solver to calculate probabilities of SCCs
     * with two or more states.
     */
    REAL lp_solve() {
        // Minimal Graph identification
        finalSeeds();
        if ( shared.iterativeOptimization )
            fixSCCsInMG(); // Repartition states before seeds into new SCCs
        minimalGraphIdentification();

        if ( shared.iterativeOptimization )
            return computeProbabilityIterative(); // iterative calculation
        else
            return computeProbabilityNoniterative(); // one big task
    }

/****************************************************************/
/* OBFR                                                         */
/****************************************************************/

    void _poneSize() {
        shared.size += getTable( PONE ).statesCount();
    }

    /// Returns a number of states in AECs
    unsigned poneSize() {
        shared.size = 0;
        domain().parallel().runInRing( shared, &This::_poneSize );
        if ( shared.size == 0 )
            shared.probability = 0;
        return shared.size;
    }
    
    //-------------------

    /// Main OBFR loop
    void obfr( bool detectEC = true ) {
        std::map< unsigned, unsigned > sccSlices;

        while ( addReadySlices(), findPivot() ) {
            unsigned i = 0;
            do {
                fwd( i++ );
            } while ( findPivot() ); // TODO backup seeds in case there is only one iteration of the above loop
            restoreSeeds();
            storeRangeSize();
            while ( !rangeEmpty() ) {
#ifdef OBFFIRST
                if ( detectEC )
#endif
                if ( owctyElimination() ) {
                    if ( shared.onlyQualitative )
                        foundAEC = true;
                    else
                        initialInsideAEC = true;
                    return;
                }
                bwd();
                sccSlices = isSCC();
#ifdef OBFFIRST
                if ( detectEC && sccSlices.size() ) {
                    if ( shared.g.base().hasProbabilisticTransitions() ) {
                        setIdOfSCC( sccSlices ); // and removes nonSCC states from B
                        if ( ( foundAEC = detectAEC( /*sccSlices*/ ) ) && ( shared.onlyQualitative || initialInsideAEC ) ) return;
                    } else
                        this->progress() << "Compact model does not contain probabilistic transitions, AEC detection disabled." << std::endl;
                    removeSCCs( sccSlices ); // and restores B
                }
#else
                setIdOfSCC( sccSlices );
                removeSCCs( sccSlices );
#endif
                seeds();
                prepareNewSlices();
            }
        }
    }

    /// Initializes structures for OBF algorithm
    void initObfr() {
        shared.offset = STARTINGOFFSET;
        domain().parallel().runInRing( shared, &This::_init );
        readySlices.insert( 0 );
    }

/****************************************************************/
/* Main routines                                                */
/****************************************************************/

    Probabilistic( Config *c = 0 ) : Algorithm( c, sizeof( Extension ) ), lastId( 0 ), lp_started( false ), foundAEC( false ), initialInsideAEC( false ), lastSliceId( 0 ) {
        if ( c ) {
            this->initPeer( &shared.g );
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTable;
            shared.onlyQualitative = c->onlyQualitative;
            shared.iterativeOptimization = c->iterativeOptimization;
            simpleOutput = c->simpleOutput;
        }
    }

    ~Probabilistic() {
#ifdef OBFFIRST
        for ( typename std::vector< Table* >::iterator it = tables.begin(); it != tables.end(); ++it )
            safe_delete( *it );
#endif
        // TODO fix me, something is released twice
    }

    std::ostream &progress( bool forceOutput = false ) {
        Algorithm::progress().clear( !simpleOutput || forceOutput ? std::ios_base::goodbit : std::ios_base::badbit );
        return Algorithm::progress();
    }

    Result run() {
        if ( !shared.g.base().hasBackwardTransitions() ) throw "Compact model does not contain backward transitions.";
        if ( !shared.g.base().hasProbabilisticTransitions() ) throw "Compact model does not contain probabilistic transitions.";

        progress() << "Qualitative analysis started...\t\t" << std::flush;
        initObfr();
#ifdef OBFFIRST
        obfr();
#else
        foundAEC = detectAEC();
#endif
        progress() << "done." << std::endl;

        if ( /*shared.g.base().hasProbabilisticTransitions() &&*/ !shared.onlyQualitative ) {
            progress() << "Quantitative analysis started." << std::endl;
            REAL probability = 0;
            if ( initialInsideAEC )
                probability = 1;
            else {
                const unsigned AECStates = poneSize();
                if ( AECStates ) probability = lp_solve();
            }
            progress() << " The maximal probability that the system does not satisfy the given LTL formula is: ";
            progress( true ) << probability;
            progress() << "." << std::endl;
            progress() << " The minimal probability that the system satisfies the given LTL formula is: " << ( 1 - probability ) << "." << std::endl;
            progress() << "Quantitative analysis completed." << std::endl;
        }
        if ( shared.onlyQualitative ) {
            progress() << "AEC is " << ( foundAEC ? "" : "not " ) << "reachable from the initial state." << std::endl;
            progress() << "The probability that the system does not satisfy the given LTL formula is: ";
            progress( true ) << ( foundAEC ? ">0" : "=0" );
            progress() << "." << std::endl;
        }

        result().fullyExplored = Result::Yes;
        shared.stats.updateResult( result() );
        return result();
    }
};

}
}

#endif
