// -*- C++ -*- (c) 2010 Milan Krivanek
//             (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
#ifndef DIVINE_GENERATOR_COINGENERATOR_H
#define DIVINE_GENERATOR_COINGENERATOR_H

#include <stdexcept>
#include <list>
#include <sstream>
#ifdef LCA
#include <unordered_set>
#endif

#include <divine/generator/common.h>
#include <divine/coin/state.h>
#include <divine/coin/transition.h>

extern FILE * yyin;

namespace divine {
namespace generator {

using namespace coin_seq;
using coin_seq::transition_t;

using std::list;

/**
 * Computes a hash of a label using the recipe from Effective Java.
 */
struct label_hasher {
    size_t operator()(const label_t & l) const {
        int result = 17;
        result = 31 * result + l.fromc_id;
        result = 31 * result + l.toc_id;
        result = 31 * result + l.act_id;
        return result;
    }
};

/**
 * unordered set from the C++0x STL proposal.
 */
#ifdef LCA
typedef std::tr1::unordered_set<label_t, label_hasher> label_set;
#endif

class Coin: public Common< Blob > {

public:
    typedef Blob Node;
    typedef typename Common< Blob >::Label Label;
    typedef Coin Graph;
    typedef generator::Common< Blob > Common;

    struct Successors {
        typedef Node Type;

        /**
         * Empty constructor
         */
        Successors() {}

        /**
         * Creates the collection of successors using a list of nodes.
         *
         * \param st  from node
         * \param l   list of successor nodes
         */
        Successors(Node st, std::list<Node> l): succs(l), _from(st) {}

        std::list<Node> succs;
        Node _from;

        /**
         * Tests if the collection is empty.
         */
        bool empty() const {
            return succs.empty();
        }

        /**
         * Returns the source node.
         *
         * \return source node
         */
        Node from() {
            return _from;
        }

        /**
         * Returns the list of successors without the first element.
         */
        Successors tail() const {
            Successors s = *this;
            s.succs.pop_front();
            return s;
        }

        /**
         * Returns the first element of the collection.
         */
        Node head() {
            return succs.front();
        }
    };

    /**
     * Empty constructor of the CoIn state space generator.
     */
    Coin();

    /**
     * Copy constructor.
     */
    Coin(const Coin & other);

    /**
     * Destructor
     */
    ~Coin();

    /**
     * Enables partial order reduction.
     */
    void initPOR();

    /**
     * Returns the initial node.
     *
     * \return initial node
     */
    template< typename Alloc >
    Node _initial( Alloc alloc ) {
        getCoinSystem();

        Blob b = newNode( alloc );
        packPrimitiveStates(primitive_states, state_vector);
        State s(primitive_states);
        s.pack(pool(), b, this->slack());

        return b;
    }

    template< typename Alloc, typename Yield >
    void initials( Alloc alloc, Yield yield ) {
        yield( Node(), _initial( alloc ), Label() );
    }

    template< typename Alloc >
    Successors _successors( Alloc alloc, Node compressed_state ) {
        vector<transition_t *> * enabled_trans = getEnabledTrans(compressed_state);

        return apply( alloc, compressed_state, enabled_trans );
    }

    template< typename Alloc >
    Successors _ample( Alloc alloc, Node compressed_state ) {
        ASSERT(por);

        vector<transition_t *> * enabled_trans = getEnabledTrans(compressed_state);

        int i = getAmpleSet(compressed_state);
        if (i < 0) {
            i = findAmpleSet(enabled_trans); //try to find an ample set
            //std::cout << "Found ample set ID: " << i << std::endl;
            if (i == -1) { // not found
                i = system_aut_id; // fully expand
            }
            setAmpleSet(compressed_state, i); // store ample set ID
        } else {
            //std::cout << "Stored ample set ID: " << i << std::endl;
        }

        ASSERT(i >= 0);

        if (i != system_aut_id) {
            //if non-empty ample set that is a proper subset of system automata was found
            vector<transition_t *>::iterator it;

            //resetting of the in_composition flags for all automata, then setting them for I
            setCandidateFlags(i);

            //filtering enabled transitions that do not belong to automata in I
            vector<transition_t *> * ample_trans = new vector<transition_t *> ;
            for (it = enabled_trans->begin(); it != enabled_trans->end(); it++) {
                transition_t * tr = *it;
                if (tr->is_sync()) {
                    if ((!tree_nodes[tr->get_automaton()]->in_composition)
                            && (!tree_nodes[tr->get_automaton2()]->in_composition)) {
                        delete tr;
                        continue;
                    }
                } else {
                    if (!tree_nodes[tr->get_automaton()]->in_composition) {
                        delete tr;
                        continue;
                    }
                }
                ample_trans->push_back(tr);
            }
            delete enabled_trans;
            return apply( alloc, compressed_state, ample_trans );
        } else {
            return apply( alloc, compressed_state, enabled_trans );
        }
    }

