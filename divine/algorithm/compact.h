// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/report.h>
#include <divine/compactstate.h>

#include <iostream>
#include <fstream>

#ifndef DIVINE_ALGORITHM_COMPACT_H
#define DIVINE_ALGORITHM_COMPACT_H

namespace divine {

/// Compact file string literals
namespace compact {
    const std::string cfStates = "states:";
    const std::string cfBackward = "backward:";
    const std::string cfForward = "forward:";
    const std::string cfInitial = "initial:";
    const std::string cfAC = "ac:";
    const std::string cfState = "state:";
}

namespace algorithm {

template< typename, typename > struct Compact;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G, typename S >
struct _MpiId< Compact< G, S > >
{
    typedef Compact< G, S > A;

    static int to_id( void (A::*f)() ) {
        if ( f == &A::_visit ) return 0;
        if ( f == &A::_dumpTextCompactRepresentation ) return 4;
        if ( f == &A::_dumpBinaryCompactRepresentation ) return 5;
        if ( f == &A::_por) return 7;
        if ( f == &A::_por_worker) return 8;
        assert_die();
    }

    static void (A::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_visit;
            case 4: return &A::_dumpTextCompactRepresentation;
            case 5: return &A::_dumpBinaryCompactRepresentation;
            case 7: return &A::_por;
            case 8: return &A::_por_worker;
            default: assert_die();
        }
    }

    template< typename T, typename O >
    static O write( T &data, O o ) {
        *o++ = data.size();
        for ( typename T::const_iterator it = data.begin(); it != data.end(); ++it )
            *o++ = *it;
        return o;
    }

    template< typename T, typename I >
    static I read( T &data, I i ) {
        const unsigned size = *i++;
        data.resize( size );
        for ( unsigned p = 0; p < size; p++ ) data[ p ] = *i++;
        return i;
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        o = s.stats.write( o );
        o = s.localStats.write( o );
        *o++ = s.initialTable;
        *o++ = s.findBackEdges;
        *o++ = s.haveInitial;

        o = write( s.statesCountSums, o );
        o = write( s.inTransitionsCountSums, o );
        o = write( s.outTransitionsCountSums, o );
        o = write( s.compactFile, o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        i = s.stats.read( i );
        i = s.localStats.read( i );
        s.initialTable = *i++;
        s.findBackEdges = *i++;
        s.haveInitial = *i++;

        i = read( s.statesCountSums, i );
        i = read( s.inTransitionsCountSums, i );
        i = read( s.outTransitionsCountSums, i );
        i = read( s.compactFile, i );

        return i;
    }
};
// END MPI drudgery

/**
 * Distributed compactization of the state space
 */
template< typename G, typename Statistics >
struct Compact : virtual Algorithm, AlgorithmUtils< G >, DomainWorker< Compact< G, Statistics > >
{
    typedef Compact< G, Statistics > This;
    typedef typename G::Node Node;

    /// Internal structure for computing final state index
    struct StateRef {
        unsigned index; // local index
        unsigned from; // peer number

        StateRef( unsigned index, unsigned from ) : index( index ), from( from ) {}

        StateRef() : index(0), from(0) {}
    };

    /// Stores count of handled states and transitions
    struct LocalStatistics {
        int states, incomingTransitions, outgoingTransitions;

        LocalStatistics() : states( 0 ), incomingTransitions( 0 ), outgoingTransitions( 0 ) {}

        void addState() { ++states; }

        void addInTransition() {
            ++incomingTransitions;
        }

        void addOutTransition() {
            outgoingTransitions++;
        }

        template< typename O >
        O write( O o ) {
            *o++ = states;
            *o++ = incomingTransitions;
            *o++ = outgoingTransitions;
            return o;
        }

        template< typename I >
        I read( I i ) {
            states = *i++;
            incomingTransitions = *i++;
            outgoingTransitions = *i++;
            return i;
        }

        void merge( LocalStatistics other ) {
            states += other.states;
            incomingTransitions += other.incomingTransitions;
            outgoingTransitions += other.outgoingTransitions;
        }
    };

    struct Shared {
        algorithm::Statistics< G > stats;
        LocalStatistics localStats;
        G g;
        int initialTable;
        bool need_expand;

