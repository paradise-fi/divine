// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include <divine/graph/probabilistictransition.h>
#include <divine/generator/common.h>
#include <divine/toolkit/bitset.h>
#include <divine/graph/compact.h>

#include <algorithm>
#include <fstream>

#ifndef DIVINE_GENERATOR_COMPACT_H
#define DIVINE_GENERATOR_COMPACT_H

namespace divine {
namespace generator {

/// Represents a generator of compact state space (.compact files)
struct Compact : public Common< Blob > {
    typedef typename Common< Blob >::Label Label;
    typedef BitSet< Compact, StateId > Table; // used state storage
    typedef generator::Common< Blob > Common;

    /// References a state by its id; additional information about transition can be encoded into flags
    struct Successor {
        StateId id:30;
        bool flag:1;
        bool flagDisabled:1;
    };

    char *nodes; // stores states in blobs: BlobHeader, Algorithm specific extension, CompactState, StateId
    std::vector< Successor > *forward; // stores forward transitions
    std::vector< Successor > *backward; // stores backward transitions
    std::vector< ProbabilisticTransition > *probabilistic; // stores probabilistic information about transitions
    StateId initialState; // stores id of the initial state
    PropertyType acPropertyType; // acceptance condition type
    unsigned acCount; // number of acceptance condition pairs/sets

    unsigned blobSize; // slack + sizeof( CompactState )
    unsigned totalBlobSize; // blobSize + sizeof( BlobHeader )
    unsigned slack; // slack (sizeof( Extension ) from Algorithm)
    StateId fromStateId, toStateId; // states handled by this mpi process (can have multiple workers)
    unsigned statesCount; // total states count
    int workerStatesCount; // states count handled by this mpi process
    unsigned peersCount; // number of peers working with this generator instance
    unsigned mpiRank; // rank of the mpi process
    unsigned mpiSize; // number of mpi processes

    bool initialized; // has it been initialized? that is, was read called?
    bool slackInitialized; // has slack been initialized?

    static Compact* master; // poor man's reference counting

    /// Creates generator::Compact instance
    Compact() : Common(), nodes( NULL ), fromStateId( 0 ), toStateId( 0 ), statesCount( 0 ), peersCount( 1 ),
        mpiRank( 0 ), mpiSize( 1 ), initialized( false ), slackInitialized( false ) { }

    /// Destroys generator::Compact instance
    ~Compact() {
        if ( initialized ) {
            initialized = false;

            if ( Compact::master == this ) {
                delete [] nodes;
                delete forward;
                if ( backward != NULL )
                    delete backward;
                if ( probabilistic != NULL )
                    delete probabilistic;
            }
        }
    }

    /// Represents list of some kind of successors of one state
    template< typename T, typename S >
    struct Successors_ { 
        typedef Node Type;
        Node _from;
        unsigned cur, end;
        Compact *parent;
        std::vector< T >* trans;

        bool empty() const {
            if ( !_from.valid() ) 
                return true;
            return cur >= end;
        }

        Node from() { return _from; }

        S tail() const {
            S s = *reinterpret_cast< const S* >( this );
            s.cur++;
            return s;
        }
    };

    /// Successors or predecessors of state
    struct Successors : Successors_< Successor, Successors > {
        Node head() {
            StateId stateId = ( *trans )[ cur ].id;
            assert( stateId );
            return parent->getAnyState( stateId );
        }

        Successor& headTransition() {
            return ( *trans )[ cur ];
        }
    };

    /// Probabilistic successors
    struct ProbabilisticTransitions : Successors_< ProbabilisticTransition, ProbabilisticTransitions > {
        ProbabilisticTransition& head() { return ( *trans )[ cur ]; }
    };

    /// Returns information about probabilistic forward transitions
    ProbabilisticTransitions probabilisticTransitions( Node st ) {
        assert( initialized );
        assert( hasProbabilisticTransitions() );

        ProbabilisticTransitions ret;
        ret._from = st;
        ret.trans = probabilistic;
        CompactState &compactState = getCompactState( st );
        ret.cur = compactState.forward;
        CompactState &nextCompactState = getNextCompactState( st );
        ret.end = nextCompactState.forward;
        ret.parent = this;
        return ret;
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        assert( initialized );

        unsigned cur = getCompactState( st ).forward,
                 end = getNextCompactState( st ).forward;

        while ( cur < end ) {
            yield( getAnyState( (*forward)[ cur ].id ), Label() );
            ++ cur;
        }
    }

