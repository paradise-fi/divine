#ifndef DIVINE_UTILITY_BUCHI_H
#define DIVINE_UTILITY_BUCHI_H

#include <divine/ltl2ba/main.h>

#if OPT_LTL3BA
struct BState;
#endif

namespace divine {

class Buchi {

public:
    // conjunction of literals, true for positive literal, false for negative
    typedef std::vector< std::pair< bool, std::string > > DNFClause;

protected:
    struct BANode {
        std::vector< std::pair< int, int > > tr; // destination id and guard id
        bool isAcc;
    };

    std::vector< BANode > nodes;
    int initId;

#if OPT_LTL3BA
    static std::vector< DNFClause > gclause;
#endif

public:
    /*
     * Builds a buchi automaton from an LTL formula
     * Given functor/lambda allows the caller to assign a number to each guard (conjunction of literals),
     * is has to accept DNFClause and return an integer
     */
    template < typename Func >
    bool build( const std::string& ltl, Func guardId ) {
#if OPT_LTL3BA
    return build3ba( ltl, guardId );
#else
    return build2ba( ltl, guardId );
#endif
    }

    template < typename Func >
    bool build2ba( const std::string& ltl, Func guardId ) {
        nodes.clear();
        initId = -1;

        BA_opt_graph_t b = buchi( ltl );
        auto nodeList = b.get_all_nodes();
        nodes.resize( nodeList.size() );

        for ( auto n = nodeList.begin(); n != nodeList.end(); ++n ) {
            int nid = (*n)->name - 1;
            assert( nid >= 0 && nid < int( nodes.size() ) );
            nodes[ nid ].isAcc = (*n)->accept;
            if ( (*n)->initial ) {
                assert( initId < 0 );
                initId = nid;
            }

            for ( auto tr = b.get_node_adj( *n ).begin(); tr != b.get_node_adj( *n ).end(); ++tr ) {
                int dstid = tr->target->name - 1;

                DNFClause clause;
                clause.reserve( tr->t_label.size() );
                for ( auto lit = tr->t_label.begin(); lit != tr->t_label.end(); ++lit )
                    clause.push_back( make_pair( lit->negace, lit->predikat ) );
                int guard = guardId( clause );

                nodes[ nid ].tr.push_back( std::make_pair( dstid, guard ) );
            }
        }

        return initId >= 0;
    }

#if OPT_LTL3BA
private:
    // parses ltl formula, returs zero on success
    int ltl3ba_parse( std::string ltl );

    // free memory
    void ltl3ba_finalize();

    // retrieves pointer to the first state
    BState *nodeBegin();

    // retrieves pointer past the last state
    BState *nodeEnd();

    // true if state is accepting
    bool isAccepting( BState* );

    // true if state is initial (only one is initial)
    bool isInitial( BState* );

    // moves pointer to the next state
    void next( BState*& );

    // retrieves previously assigned id
    int getId( BState* );

    // assign custom id to the given state
    void assignId( BState*, int );

    // returns number of edges leadig from the given state
    int getEdgeCount( BState* );

    // parses i-th edge of the given state by putting zero or more DNF clauses into (static) gclause and returning id of the destination state
    int parseEdge( BState*, int );

    // callback for printing guards, has to be static
    static void allsatHandler( char* varset, int size );

public:
    // build automaton using the ltl3ba library
    // methods defined above are used to operate with the BState structures to avoid cluttering the global namespace by including ltl3ba.h directly
    template < typename Func >
    bool build3ba( const std::string& ltl, Func guardId ) {
        nodes.clear();
        initId = -1;
        int ret = ltl3ba_parse( ltl );
        if ( ret )
            return false;

        for( BState* s = nodeBegin(); s != nodeEnd(); next( s ) ) {
            assignId( s, nodes.size() );
            nodes.push_back( BANode() );
            nodes.back().isAcc = isAccepting( s );
            if ( isInitial( s ) ) {
                assert( initId < 0 );
                initId = getId( s );
            }
        }

        for( BState* s = nodeBegin(); s != nodeEnd(); next( s ) ) {
            for( int i = 0; i < getEdgeCount( s ); ++i ) {
                int destId = parseEdge( s, i );
                for ( auto cl = gclause.begin(); cl != gclause.end(); ++cl ) {
                    int guard = guardId( *cl );
                    nodes[ getId( s ) ].tr.push_back( std::make_pair( destId, guard ) );
                }
            }
        }

        if ( nodeBegin() == nodeEnd() ) {   // LTL3BA can produce automaton with no states
            nodes.push_back( BANode() );
            nodes.back().isAcc = false;
            initId = 0;
        }

        assert( initId >= 0 );
        ltl3ba_finalize();
        return true;
    }
#endif

public:
    // adds state, returns its id
    int addState() {
        nodes.emplace_back();
        return nodes.size() - 1;
    }

    int duplicateState( int src ) {
        nodes.push_back( nodes[ src ] );
        return nodes.size() - 1;
    }

    void setAccepting( int id, bool isAcc ) {
        nodes[ id ].isAcc = isAcc;
    }

    // builds BA containing single (non-accepting) state with self-loop
    void buildEmpty();

    bool isAccepting( int nodeId ) const {
        return nodes[ nodeId ].isAcc;
    }

    /*
     * Obtain outgoing transitions from given node
     * represented as pairs of destination node ids and custom guard numbers assigned during the construction
     */
    const std::vector< std::pair< int, int > >& transitions( int nodeId ) const {
        return nodes[ nodeId ].tr;
    }

    std::vector< std::pair< int, int > >& editableTrans( int nodeId ) {
        return nodes[ nodeId ].tr;
    }

    int getInitial() const {
        return initId;
    }

    unsigned int size() const {
        return nodes.size();
    }

    typedef std::map< std::string, std::string > StringMap;

    // read LTL file, builds list of properties and definitions
    // returns true if at leas one property was read
    static bool readLTLFile( const std::string& fname, std::vector< std::string >& props, StringMap& defs );

    // prints automaton constructed from given property in dve format
    static std::ostream& printAutomaton( std::ostream& o, const std::string& property, const StringMap& defs = StringMap() );
};

}

#endif