        // cumulative sums of numbers of states and transitions on each node + total sum
        std::vector< unsigned > statesCountSums, inTransitionsCountSums, outTransitionsCountSums; 
        std::string compactFile; // output file name
        bool findBackEdges; // find also backward transitions
        bool haveInitial; // signifies that this peer handles the initial state

        Shared() : haveInitial( false ), /*keepOriginal( false ),*/ findBackEdges( false ) {}
    } shared;

    std::deque< Node > states; // stores all states handled by this peer
    LocalStatistics localStats; // local backup of statistics
    std::string compactFile; // output file holder
    unsigned initialPeer; // globalId of peer with initial state
    bool plaintextFormat; // output compact state space in plaintext format

    /// Returns reference to the domain instance
    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

    /// Algorithm Extension structure
    struct Extension {
        int index:31; // local state index
        bool back:1; // internal helper, denotes state being sent back through original forward transition
        unsigned succsCount; // number of successors
        StateRef *succs; // references state successors
        std::vector< StateRef > *preds; // references state predecessors
    };

    /// Returns algorithm's extension of state
    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    /// Adds new state to statistics
    void statsAddState( G &g, Node &node ) {
        shared.stats.addNode( g, node );
        shared.localStats.addState();
        localStats.addState();
    }

    /// Adds new backward edge to statistics
    void statsAddInTransition() {
        shared.stats.addEdge();
        if ( shared.findBackEdges ) {
            localStats.addInTransition();
            shared.localStats.addInTransition();
        }
    }

    /// Adds new forward edge to statistics
    void statsAddOutTransition() {
        localStats.addOutTransition();
        shared.localStats.addOutTransition();
    }

    /// Returns a number of successors of state st
    unsigned countSuccessors( Node st ) {
        typename G::Successors succs = shared.g.successors( st ); 
        unsigned succsCount = 0;
        while ( !succs.empty() ) {
            shared.g.release( succs.head() );
            succs = succs.tail();
            succsCount++;
        }
        return succsCount;
    }

    /// Expands state, handles incoming transitions (by sending notification to the owner)
    void updateNode( Node &from, Node &to ) {
        Extension &toExt = extension( to );

        // if state extension is not initialized yet (it hasn't been expanded yet)
        if ( toExt.index == 0 ) {
            assert( toExt.preds == NULL );
            assert( toExt.succs == NULL );
            assert_eq( toExt.succsCount, 0 );

            toExt.index = localStats.states + 1;

            StateRef nodeRef( toExt.index, this->globalId() );
            states.push_back( to );

            if ( shared.findBackEdges )
                toExt.preds = new std::vector< StateRef >();

            toExt.succsCount = countSuccessors( to );
            toExt.succs = new StateRef[ toExt.succsCount ];
        }

        // add backward transition reference, send notification to from owner
        if ( from.valid() ) {
            statsAddInTransition();
            int fromOwner = visitor->owner( from );
            if ( shared.findBackEdges ) {
                StateRef fromRef( extension( from ).index, fromOwner );
                toExt.preds->push_back( fromRef );
            }
            Node copy( shared.g.g().pool(), to.size() );
            to.copyTo( copy );
            extension( copy ).back = true;
            visitor->queueAny( copy, from, fromOwner ); // goes to the owner of from
        } else {
            shared.haveInitial = true; // this peer owns the initial state
        }
    }

    /// Visitor's expansion implementation
    visitor::ExpansionAction expansion( Node st ) {
        statsAddState( shared.g, st );
        shared.g.porExpansion( st );
        return visitor::ExpandState;
    }

    /**
     * Visitor's transition implementation. Handles notifications about indices
     * of states on forward transitions.
     */
    visitor::TransitionAction transition( Node f, Node t ) {
        // store the forward state index
        if ( f.valid() && extension( f ).back ) {
            statsAddOutTransition();
            StateRef originalToRef( extension( f ).index, visitor->owner( f ) );

            extension( t ).succs[ shared.g.successorNum( *this, t, f ) - 1 ] = originalToRef;
            f.free( shared.g.g().pool() );
            return visitor::IgnoreTransition;
        }

        shared.g.porTransition( f, t, 0 );

        updateNode( f, t );

        return visitor::FollowTransition;
    }