    /// Returns predecessors of state st
    Successors predecessors( Node st ) {
        assert( initialized );
        assert( hasBackwardTransitions() );

        Successors ret;
        ret._from = st;
        ret.trans = backward;
        CompactState &compactState = getCompactState( st );
        ret.cur = compactState.backward;
        CompactState &nextCompactState = getNextCompactState( st );
        ret.end = nextCompactState.backward;
        ret.parent = this;
        return ret;
    }
    
    /// Returns true iff current model description contains backward transitions.
    bool hasBackwardTransitions() { return backward; }
    
    /// Returns true iff current model description contains information about probabilistic transitions.
    bool hasProbabilisticTransitions() { return probabilistic; }

    /// Returns CompactState of state with identifier i
    CompactState& getCompactState( unsigned i ) {
        assert( initialized );
        assert( i >= fromStateId && i <= toStateId + 1 ); // +1 to get the last border
        return getStateExtension< CompactState >( i, sizeof( BlobHeader ) + slack );
    }

    /// Determines if state st is located in the nodes array
    bool inCompactPool( Node st ) {
        assert( initialized );

        return st.ptr >= nodes && st.ptr < nodes + totalBlobSize * workerStatesCount;
    }

    /// Returns CompactState extension of state st
    CompactState& getCompactState( Node st ) {
        assert( initialized );

        if ( inCompactPool( st ) || !belongsToUs( getStateId( st ) ) )
            return st.get< CompactState >( slack );
        else
            return getCompactState( getStateId( st ) );
    }

    /// Returns id of state st
    StateId& getStateId( Node st ) {
        assert( initialized );
        assert( st.valid() );

        return st.get< StateId >( blobSize - sizeof( StateId ) );
    }

    /// Returns a state with id id
    Node getState( const StateId id ) {
        assert( initialized );
        assert( belongsToUs( id ) );

        Node n( nodes + totalBlobSize * ( id - fromStateId ) );
        return n;
    }

    /** Returns a state with id id if id is handled by this instance. 
     * Constructs a new state with required id otherwise.
     */
    Node getAnyState( const StateId id ) {
        assert( initialized );

        if ( belongsToUs( id ) )
            return getState( id );
        else
            return createState( id );
    }

    /// Creates a new state if id belongs to a different generator instance
    Node createState( const StateId id ) {
        assert( initialized );
        assert( !belongsToUs( id ) );

        Node fakeState( pool(), blobSize );
        memset( fakeState.ptr + sizeof( BlobHeader ), 0, blobSize );
        getStateId( fakeState ) = id;
        return fakeState;
    }

    /// Returns CompactState of state after st in the nodes array
    CompactState& getNextCompactState( Node st ) {
        assert( initialized );
        StateId id = getStateId( st );
        assert( id >= fromStateId && id <= toStateId + 1 );

        return getStateExtension< CompactState >( id, totalBlobSize + sizeof( BlobHeader ) + slack );
    }

    /// Returns a number of predecessor states
    unsigned predsCount( Node st ) {
        assert( initialized );
        assert( backward != NULL );
        assert( belongsToUs( getStateId( st ) ) );
        return getNextCompactState( st ).backward - getCompactState( st ).backward;
    }

    /// Returns a state extension
    template< typename Extension >
    Extension& getStateExtension( unsigned i, int off = sizeof( BlobHeader ) ) {
        assert( initialized );

        return *reinterpret_cast< Extension* >( nodes + off + totalBlobSize * ( i - fromStateId ) );
    }

    /// Make i higher or equal multiple of 4 (alignment)
    unsigned roundTo4Multiple( unsigned i ) {
        i += 3;
        i = ~i;
        i |= 3;
        i = ~i;
        return i;
    }