    template< typename Alloc, typename Yield >
    void successors( Alloc alloc, Node n, Yield yield ) {
        Successors s = _successors( alloc, n );
        while ( !s.empty() ) {
            yield( s.head(), Label() );
            s = s.tail();
        }
    }

    template< typename Alloc, typename Yield >
    void ample( Alloc alloc, Node n, Yield yield ) {
        Successors s = _ample( alloc, n );
        while ( !s.empty() ) {
            yield( s.head(), Label() );
            s = s.tail();
        }
    }

    /**
     * Deallocates a node.
     *
     * \param node  node to be deallocated
     */
    template< typename Alloc >
    void release( Alloc alloc, Node n ) {
        alloc.drop( pool(), n );
    }

    template< typename Yield >
    void enumerateFlags( Yield ) { } // no flags supported beyond implicit accepting and goal

    template< typename QueryFlags >
    graph::FlagVector stateFlags( Node s, QueryFlags qf ) {
        // ID of the current state of the property automaton = the last element of the vector
        unsigned int sid = State::getPrimitiveState( pool(), s, this->slack(), packed_state_size - 1 );

        for ( auto f : qf )
            if ( f == graph::flags::accepting && property
                    && property->accepting_states.find( sid ) != property->accepting_states.end() )
                return { graph::flags::accepting };
        return { };
    }

    /**
     * Converts compressed_state to a string
     *
     * \return string representation of compressed_state
     */
    std::string showNode(Node compressed_state);

    /// currently only dummy method
    std::string showTransition( Node from, Node to, Label );

    /**
     * Initializes the generator using the given file
     *
     * \param path  input file
     */
    void read(std::string path, std::vector< std::string > /* definitions */,
            Coin * = nullptr );

    /**
     * Assignment operator
     */
    Coin & operator=( const Coin &other );

    /**
     * Sets the extra space allocated for every Node.
     */
    int setSlack( int s );

private:

    static int count;
    int id;

    int state_size;
    int prop_aut_id; //id of the property automaton
    vector<int> state_vector; //current configuration
    vector<int> next_state_vector; //successor configuration
    // the number of simple automata (plus the property automaton, if available)
    int packed_state_size;
    vector<int> primitive_states;
    std::string file;

    bool por;

    coin_system_t * coin_system;

    int system_aut_id; //id of the automaton representing the system
    symtable<aut_t> * automata; //table of all automata
    int automata_size; //cached size of <code>automata</code>
    prop_t * property; //the property automaton

    coin_system_t * getCoinSystem(); //returns the model of the system

    /**
     * Returns the number of bytes necessary to store
     * the given number of states.
     *
     * \param number_of_states     the number of states of a simple automaton
     * \return                     the number of bytes to store the states
     */
    size_t getSize(unsigned int number_of_states);

    /**
     * Initialize the generator,
     * set the vector of initial states (initial configuration).
     */
    void init();

    /**
     * Recursive algorithm computing the successors
     *
     * \param aid  automaton ID, the root of the currently processed subtree
     * \return successors from the given subtree
     */
    vector<sometrans> getSuccTransitions(int aid);

    /**
     * Calculate the enabled transitions of the whole system.
     */
    vector<transition_t *> * getEnabledTrans(Node compressed_state);

    /**
     * Combine the transitions with the property automaton.
     */
    vector<transition_t *> * combineWithProp(vector<transition_t *> * system_trans);

    template< typename Alloc >
    Successors apply( Alloc alloc, Node st, vector<transition_t *> * succ_trans ) {

        // if property, combine enabled transitions with property
        if (property) {
            vector<transition_t *> * combined_trans;
            combined_trans = combineWithProp(succ_trans);
            // leak corrected
            for (vector<transition_t *>::iterator i = succ_trans->begin();
                    i != succ_trans->end(); i++) {
                delete *i;
            }
            //
            delete succ_trans;
            succ_trans = combined_trans;
        }

        // we can now compute the effect of effective transitions
        std::list<Node> succs_list;
        for (vector<transition_t *>::iterator i = succ_trans->begin();
                i != succ_trans->end(); i++) {
            transition_t * t = *i;
            t->get_effect(state_vector, next_state_vector);

            Blob b = newNode( alloc );
            packPrimitiveStates(primitive_states, next_state_vector);
            State s(primitive_states);
            s.pack(pool(), b, this->slack());
            succs_list.push_back(b);

            delete t;
        }
        delete succ_trans;

        Successors s(st, succs_list);
        return s;
    }