    /// Visitor's default deadlocked implementation
    typedef visitor::Setup< G, This, typename AlgorithmUtils< G >::Table, Statistics > VisitorSetup;

    typedef visitor::Partitioned< VisitorSetup, This, Hasher > Visitor;

    Visitor *visitor; // Visitor reference

    /// Replaces received stats by local backup copy
    void loadLocalStats() {
        shared.localStats = localStats;
    }

    /// Called in parallel. State space exploration.
    void _visit() {
        loadLocalStats();
        this->initPeer( &shared.g, &shared.initialTable, this->globalId() );

        Visitor visitor( shared.g, *this, *this, hasher, &this->table() );
        this->visitor = &visitor;
        shared.g.queueInitials( visitor );
        visitor.processQueue();
        this->visitor = NULL; // we no longer need it
    }

    /**
     * Base class for dumping compact state space to a file
     */
    struct CompactWriter {

        This* parent; // pointer to algorithm::Compact
        std::ostream *out; // output stream holder

        CompactWriter( This *alg ) : parent( alg ), out( 0 ) {}

        /// Acceptance condition information
        void getAC( generator::PropertyType& acType, int& acCount, bool& acPair ) {
            if ( acType = parent->shared.g.propertyType() ) {
                if ( acType != generator::AC_None )
                    acCount = parent->shared.g.acceptingGroupCount();
                if ( acType == generator::AC_Rabin || acType == generator::AC_Streett )
                    acPair = true;
            }
        }

        /// Opens default output, seeks a position for binary, appends to text
        void openOutput( bool binary = false, unsigned offset = 0 ) {
            assert( parent->shared.compactFile != "" || !binary );

            if ( out == NULL ) {
                if ( parent->shared.compactFile == "" )
                    out = &std::cout;
                else
                    out = new std::ofstream( parent->shared.compactFile.c_str(), std::ios_base::out |
                        ( binary ? std::ios_base::binary | std::ios_base::in : std::ios_base::app ) );
            }

            if ( binary ) out->seekp( offset, std::ios::beg );
        }

        /// Closes default output
        void closeOutput() {
            if ( parent->shared.compactFile != "" ) {
                std::ofstream *fout = static_cast< std::ofstream* >( out );
                fout->close();
                delete fout;
            }
            out = NULL;
        }

        /// Empties default output
        void prepareOutput( bool binary = false ) {
            if ( parent->compactFile == "" && binary ) {
                parent->compactFile = "a.compact";
                std::cerr << "Binary output is not enabled for standard output. Dumping to ./" <<
                        parent->compactFile << " instead." << std::endl;
            }
            if ( parent->compactFile != "" ) {
                std::ofstream f( parent->compactFile.c_str(), std::ios_base::trunc );
                f.close();
                parent->shared.compactFile = parent->compactFile;
            }
        }

        /// Copies contents of original model source file to the default output stream
        void copyOriginal() {
            std::ifstream original( parent->m_config->input.c_str() );
            while ( !original.eof() ) {
                char buf[ 4096 ];
                original.read( buf, 4096 );
                const unsigned read = original.gcount();
                out->write( buf, read );
            }
            original.close();
        }

        /// Dumps compact state space to the default output stream
        virtual void writeOutput() = 0;
    };

    /**
     * Dumps compact state space to a plaintext file
     */
    struct TextWriter : CompactWriter {

        This& alg() { return *this->parent; }

        G& g() { return this->parent->shared.g; }

        typename This::Shared& shared() { return this->parent->shared; }

        TextWriter( This *alg ) : CompactWriter( alg ) {}

        /// Acceptance condition format
        void printACSign( bool belongsTo ) {
            *this->out << ( belongsTo ? '+' : '-' );
        }