    /// Sets algorithm's slack and returns used slack
    int setSlack( int s ) {
        assert( s >= 0 );
        slackInitialized = true;

        slack = roundTo4Multiple( s );
        blobSize = slack + sizeof( CompactState ) + sizeof( StateId );
        const int resSlack = blobSize - sizeof( StateId ); // only id is used for hashing
        alloc.setSlack( resSlack );
        totalBlobSize = blobSize + sizeof( BlobHeader );
        assert_eq( totalBlobSize, Node::allocationSize( blobSize ) );
        return resSlack;
    }

    // Acceptance condition type. FIXME.
    PropertyType propertyType( std::string ) { return acPropertyType; }

    /// Is state s in the accepting set?
    bool isAccepting( Node s ) {
        assert( initialized );

        if ( acPropertyType == PT_Deadlock )
            return false;
        return isInAccepting( s, 0 );
    }

    /**
     * Is state s in the acc_group set of the acceptance condition or
     * in the accepting set of the acc_group pair?
     */
    bool isInAccepting( Node s, int acc_group ) {
        assert( initialized );

        assert( acPropertyType != PT_Deadlock );
        assert( acc_group < acCount );

        unsigned group = acc_group;
        if ( acPropertyType == PT_Rabin || acPropertyType == PT_Streett )
            group *= 2;
        return getCompactState( s ).ac[ group ];
    }

    /**
     * Is state s in the acc_group set of the acceptance condition?
     */
    bool isInRejecting( Node s, int acc_group ) {
        assert( initialized );

        assert( acPropertyType != PT_Deadlock );
        assert( acc_group < acCount );
        assert( acPropertyType == PT_Rabin || acPropertyType == PT_Streett );

        return getCompactState( s ).ac[ acc_group * 2 + 1 ];
    }

    /// Number of acceptance condition pairs/sets
    unsigned acceptingGroupCount() { return acCount; }

    /// Does state with id id belong to this mpi process?
    bool belongsToUs( StateId id ) {
        return fromStateId <= id && toStateId >= id;
    }

    /// Returns the initial state
    Node initial() {
        assert( initialized );

        return getAnyState( initialState );
    }

    /// Enques initial state
    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial(), Label() );
    }

    /// Releases memory used by state s
    void release( Node s ) { 
        if ( !inCompactPool( s ) ) {
            if ( s.header().heap )
                s.free();
            else
                s.free( pool() );
        }
    }

    bool isGoal( Node s ) {
        return false;
    }

    std::string showNode( Node s ) {
        if ( !s.valid() )
            return "[]";
        std::stringstream stream;
        stream << "[" << getStateId( s ) << "]";
        return stream.str();
    }

    /// currently only dummy method
    std::string showTransition( Node from, Node to, Label ) {
        return "";
    }

    /// How many states are handled by one peer based on number of peers peersCount
    unsigned getStatesPerPeer() {
        assert( peersCount );
        return statesCount / peersCount + 1;
    }

    /// Returns owner id of state n based on peersCount, overrides superclass implementation.
    template< typename Hash, typename Worker >
    int owner( Hash &, Worker &, Node n, hash_t = 0 ) {
        assert( n.valid() );

        return owner( n );
    }

    /// Returns owner id of state n based on peersCount
    unsigned owner( Node n ) {
        assert( initialized );

        // states are partitioned into continuous blocks based on id
        return getStateId( n ) / getStatesPerPeer();
    }

    /// Is id id valid node id?
    bool isValidId( StateId id ) {
        return id >= 1 && id <= statesCount;
    }

    /// Returns pair of minimal and maximal ids of states handled by peer with global id peerId
    std::pair< StateId, StateId > getPeerStates( const unsigned peerId ) {
        const unsigned statesPerPeer = getStatesPerPeer();
        assert( statesPerPeer > 0 );
        assert( peersCount > 0 );
        return getStatesPartition( peerId, peersCount, statesPerPeer );
    }