    //used for partial order reduction, the algorithm marks unusable nodes
    vector<tree_node_t *> tree_nodes;

    /**
     * All the descendants (and only them) of the given automaton
     * will be marked with "in_composition=true"
     *
     * \param i     automaton ID
     */
    void setCandidateFlags(int i);

    /**
     * Called by setCandidateFlags()
     */
    void setInComposition(int i);

    /**
     * Returns true if the given automaton satisfies C1
     */
    bool checkC1(int aid);

    /**
     * For all nodes sets whether they satisfy C0 and C2 or not.
     */
    void setC0C2(const vector<transition_t *> * enabled_trans);

    // the size of slack data allocated by higher layers
    // offset of the generator's slack data (Extension)
    int original_slack;

    /**
     * Used to store the ID of the selected ample set.
     */
    struct Extension {
        int ample_set_id;
    };

    /**
     * Allocates and initializes a new node.
     */
    template< typename Alloc >
    Node newNode( Alloc alloc ) {
        Blob b = makeBlobCleared( alloc, State::metrics.size );
        setAmpleSet(b, -1); //newly allocated node, ample set is unknown
        return b;
    }

    /**
     * Extract the Extension from a node.
     */
    Extension &extension( Node &n ){
        return pool().get< Extension >( n, original_slack );
    }

    /**
     * Returns the stored ample set ID, if available
     */
    int getAmpleSet( Node &n ) {
        return extension(n).ample_set_id;
    }


    /**
     * Stores ample set ID.
     */
    void setAmpleSet( Node &n, int id ) {
        extension(n).ample_set_id = id;
    }


    /**
     * Searches for an ample set, returns its ID, if available.
     */
    int findAmpleSet(vector<transition_t *> * enabled_trans);

    /**
     * The unpacked vector contains empty elements for composite automata;
     * this simplifies the access to states of simple automata, because
     * we can just use the simple automaton's ID as the index.
     */
    void packPrimitiveStates(vector<int> & packed_state, vector<int> & unpacked_state);

    /**
     * Adds empty elements into the vector to the positions
     * of composite automata, so that the state of
     * a simple automaton can be retrieved from the position given by its ID.
     */
    void unpackPrimitiveStates(vector<int> & packed_state, vector<int> & unpacked_state);

#ifdef LCA
    vector<int> system_primitive_components;
    int **lca;
    label_set *** restrict;
    label_set *** only;
    label_set * empty_label_set;

    /**
     * Recursively walk through all children of the given automaton
     * (is supposed to be called with the system automaton's ID).
     */
    void markSystemComponents(int id);

    /**
     * Initializes the vector containing only those primitive automata
     * which are descendants of the system automaton.
     * Also deletes the restriction sets from all but the system automaton
     * and deletes all the automata which are not
     * descendants of the system automaton.
     */
    void initSystemPrimitiveComponents();

    /**
     * Computes the lowest common ancestor of two automata.
     */
    int computeLCA(int a1, int a2);

    /**
     * Initializes the matric of lowest common ancestors.
     */
    void initLCA();

    /**
     * Returns the lowest common ancestor of two automata.
     */
    int getLCA(int u, int v);

    /**
     * Computes the intersection of sets
     * between the lower and the upper automaton.
     */
    label_set * computeOnlySet(int lower, int upper);
    /**
     * Computes the union of the sets
     * between the lower and the upper automaton.
     */
    label_set * computeRestrictSet(int lower, int upper);

    /**
     * Initializes the matrix of precomputed sets.
     */
    void initRestrictSets();

    /**
     * Returns the precomputed "onlyL" set.
     */
    label_set * getOnlySet(int lower, int upper);
    /**
     * Returns the precomputed "restrictL" set.
     */
    label_set * getRestrictSet(int lower, int upper);

    /**
     * Checks whether the given label is allowed
     * between the automaton _from_ and _to_
     * (not including _to_, unless _incl_ is true).
     */
    bool isLabelAllowed(const label_t &l, int from, int to, bool incl);

    /**
     * Computes the collection of successors
     * using the LCA-based algorithm.
     */
    vector<sometrans> getSuccTransitionsLCA();
#endif

};

}  // namespace generator
}  // namespace divine

#endif