        /// Dumps states handled by a single peer
        void dumpCompactRepresentation() {
            int acCount = 0; // number of accepting sets
            bool acPair = false; // accepting sets or pairs of sets
            generator::PropertyType acType;

            this->getAC( acType, acCount, acPair );

            this->openOutput();

            unsigned i = 0;
            // for each state
            for ( typename std::deque< Node >::iterator it = alg().states.begin();
                  it != alg().states.end(); ++it, i++ ) {
                const Node &target = *it;
                Extension &targetExt = alg().extension( target );

                // state index and acceptance condition
                *this->out << compact::cfState << " " << (
                        shared().statesCountSums[ alg().globalId() ] + i + 1 );
                *this->out << " ";
                // store accepting condition
                for ( unsigned j = 0; j < acCount; j++ ) {
                    if ( acPair ) {
                        printACSign( g().isInAccepting( *it, j ) );
                        printACSign( g().isInRejecting( *it, j ) );
                    }
                    else
                        printACSign( g().isInAccepting( *it, j ) );
                }
                *this->out << std::endl;

                // store backward transitions
                if ( shared().findBackEdges ) {
                    *this->out << compact::cfBackward << " ";
                    for ( typename std::vector< StateRef >::const_iterator bit =
                          targetExt.preds->begin(); bit != targetExt.preds->end(); ++bit )
                        *this->out << shared().statesCountSums[ bit->from ] + bit->index << " ";
                    *this->out << std::endl;
                    delete targetExt.preds;
                }

                // store forward transitions
                *this->out << compact::cfForward << " ";
                for ( StateRef *fit = targetExt.succs; fit != targetExt.succs +
                      targetExt.succsCount; ++fit )
                    *this->out << shared().statesCountSums[ fit->from ] + fit->index << " ";
                *this->out << std::endl;
                delete[] targetExt.succs;

                it->header().permanent = false;
                g().release( *it );
            }

            this->closeOutput();
        }

        /// Outputs compact state space header information
        void initializeOutput() {
            this->prepareOutput();
            this->openOutput();
            *this->out << compact::cfStates << " " << alg().localStats.states << std::endl;
            *this->out << compact::cfBackward << " " << ( shared().findBackEdges ?
                    alg().localStats.incomingTransitions : 0 ) << std::endl;
            *this->out << compact::cfForward << " " <<
                    alg().localStats.outgoingTransitions << std::endl;
            *this->out << compact::cfInitial << " " <<
                    shared().statesCountSums[ alg().initialPeer ] + 1 << std::endl;

            *this->out << compact::cfAC << " ";
            generator::PropertyType acType;
            if ( ( acType = g().propertyType() ) != generator::AC_None )
                *this->out << acType << " " << g().acceptingGroupCount();
            else
                *this->out << generator::AC_None;
            *this->out << std::endl;

            this->closeOutput();
        }

        /// Separates the compact state space data from the original model
        void printBanner() {
            const unsigned inputFilenameLength = alg().m_config->input.size();
            *this->out << std::endl;
            *this->out << "******************";
            for ( unsigned i = 0; i < inputFilenameLength; i++ ) this->out->put( '*' );
            *this->out << "**" << std::endl;
            *this->out << "* Original model: " << alg().m_config->input << " *" << std::endl;
            *this->out << "******************";
            for ( unsigned i = 0; i < inputFilenameLength; i++ ) this->out->put( '*' );
            *this->out << "**" << std::endl;
            *this->out << std::endl;
        }

        /// Appends original model data
        void appendOriginal() {
            this->openOutput();

            printBanner();

            this->copyOriginal();

            this->closeOutput();
        }

        /// Dumps the compact state space to a file
        void writeOutput() {
            initializeOutput();
            alg().domain().parallel().runInRing( shared(), &This::_dumpTextCompactRepresentation );
            appendOriginal();
        }
    };

    /**
     * Dumps compact state space to a binary file
     */
    struct BinaryWriter : CompactWriter {

        This& alg() { return *this->parent; }

        G& g() { return this->parent->shared.g; }

        typename This::Shared& shared() { return this->parent->shared; }

        BinaryWriter( This *alg ) : CompactWriter( alg ) {}

        /// Header size
        unsigned binaryHeaderSize() { return 6 * sizeof( unsigned ); }

        /// Offset of cumulative sum of transitions for particular peer
        unsigned binaryTransitionsOffset( unsigned idPeer ) {
            return binaryHeaderSize() + ( shared().statesCountSums[ idPeer ] +
                    ( idPeer == alg().peers() ? 1 : 0 ) ) *
                    ( shared().findBackEdges ? 2 : 1 ) * sizeof( unsigned );
        }