    /**
     * Returns pair of minimal and maximal ids of states handled by unit id out of idCount units.
     * Each unit handles up to partitionSize states.
     */
    std::pair< StateId, StateId > getStatesPartition( const unsigned id, const unsigned idCount, 
                                                      const unsigned partitionSize ) {
        assert( idCount );
        assert( partitionSize );

        StateId fromStateId = id * partitionSize;
        if ( fromStateId == 0 ) fromStateId = 1;

        StateId toStateId;
        if ( id == idCount - 1 )
            toStateId = statesCount;
        else
            toStateId = partitionSize * ( id + 1 ) - 1;

        if ( fromStateId > statesCount ) // not enough states for this worker
            toStateId = fromStateId - 1; 

        return std::make_pair( fromStateId, toStateId );
    }

    /// Sets information about domain geometry
    void setDomainSize( const unsigned mpiRank = 0, const unsigned mpiSize = 1,
                        const unsigned peersCount = 1 ) {
        assert( peersCount );
        assert( mpiSize );
        this->mpiRank = mpiRank;
        this->mpiSize = mpiSize;
        this->peersCount = peersCount;
    }

    /**
     * Base class for reading compact files.
     */
    struct CompactReader {

        Compact* parent; // pointer to generator::Compact

        CompactReader( Compact* generator ) : parent( generator ) {}
        virtual ~CompactReader() {};

        /// Sets BlobHeader
        void setHeader( unsigned i ) {
            BlobHeader &bh = parent->getStateExtension< BlobHeader >( i, 0 );
            bh.size = parent->blobSize;
            bh.permanent = 1;
            bh.heap = 0;
        }

        /// Allocates memory for storing transitions
        void allocTransitions( const unsigned backwardCount, const unsigned forwardCount, const bool probabilisticEnabled ) {
            parent->backward = backwardCount == 0 || !parent->slackInitialized ? NULL :
                new std::vector< Successor >( backwardCount );

            parent->forward = !parent->slackInitialized ? NULL :
                new std::vector< Successor >( forwardCount ); 

            parent->probabilistic = !parent->slackInitialized || !probabilisticEnabled ? NULL :
                new std::vector< ProbabilisticTransition >( forwardCount );
        }

        /// Allocates memory for states handled by this mpi process
        bool allocMPIProcessStates() {
            setMPIProcessStates();

            // this process will not handle any states
            if ( parent->workerStatesCount <= 0 ) return false;

            // we only want to fetch header
            if ( !parent->slackInitialized ) return false;

            const unsigned nodesSize = parent->totalBlobSize * ( parent->workerStatesCount + 1 );
            parent->nodes = new char[ nodesSize ];
            memset( parent->nodes, 0, nodesSize );

            return true;
        }

        /// Sets information about states handled by this mpi process
        void setMPIProcessStates() {
            assert( parent->mpiSize );
            assert( parent->peersCount );

            const unsigned statesPerWorker = parent->getStatesPerPeer() *
                    ( parent->peersCount / parent->mpiSize );

            std::pair< StateId, StateId > partition =
                    parent->getStatesPartition( parent->mpiRank, parent->mpiSize, statesPerWorker );
            parent->fromStateId = partition.first; parent->toStateId = partition.second;
            parent->workerStatesCount = parent->toStateId - parent->fromStateId + 1;
        }

        /// Initializes a state in compact pool
        CompactState& initState( StateId id, unsigned b, unsigned f ) {
            CompactState &compactState = parent->getCompactState( id );
            compactState.backward = b;
            compactState.forward = f;

            setHeader( id );

            parent->getStateExtension< StateId >( id, parent->totalBlobSize - sizeof( StateId ) ) = id;
            assert_eq( parent->getStateId( parent->getState( id ) ), id );

            return compactState;
        }

        /// Adds a transition to a state
        void addTransition( std::vector< Successor > *transitions, unsigned i, StateId t ) {
            assert( parent->isValidId( t ) );
            if ( i == transitions->size() ) 
                transitions->resize( transitions->size() + parent->workerStatesCount );
            ( *transitions )[ i ].id = t;
            ( *transitions )[ i ].flag = false;
        }

        /// Adds a probabilistic information about transition
        void addProbabilisticTransition( unsigned i, ProbabilisticTransition pt ) {
            if ( i == parent->probabilistic->size() ) 
                parent->probabilistic->resize( parent->probabilistic->size() + parent->workerStatesCount );
            ( *parent->probabilistic )[ i ] = pt;
        }

        /// Appends last indices to transition vectors
        void setLastTransitionIndices( unsigned b, unsigned f ) {
            CompactState &lastCS = parent->getCompactState( parent->toStateId + 1 );
            lastCS.backward = b;
            lastCS.forward = f;
        }

        /// Reads a compact state space from a file
        virtual void readInput( std::ifstream& compactFile, bool abortAfterFetchingHeader ) = 0;
    };

    /**
     * Class used for reading plaintext compact state space files.
     */
    struct TextReader : CompactReader {

        TextReader( Compact* generator ) : CompactReader( generator ) {}

        /// Sets acceptance condition
        void setAC( CompactState &cn, std::ifstream &compactFile ) {
            if ( parent->acCount > 0 ) {
                std::string acCaption;
                compactFile >> acCaption;
                for ( std::string::const_iterator it = acCaption.begin(); it != acCaption.end(); ++it )
                    cn.ac[ it - acCaption.begin() ] = *it == '+';
            }
        }

        /// Sets backward transitions
        void setBackward( std::ifstream &compactFile, unsigned &b ) {
            std::string label;
            compactFile >> label;
            assert_eq( label, compact::cfBackward );

            StateId bt;
            while ( compactFile >> bt )
                addTransition( parent->backward, b++, bt );
            compactFile.clear();
        }

        /// Sets forward transitions
        void setForward( std::ifstream &compactFile, unsigned &f ) {
            std::string label;
            compactFile >> label;
            assert_eq( label, compact::cfForward );
            
            StateId ft;
            while ( compactFile >> ft )
                addTransition( parent->forward, f++, ft );
            compactFile.clear();
        }
        
        /// Sets probabilistic information about transitions
        void setProbabilistic( std::ifstream &compactFile, unsigned &f ) {
            std::string label;
            compactFile >> label;
            assert_eq( label, compact::cfProbabilistic );
            
            ProbabilisticTransition pt;
            bool isProbabilistic;
            unsigned weight, sum, id;
            while ( compactFile >> isProbabilistic ) {
                pt.isProbabilistic = isProbabilistic;
                if ( isProbabilistic ) {
                    compactFile >> weight >> sum >> id;
                    assert( weight <= 0xFF );
                    assert( sum <= 0x7FFF );
                    assert( id <= 0xFF );
                    
                    pt.weight = weight;
                    pt.sum = sum;
                    pt.id = id;
                }
                addProbabilisticTransition( f++, pt );
            }
            compactFile.clear();
        }

        /// Allocates appropritate space for storing transitions
        void prepareTransitions( std::ifstream &compactFile, bool& backwardEnabled, bool& probabilisticEnabled ) {
            std::string label;
            
            probabilisticEnabled = fetchProbabilistic( compactFile );

            int backwardCount;
            compactFile >> label >> backwardCount;
            assert_eq( label, compact::cfBackward );

            int forwardCount;
            compactFile >> label >> forwardCount;
            assert_eq( label, compact::cfForward );

            if ( backwardCount ) assert_eq( backwardCount, forwardCount );
            backwardEnabled = backwardCount;

            // lets take a bit more than number of our states, resize later
            allocTransitions( std::min( backwardCount, parent->workerStatesCount * 2 ),
                              std::min( forwardCount, parent->workerStatesCount * 2 ), probabilisticEnabled );
        }

        /// Sets the initial state id
        void fetchInitialState( std::ifstream &compactFile ) {
            std::string label;

            compactFile >> label >> parent->initialState;
            assert_eq( label, compact::cfInitial );
        }

        /// Retrieves information about the acceptance condition
        void fetchAcceptanceCondition( std::ifstream &compactFile ) {
            std::string label;

            unsigned acTypeNum;
            parent->acCount = 0;
            compactFile >> label >> acTypeNum;
            assert_eq( label, compact::cfAC );
            parent->acPropertyType = static_cast< PropertyType >( acTypeNum );
            if ( parent->acPropertyType != PT_Deadlock )
                compactFile >> parent->acCount;
        }

        /// Retrieves information about number of states
        void fetchStatesCount( std::ifstream &compactFile ) {
            std::string label;

            compactFile >> label >> parent->statesCount;
            assert_eq( label, compact::cfStates );
            assert( parent->statesCount );
        }
        