        /// Offset of states of particular peer
        unsigned binaryStatesOffset( unsigned idPeer ) {
            return binaryTransitionsOffset( alg().peers() ) +
                    ( shared().inTransitionsCountSums[ idPeer ] +
                    shared().outTransitionsCountSums[ idPeer ] ) * sizeof( unsigned ) +
                    shared().statesCountSums[ idPeer ] * ( sizeof( AC ) +
                    sizeof( unsigned ) * ( shared().findBackEdges ? 2 : 1 ) );
        }

        /// Dumps states of one peer to a file
        void dumpCompactRepresentation() {
            int acCount = 0; // number of accepting sets
            bool acPair = false; // accepting sets or pairs of sets
            generator::PropertyType acType;

            this->getAC( acType, acCount, acPair );

            this->openOutput( true, binaryTransitionsOffset( alg().globalId() ) );

            unsigned b = 0, f = 0;
            // for each of our states
            for ( typename std::deque< Node >::iterator it = alg().states.begin();
                  it != alg().states.end(); ++it ) {
                const Node &target = *it;
                Extension &targetExt = alg().extension( target );

                // store backward transitions cumulative sum
                if ( shared().findBackEdges ) {
                    write( shared().inTransitionsCountSums[ alg().globalId() ] + b );
                    b += targetExt.preds->size();
                }

                // store forward transitions cumulative sum
                write( shared().outTransitionsCountSums[ alg().globalId() ] + f );
                f += targetExt.succsCount;
            }
            // store total sums
            if ( alg().globalId() == alg().peers() - 1 ) {
                if ( shared().findBackEdges )
                    write( shared().inTransitionsCountSums[ alg().peers() ] );
                write( shared().outTransitionsCountSums[ alg().peers() ] );
            }

            // move to the block that should contain state data of this peer
            openOutput( true, binaryStatesOffset( alg().globalId() ) );

            unsigned i = 0;
            // for each of our states
            for ( typename std::deque< Node >::iterator it = alg().states.begin();
                  it != alg().states.end(); ++it, i++ ) {
                const Node &target = *it;
                Extension &targetExt = alg().extension( target );

                // store accepting condition
                AC acData;
                for ( unsigned j = 0; j < acCount; j++ ) {
                    if ( acPair ) {
                        acData[ j * 2 ] = g().isInAccepting( *it, j );
                        acData[ j * 2 + 1 ] = g().isInRejecting( *it, j );
                    }
                    else
                        acData[ j ] = g().isInAccepting( *it, j );
                }
                write( acData.to_ulong() );

                // store backward transitions
                if ( shared().findBackEdges ) {
                    write( static_cast< unsigned >( targetExt.preds->size() ) );
                    for ( typename std::vector< StateRef >::const_iterator bit =
                          targetExt.preds->begin(); bit != targetExt.preds->end(); ++bit )
                        write( shared().statesCountSums[ bit->from ] + bit->index );
                    delete targetExt.preds;
                }

                // store forward transitions
                write( targetExt.succsCount );
                for ( StateRef *fit = targetExt.succs;
                      fit != targetExt.succs + targetExt.succsCount; ++fit )
                    write( shared().statesCountSums[ fit->from ] + fit->index );
                delete[] targetExt.succs;

                it->header().permanent = false;
                g().release( *it );
            }

            this->closeOutput();
        }

        /// Helper to store data to the binary file
        template< typename T >
        void write( T num ) {
            this->out->write( reinterpret_cast< char* >( &num ), sizeof( T ) );
        }

        /// Writes compact states space header
        void initializeOutput() {
            this->prepareOutput( true );
            this->openOutput( true );
            write( alg().localStats.states );
            write( ( shared().findBackEdges ? alg().localStats.incomingTransitions : 0 ) );
            write( alg().localStats.outgoingTransitions );
            write( shared().statesCountSums[ alg().initialPeer ] + 1 );
            generator::PropertyType acType;
            if ( ( acType = shared().g.propertyType() ) != generator::AC_None ) {
                write( acType );
                write( g().acceptingGroupCount() );
            }
            else {
                write( generator::AC_None );
                write( 0 );
            }
            this->closeOutput();
        }