        /// Retrieves information about having probabilistic transitions
        bool fetchProbabilistic( std::ifstream &compactFile ) {
            std::string label;
            
            bool probabilistic;

            compactFile >> label >> probabilistic;
            assert_eq( label, compact::cfProbabilistic );
            
            return probabilistic;
        }

        /// Reads compact state space from file
        void readInput( std::ifstream& compactFile, bool abortAfterFetchingHeader ) {
            std::string label;

            // read header
            fetchStatesCount( compactFile );
            // if we do not have enough states for this mpi process, we can abort
            if ( !allocMPIProcessStates() ) abortAfterFetchingHeader = true; 

            bool backwardEnabled, probabilisticEnabled;
            prepareTransitions( compactFile, backwardEnabled, probabilisticEnabled );
            fetchInitialState( compactFile );
            fetchAcceptanceCondition( compactFile );

            if ( abortAfterFetchingHeader ) return; // abort reading before loading states

            Compact::master = parent;
            unsigned b = 0, f = 0;

            // for each state
            for ( unsigned i = 1; i <= parent->statesCount; i++ ) {
                compactFile >> label;
                while ( label != compact::cfState ) {
                    getline( compactFile, label);
                    compactFile >> label;
                }

                // if this state does not belong to us
                if ( i < parent->fromStateId ) continue;
                if ( i > parent->toStateId ) break;

                StateId stateNum;
                compactFile >> stateNum;
                assert_eq( label, compact::cfState );
                assert_eq( stateNum, i );
                assert( parent->isValidId( stateNum ) );

                // initialize state's CompactState, BlobHeader, id, AC
                CompactState& compactState = initState( i, b, f );

                setAC( compactState, compactFile );

                // handle transitions
                if ( backwardEnabled ) setBackward( compactFile, b );

                unsigned origF = f;
                setForward( compactFile, f );
                
                if ( probabilisticEnabled ) {
                    setProbabilistic( compactFile, origF );
                    assert_eq( origF, f );
                }
            }

            if ( backwardEnabled ) parent->backward->resize( b );
            parent->forward->resize( f );
            if ( probabilisticEnabled ) parent->probabilistic->resize( f );

            setLastTransitionIndices( b, f );
        }
    };

    /**
     * Class used for loading binary format compact state space files.
     */
    struct BinaryReader : CompactReader {

        BinaryReader( Compact* generator ) : CompactReader( generator ) {}

        /// Helper method used for reading ints from file
        template< typename T >
        void read( std::ifstream& compactFile, T& data ) {
            compactFile.read( reinterpret_cast< char* >( &data ), sizeof( T ) );
        }

        /// Size of binary header
        unsigned binaryHeaderSize() { return 6 * sizeof( unsigned ) + sizeof( bool ); }

        /// Offset of cumulative sum of transitions for state with id id
        unsigned binaryTransitionsOffset( StateId id, bool backwardEnabled ) { 
            return binaryHeaderSize() + ( id - 1 ) * ( backwardEnabled ? 2 : 1 ) * sizeof( unsigned ); 
        }

        /// Offset of state data for state with id id
        unsigned binaryStatesOffset( StateId id, bool backwardEnabled, unsigned backwardTransitionsBefore, unsigned forwardTransitionsBefore ) {
            return binaryTransitionsOffset( parent->statesCount + 1 + 1, backwardEnabled ) +
                   backwardTransitionsBefore * sizeof( unsigned ) +
                   forwardTransitionsBefore * ( sizeof( unsigned ) + ( parent->probabilistic ? sizeof( ProbabilisticTransition ) : 0 ) ) +
                   ( id - 1 ) * ( sizeof( AC ) + sizeof( unsigned ) * ( backwardEnabled ? 2 : 1 ) );
        }

        /// Reads compact state space from file
        void readInput( std::ifstream& compactFile, bool abortAfterFetchingHeader ) {
            // read header
            read( compactFile, parent->statesCount );
            // if we do not have enough states for this mpi process, we can abort
            if ( !allocMPIProcessStates() ) abortAfterFetchingHeader = true;
            
            bool probabilisticEnabled;
            read( compactFile, probabilisticEnabled );

            unsigned totalForwardCount, totalBackwardCount;
            read( compactFile, totalBackwardCount );
            read( compactFile, totalForwardCount );
            bool backwardEnabled = totalBackwardCount;

            read( compactFile, parent->initialState );

            unsigned acTypeNum;
            read( compactFile, acTypeNum );
            read( compactFile, parent->acCount );
            parent->acPropertyType = static_cast< PropertyType >( acTypeNum );

            if ( abortAfterFetchingHeader ) return; // abort reading before loading states

            Compact::master = parent;
            unsigned b = 0, f = 0;

            // compute state data offset based on number of transitions stored before requested state
            compactFile.seekg( binaryTransitionsOffset( parent->fromStateId, backwardEnabled ),
                               std::ios::beg );
            unsigned backwardOffset = 0, forwardOffset, backwardEndOffset = 0, forwardEndOffset;
            if ( backwardEnabled ) read( compactFile, backwardOffset );
            read( compactFile, forwardOffset );

            compactFile.seekg( binaryTransitionsOffset( parent->toStateId + 1, backwardEnabled ),
                               std::ios::beg );
            if ( backwardEnabled ) read( compactFile, backwardEndOffset );
            read( compactFile, forwardEndOffset );
            const unsigned backwardCount = backwardEndOffset - backwardOffset;
            const unsigned forwardCount = forwardEndOffset - forwardOffset;

            allocTransitions( backwardCount, forwardCount, probabilisticEnabled );

            // move to first state handled by this instance
            compactFile.seekg( binaryStatesOffset( parent->fromStateId, backwardEnabled,
                               backwardOffset, forwardOffset ), std::ios::beg );

            // for each state
            for ( StateId i = parent->fromStateId; i <= parent->toStateId; i++ ) {

                // initialize state's CompactState, BlobHeader, id, AC
                CompactState& compactState = initState( i, b, f );

                unsigned long acData;
                read( compactFile, acData );
                compactState.ac = AC( acData );

                // handle transitions
                if ( backwardEnabled ) {
                    unsigned backwardCount;
                    read( compactFile, backwardCount );
                    for ( unsigned j = 0; j < backwardCount; j++ ) {
                        StateId bt;
                        read( compactFile, bt );
                        addTransition( parent->backward, b++, bt );
                    }
                }

                unsigned forwardCount;
                read( compactFile, forwardCount );
                unsigned origF = f;

                for ( unsigned j = 0; j < forwardCount; j++ ) {
                    StateId ft;
                    read( compactFile, ft );
                    addTransition( parent->forward, f++, ft );
                }
                
                if ( probabilisticEnabled ) {
                    for ( unsigned j = 0; j < forwardCount; j++ ) {
                        ProbabilisticTransition pt;
                        read( compactFile, pt );
                        addProbabilisticTransition( origF++, pt );
                    }
                    assert_eq( origF, f );
                }
            }

            setLastTransitionIndices( b, f );
        }
    };

    /// Initializes compact state space by reading input file
    void read( std::string path ) {
        assert( !initialized );
        initialized = slackInitialized;

        // at this point we do not want to load states and transitions
        bool abortAfterFetchingHeader = false;
        if ( !slackInitialized ) abortAfterFetchingHeader = true;

        assert( peersCount );

        std::ifstream compactFile( path.c_str(), std::ios_base::in | std::ios_base::binary );
        const unsigned statesLabelSize = compact::cfStates.size();
        char possibleStatesLabel[ statesLabelSize + 1 ];
        compactFile.read( possibleStatesLabel, statesLabelSize );
        possibleStatesLabel[ statesLabelSize ] = 0;

        CompactReader* reader;

        if ( possibleStatesLabel == compact::cfStates ) {
            compactFile.close();
            compactFile.open( path.c_str() );
            reader = new TextReader( this );

        } else {
            compactFile.seekg( 0, std::ios::beg );
            reader = new BinaryReader( this );
        }

        reader->readInput( compactFile, abortAfterFetchingHeader );
        delete reader;

        compactFile.close();
    }
};

}
}

#endif