        /// Appends the original model to the default output stream
        void appendOriginal() {
            openOutput( true, binaryStatesOffset( alg().peers() ) );

            std::string prefix = alg().m_config->input + ": ";
            this->out->write( prefix.c_str(), prefix.size() );
            this->copyOriginal();

            this->closeOutput();
        }

        /// Dumps the compact state space to the file
        void writeOutput() {
            initializeOutput();
            alg().domain().parallel().run( shared(),
                &This::_dumpBinaryCompactRepresentation );
            appendOriginal();
        }
    };

    /** Called in ring. Outputs compact state space representation 
     * to the default output in plaintext format.
     */
    void _dumpTextCompactRepresentation() {
        TextWriter writer( this );
        writer.dumpCompactRepresentation();
    }

    /** Called in parallel. Outputs compact state space representation 
     * to the default output in binary format.
     */
    void _dumpBinaryCompactRepresentation() {
        this->restart();

        BinaryWriter writer( this );
        writer.dumpCompactRepresentation();

        while ( !this->idle() ) ; // let mpi know that we are finished here
    }

    /// Called from master. Initializes the compact state space representation.
    void initCompactRepresentation() {
        std::vector< unsigned > states( this->peers() + 1), inTransitions( this->peers() + 1),
                                        outTransitions( this->peers() + 1);
        states[ 0 ] = inTransitions[ 0 ] = outTransitions[ 0 ] = 0;
        for ( int i = 1; i <= this->peers(); i++ ) {
            states[ i ] = states[ i - 1 ] + this->master().shared( i - 1 ).localStats.states;
            inTransitions[ i ] = inTransitions[ i - 1 ] +
                    this->master().shared( i - 1 ).localStats.incomingTransitions;
            outTransitions[ i ] = outTransitions[ i - 1 ] +
                    this->master().shared( i - 1 ).localStats.outgoingTransitions;
        }
        shared.statesCountSums = states;
        shared.inTransitionsCountSums = inTransitions;
        shared.outTransitionsCountSums = outTransitions;
    }

    void _por_worker() {
        shared.g._porEliminate( *this, hasher, this->table() );
    }

    void _por() {
        loadLocalStats();
        if ( shared.g.porEliminate( domain(), *this ) )
            shared.need_expand = true;
    }

    /// Constructs Compact algorithm
    Compact( Config *c = 0 ) : Algorithm( c, sizeof( Extension ) ), 
        compactFile( "" ), initialPeer( 0 ) {
        if ( c ) {
            this->initPeer( &shared.g );
            this->becomeMaster( &shared, workerCount( c ) );
            shared.initialTable = c->initialTable;
            compactFile = c->compactFile;
            plaintextFormat = c->textFormat;
            shared.findBackEdges = c->findBackEdges;
        }
    }

    /// Handles statistics
    void collect() {
        localStats = LocalStatistics();

        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            shared.stats.merge( s.stats );
            localStats.merge( s.localStats );
        }
        shared.stats.updateResult( result() );
        shared.stats = algorithm::Statistics< G >();
    }

    /// Stores which peer owns the initial state
    void findInitial() {
        for ( int i = 0; i < domain().peers(); ++i ) {
            if ( domain().shared( i ).haveInitial )
                initialPeer = i;
        }
    }

    /// Runs the Compact algorithm. This is expected to run only once per instance.
    Result run() {
        // first we explore the original state space
        do 
        {
            domain().parallel().run( shared, &This::_visit );
            collect();
            findInitial();
            shared.need_expand = false;
            domain().parallel().runInRing( shared, &This::_por );
        } while ( shared.need_expand );

        // then we construct compact representation
        initCompactRepresentation();

        CompactWriter* writer;

        if ( plaintextFormat )
            writer = new TextWriter( this );
        else
            writer = new BinaryWriter( this );

        writer->writeOutput();
        delete writer;

        result().fullyExplored = Result::Yes;
        shared.stats.updateResult( result() );
        return result();
    }
};

}
}

#endif
