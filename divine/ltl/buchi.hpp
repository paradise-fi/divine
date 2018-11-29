/*
 * (c) 2017 Tadeáš Kučera <kucerat@mail.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ltl.hpp"
#include <string>
#include <algorithm>
#include <map>
#include <cassert>
#include <stack>
#include <fstream>

#ifndef LTL2C_BUCHI_H
#define LTL2C_BUCHI_H



namespace divine {
namespace ltl {


extern size_t uCount; // number of Until subformulae  ==  number of possible classes of states

size_t newNodeId();
size_t newClassId();


struct Node;
struct State;
struct TGBA;
struct TGBA2;
using NodePtr = std::shared_ptr< Node >;
using StatePtr = std::shared_ptr< State >;


struct State
{
    struct Comparator
    {
        bool operator()( StatePtr s1, StatePtr s2 ) const
        {
            return s1->id < s2->id;
        }
    };
    struct Edge
    {
        std::set< size_t > sources; // ids of states
        std::set< LTLPtr, LTLComparator2 > label; // set of literals that must hold if choosing this edge
        std::vector< bool > accepting; // says to which accepting sets this edge belongs
        Edge( const std::set< size_t >& _sources, const std::set< LTLPtr, LTLComparator2 >& _label, const std::vector< bool >& _accepting )
            : sources( _sources )
            , label( _label )
            , accepting( _accepting )
        {
        }
        friend std::ostream& operator<<( std::ostream & os, const Edge& e ) {
            os << "(";
            for( const auto & source : e.sources )
                os << source << ", ";
            os << " --- ";
            for( auto ltlIt = e.label.begin(); ltlIt != e.label.end(); ) {
                os << (*ltlIt)->string();
                if( ++ltlIt != e.label.end() )
                    os << " & ";
            }
            os << " --> . Accepting = ";
            for( auto b : e.accepting ) {
                os << b;
            }
            os << ")";
            return os;
        }

        std::string accSets() const {
            std::stringstream output;
            output << " {";
            bool flag = false;
            for( size_t i = 0; i != accepting.size(); ++i )
                if( accepting[i] ) {
                    if( flag )
                        output << " ";
                    flag = true;
                    output << i;
                }
            if( !flag )
                return std::string();
            output << "}";
            return output.str();
        }
    };

    size_t id;
    std::vector< Edge > edgesIn;
    std::set< LTLPtr, LTLComparator > next; //TODO use second comparator?
    std::set< LTLPtr, LTLComparator > old;

    State() = default;
    State( size_t _id )
	: id( _id )
    {
    }
    State( const State& other )
        : id( other.id )
        , edgesIn( other.edgesIn )
        , next( other.next )
        , old( other.old )
    {
    }
    State( Node* node );

    void addEdge( const std::set< size_t >& _sources, const std::set< LTLPtr, LTLComparator2 >& _label, const std::vector< bool >& _accepting )
    {
        edgesIn.emplace_back( _sources, _label, _accepting );
    }
    void merge( Node* node );

    friend std::ostream& operator<<( std::ostream & os, const State& s ) {
        os << "  State " << s.id << std::endl;
        for( const auto & e : s.edgesIn )
            os << e << ", ";
        os << std::endl;
        return os;
    }
};


struct Node
{
    struct Comparator
    {
        bool operator()( NodePtr nodeA, NodePtr nodeB ) const
        {
            return nodeA->id == nodeB->id;
        }
    };

    size_t id; // unique identifier for the node
    std::set< size_t > incomingList; // the list of predecessor nodes
    std::set< LTLPtr, LTLComparator > old; // LITERAL subformulas of Phi already processed - must hold in corresponding state
    std::set< LTLPtr, LTLComparator > toBeDone; // subformulas of Phi yet to be processed
    std::set< LTLPtr, LTLComparator > next; // subformulas of Phi that must hold in immediate successors of states that satisfy all properties in Old
    std::vector< bool > untils;
    std::vector< bool > rightOfUntils;

    static size_t depthOfRecursion;
    size_t localDepthOfRecursion = 1;

    void print() const;

    Node()
        : id( newNodeId() )
    {
        untils.resize( uCount );
        rightOfUntils.resize( uCount );
        resetUntils();
        resetRightOfUntils();
    }

    Node( const Node& o )
        : id( newNodeId() )
        , incomingList( o.incomingList )
        , old( o.old )
        , toBeDone( o.toBeDone )
        , next( o.next )
        , untils( o.untils )
        , rightOfUntils( o.rightOfUntils )
    {
    }

    void resetUntils( )
    {
        for( bool b : untils )
            b = false;
    }
    void resetRightOfUntils( )
    {
        for( bool b : rightOfUntils )
            b = false;
    }
    /**
      * Finds (first) "node with same old and next" in specified set of nodes
      * @return node with same old and next / nullptr iff not present
    **/
    StatePtr findTwin( const std::set< StatePtr, State::Comparator >& states );

    NodePtr split( LTLPtr form );

    bool isinSI( LTLPtr phi, const std::set< LTLPtr, LTLComparator >& A, const std::set< LTLPtr, LTLComparator >& B );

    bool contradics( LTLPtr phi ); //true if neg(phi) in SI(node.old, node.next)

    bool isRedundant( LTLPtr phi );

    std::set< StatePtr, State::Comparator > expand( std::set< StatePtr, State::Comparator >& states );
};

struct TGBA1 {
    LTLPtr formula;
    std::string name;
    std::vector< StatePtr > states;
    std::vector< std::vector< State::Edge > > acceptingSets;
    std::vector< LTLPtr > allLiterals;
    std::vector< LTLPtr > allTrivialLiterals; //those which do not have negation

    TGBA1() = default;
    TGBA1( LTLPtr _formula, std::set< StatePtr, State::Comparator >& _states )
        : formula( _formula )
        , name( _formula->string() )
    {
        std::copy( _states.begin(), _states.end(), std::back_inserter( states ) );
        std::set< LTLPtr, LTLComparator2 > allLiteralsSet;
        acceptingSets.resize( uCount );

        std::vector< size_t > ids;
        for( auto state : states )
            ids.push_back( state->id );
        std::sort( ids.begin(), ids.end() );
        std::map< size_t, size_t > idsMap;
        for( size_t i = 0; i < ids.size(); ++i )
            idsMap.emplace( std::make_pair( ids[i], i ) );

        for( auto state : states )
        {
            state->id = idsMap.at( state->id );
            for( State::Edge & edge : state->edgesIn )
            {
                std::set< size_t > newSources;
                for( size_t s : edge.sources )
                    newSources.insert( idsMap.at( s ) );
                edge.sources = newSources;
                for( size_t i = 0; i < uCount; ++i )
                    if( edge.accepting[i] )
                        acceptingSets[i].push_back( edge );
                for( auto l : edge.label ) {
                    if( !l->is< Boolean >() ) {
                        allLiteralsSet.insert( l );
                        if( l->isType( Unary::Neg ) )
                            allLiteralsSet.insert( l->get< Unary >().subExp );
                    }
                }
            }
        }
        for( auto l : allLiteralsSet ) {
            allLiterals.emplace_back( l );
            if( !l->isType( Unary::Neg ) )
                allTrivialLiterals.emplace_back( l );
        }
        for( size_t i = 0; i < states.size(); ++i )
            assert( i == states.at( i )->id );
        assert( allLiterals.size() == allLiteralsSet.size() );
    }
    TGBA1( const TGBA2& _tgba2 );

    void print() const
    {
        std::cout << "TGBA made from formula " << name << ":" << std::endl;
        for( auto state : states )
            std::cout << *state;
    }

    std::string indexOfLiteral( LTLPtr literal ) const {
        std::stringstream output;
        auto wanted = literal->string();
        for( size_t i = 0; i < allTrivialLiterals.size(); ++i ) {
            if( allTrivialLiterals[i]->string() == wanted ) {
                output << i;
                return output.str();
            }
            if( "!" + allTrivialLiterals[i]->string() == wanted ) {
                output << "!" << i;
                return output.str();
            }
        }
        std::cerr << std::endl<< std::endl << "Literal " << wanted << " not found in " << std::endl;
        for( auto l : allTrivialLiterals )
            std::cerr << l->string() << ", ";
        std::cerr << std::endl;
        assert( false && "unused literal in TGBA" );
        return "error";
    }

    friend std::ostream& operator<<( std::ostream & os, const TGBA1& tgba ) {
        os << "HOA: v1" << std::endl;
        os << "name: \"" << tgba.name << "\"" << std::endl;
        os << "States: " << tgba.states.size() << std::endl;
        os << "Start: 0" << std::endl;
        os << "AP: " << tgba.allTrivialLiterals.size();
        for( auto literal : tgba.allTrivialLiterals )
            os << " \"" << literal->string() << "\"";
        if( uCount == 0 )
            os << std::endl << "acc-name: all" << std::endl;
        else if( uCount == 1 )
            os << std::endl << "acc-name: Buchi" << std::endl;
        else
            os << std::endl << "acc-name: generalized-Buchi " << uCount << std::endl;
        os << "Acceptance: " << uCount;
        if( uCount == 0 )
            os << " t";
        for( size_t i = 0; i < uCount; ++i) {
            os << " Inf(" << i << ")";
            if ( i + 1 != uCount )
                os << " &";
        }
        os << std::endl;
        os << "properties: trans-labels trans-acc " << std::endl;
        os << "--BODY--" << std::endl;
        for( auto state : tgba.states ) {
            os << "State: " << state->id << std::endl;
            for( auto s : tgba.states ) {
                for( const auto & e : s->edgesIn ) {
                    if( e.sources.count( state->id ) != 0 ) { //this transition goes from state state
                        os << "[";
                        bool flag = false;
                        for( auto l : e.label ) {
                            if( flag )
                                os << "&";
                            os << tgba.indexOfLiteral( l );
                            flag = true;
                        }
                        if( !flag )
                            os << "t";
                        os << "] " << s->id;
                        os << e.accSets() << std::endl;
                    }
                }
            }
        }
        os << "--END--" << std::endl;
        return os;
    }
};

struct Transition{
    size_t target;
    std::set< std::pair< bool, size_t > > label; // of indices of literals that must hold if choosing this transition
    std::set< size_t > accepting; // says to which accepting sets this transition belongs to
    Transition( size_t _target, std::set< std::pair< bool, size_t > > _label, std::set< size_t > _accepting )
        : target( _target )
        , label( _label )
        , accepting( _accepting )
    {
    }
    friend std::ostream& operator<<( std::ostream & os, const Transition& t )
    {
        os << "[";
        bool flag = false;
        for( auto l : t.label ) {
            if( flag )
                os << "&";
            if( !l.first )
                os << "!";
            os << l.second;
            flag = true;
        }
        if( !flag )
            os << "t";
        os << "] " << t.target;
        if( !t.accepting.empty() ) {
            os << " {";
            bool flag = false;
            for( size_t acc : t.accepting ) {
                if( flag )
                    os << " ";
                flag = true;
                os << acc;
            }
            os << "}";
        }
        return os;
    }
};

static inline std::pair< bool, size_t > indexOfLiteral( LTLPtr literal, const std::vector< LTLPtr >& literals ) {
    auto wanted = literal->string();
    for( size_t i = 0; i < literals.size(); ++i ) {
        if( literals.at( i )->string() == wanted )
            return std::make_pair( true, i );
        if( "!" + literals.at( i )->string() == wanted )
            return std::make_pair( false, i );
    }
    std::cerr << std::endl<< std::endl << "Literal " << wanted << " not found in " << std::endl;
    assert( false && "unused literal in TGBA" );
    return std::make_pair( true, 0 );
}


/*
 * While TGBA1 is designed such a way that in each node it stores all the transitions
 * that are incoming in the node (which is needed for the translation process), the
 * TGBA2 structure stores for each node all the transitions that go out of it. This
 * property is needed for the product construction as well as for the computation of
 * the accepting strongly connected components: Both representations TGBA1 and
 * TGBA2 of the same automata A together give us access to A and A^T both efficiently.
*/
struct TGBA2 {
    TGBA1 tgba1;
    std::string name;
    LTLPtr formula;
    size_t nStates;
    size_t start = 0;
    size_t nAcceptingSets;
    std::vector< LTLPtr > allLiterals;
    std::vector< LTLPtr > allTrivialLiterals; //those which do not have negation
    std::vector< std::vector< Transition > > states;
    std::vector< std::optional< size_t > > accSCC; //each state is assigned id of optional acc. SCC

    TGBA2() = default;
    TGBA2( TGBA1&& _tgba )
        : tgba1( _tgba )
        , name( tgba1.formula->string() )
        , formula( tgba1.formula )
        , nStates( tgba1.states.size() )
        , nAcceptingSets( tgba1.acceptingSets.size() )
        , allLiterals( tgba1.allLiterals )
        , allTrivialLiterals( tgba1.allTrivialLiterals )
    {
        states.resize( tgba1.states.size() );
        for( auto state : tgba1.states ) {
            assert( state->id < tgba1.states.size() );
            for( const auto& edge : state->edgesIn ) {
                std::set< size_t > acc;
                for( size_t i = 0; i < edge.accepting.size(); ++i )
                    if( edge.accepting.at( i ) )
                        acc.insert( i );
                std::set< std::pair< bool, size_t > > label;
                for( auto literal : edge.label )
                    label.insert( indexOfLiteral( literal, allTrivialLiterals ) );
                for( size_t s : edge.sources )
                    states.at( s ).emplace_back( state->id, label, acc );
            }
        }
        computeAccSCC();
    }

    friend std::ostream& operator<<( std::ostream & os, const TGBA2& tgba2 )
    {
        os << "HOA: v1" << std::endl;
        os << "name: \"" << tgba2.name << "\"" << std::endl;
        os << "States: " << tgba2.nStates << std::endl;
        os << "Start: 0" << std::endl;
        os << "AP: " << tgba2.allTrivialLiterals.size();
        for( auto literal : tgba2.allTrivialLiterals )
            os << " \"" << literal->string() << "\"";
        if( uCount == 0 )
            os << std::endl << "acc-name: all" << std::endl;
        else if( uCount == 1 )
            os << std::endl << "acc-name: Buchi" << std::endl;
        else
            os << std::endl << "acc-name: generalized-Buchi " << uCount << std::endl;
        os << "Acceptance: " << tgba2.nAcceptingSets;
        if( uCount == 0 )
            os << " t";
        for( size_t i = 0; i < uCount; ++i) {
            os << " Inf(" << i << ")";
            if ( i + 1 != uCount )
                os << " &";
        }
        os << std::endl;
        os << "properties: trans-labels trans-acc " << std::endl;
        os << "--BODY--" << std::endl;
        for( size_t stateId = 0; stateId < tgba2.states.size(); ++stateId ) {
            os << "State: " << stateId << std::endl;
            for( const auto& trans : tgba2.states.at( stateId ) )
                os << trans << std::endl;
        }
        os << "--END--" << std::endl;
        return os;
    }

    void DFSUtil1( StatePtr state, std::vector< size_t >& visited, std::vector< size_t >& leaved, std::stack< size_t >& leavedStack, size_t& counter )
    {
        visited.at( state->id ) = ++counter;
        for( const auto& edge : state->edgesIn )
            for( size_t child : edge.sources )
            {
                if( visited.at(child) == 0 )
                    DFSUtil1( tgba1.states.at( child ), visited, leaved, leavedStack, counter );
            }
        leaved.at( state->id ) = ++counter;
        leavedStack.push( state->id );
    }
    //returns vector of all states in accepting SCC containing state state
    void DFSUtil2( size_t state, std::set< size_t >& comp, std::vector< size_t >& visited, std::vector< size_t >& leaved, size_t& counter ) {
        visited[state] = ++counter;
        for( const auto& trans : states.at( state ) )
            if( visited.at( trans.target ) == 0 )
                DFSUtil2( trans.target, comp, visited, leaved, counter );
        comp.insert( state );
        leaved.at( state ) = ++counter;
    }
    bool isAccepting( const std::set< size_t >& component ) {
        if( component.empty() )
            return false;
        std::set< size_t > presentColors;
        for( size_t s : component ) {
            for( const Transition& trans : states.at( s ) ) {
                if( component.count( trans.target ) != 0 )
                    presentColors.insert( trans.accepting.begin(), trans.accepting.end() );
            }
        }
        return presentColors.size() == nAcceptingSets;
    }
    //returns number of accepting components in the TGBA
    size_t computeAccSCC() {
        accSCC.resize( states.size() );
        // first run dfs in G^T (tgba1) to determine the order of states
        std::vector< size_t > visited( states.size(), 0 );
        std::vector< size_t > leaved( states.size(), 0 );
        std::stack< size_t > leavedStack;

        size_t counter = 0;
        for( StatePtr state : tgba1.states )
        {
            if( visited[state->id] == 0 ) //state was not visited yet -> start from it
                DFSUtil1( state, visited, leaved, leavedStack, counter );
        }
        // second round
        leaved = std::vector< size_t >( states.size(), 0 );
        visited = std::vector< size_t >( states.size(), 0 );
        counter = 0;
        size_t componentCounter = 0;

        while( !leavedStack.empty() )
        {
            size_t state = leavedStack.top();
            leavedStack.pop();
            if( visited[state] == 0 ) //state was not visited yet -> start from it
            {
                std::set< size_t > component;
                DFSUtil2( state, component, visited, leaved, counter );
                if( isAccepting( component ) ) {
                    ++componentCounter;
                    //std::cout << "Accepting component:" ;
                    for( size_t s : component ) {
                        //std::cout << ", " << s;
                        accSCC.at( s ) = componentCounter;
                    }
                }
            }
        }
        return componentCounter;
    }
};

static inline TGBA2 HOAParser( const std::string& filename )
{
    //std::cout << "HOAParser called on " << filename << std::endl;
    std::ifstream source( filename );
    TGBA2 tgba2;
    std::string line;
    std::getline( source, line );
    if( line.substr( 0, 4 ) != "HOA:" )
        throw std::invalid_argument("invalid HOA format: 'HOA:' should always be the first token of the file.");
    while( std::getline( source, line ) )
    {
        if( line.substr( 0, 8 ) == "--BODY--" )
            break;
        std::stringstream ssLine( line );
        std::string feature;
        std::string data;
        getline( ssLine, feature, ':' );
        if( feature == "name" || feature == "Name" )
        {
            ssLine.ignore(std::numeric_limits<std::streamsize>::max(), '"');
            std::getline( ssLine, tgba2.name, '"' );
            ssLine.ignore(std::numeric_limits<std::streamsize>::max(), ssLine.widen( '\n' ) );
        }
        else if( feature == "states" || feature == "States" )
        {
            std::getline( ssLine, data );
            std::stringstream ssData( data );
            ssData >> tgba2.nStates;
            tgba2.states.resize( tgba2.nStates );
            /*for( size_t i = 0; i < tgba2.nStates; ++i )
            {
                tgba2.states.emplace_back(  )
            }*/
        }
        else if( feature == "start" || feature == "Start" )
        {
            std::getline( ssLine, data );
            std::stringstream ssData( data );
            ssData >> tgba2.start;
        }
        else if( feature == "AP" )
        {
            ssLine.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            std::getline( ssLine, data, ' ' );
            //std::cout << "data " << "'" << data << "'" << std::endl;
            size_t apCount;
            apCount = std::stoll( data );
            while( std::getline( ssLine, data, ' ' ) )
            {
                std::string atom = data.substr(1, data.length() - 2);
                //std::cout << "'" << atom << "'" << std::endl;
                tgba2.allTrivialLiterals.push_back( LTL::make( atom ) );
            }
            //std::cout << apCount << " vs " << tgba2.allTrivialLiterals.size() << std::endl;
            if( apCount != tgba2.allTrivialLiterals.size() )
                throw std::invalid_argument( "Invalid HOA format: unexpected number of atomic propositions." );

        }
        else if( feature == "acceptance" || feature == "Acceptance" )
        {
            ssLine.ignore( std::numeric_limits<std::streamsize>::max(), ' ' );
            std::getline( ssLine, data, ' ' );
            tgba2.nAcceptingSets = std::stoll( data );
            uCount = tgba2.nAcceptingSets;
        }
        else if( feature != "acc-name" && feature != "Acc-name" && feature != "properties" && feature != "Properties" )
            throw std::invalid_argument( "invalid HOA format: unrecognized option '" + feature );
    }
    size_t stateId;
    while( std::getline( source, line ) )
    {
        if( line.substr( 0, 7 ) == "--END--" )
            break;
        if( line.substr( 0, 6 ) == "State:" ) //finished actual state and starting new one
        {
            stateId = std::stoll( line.substr( 6 ) );
            //std::cout << "'State' token with id " << stateId << std::endl;
            continue;
        }
        else
        {
            //std::cout << "'[' token" << std::endl;
            std::stringstream ssLine( line );
            std::string label, tmpLetter;

            std::getline( ssLine, label, ']' );
            assert( label.at( 0 ) == '[' );

            std::stringstream ssLabel( label );
            ssLabel.ignore( 1, '[' );

            size_t target;
            std::set< std::pair< bool, size_t > > labels;
            std::set< size_t > accepting;

            //PARSING labels
            std::set< std::pair< bool, size_t > > parsedLabels;
            while( std::getline( ssLabel, tmpLetter, '&') )
            {
                std::string tmpSubLetter;
                std::pair< bool, size_t > parsedLabel;
                //std::cout << "  Letter '" << tmpLetter << "'" << std::endl;
                std::stringstream ssLetter( tmpLetter );
                while( tmpSubLetter.empty() && std::getline( ssLetter, tmpSubLetter, ' ' ) )
                {}
                //std::cout << "  tmpSubLetter '" << tmpSubLetter << "'" << std::endl;
                if( !tmpSubLetter.empty() )
                {
                    parsedLabel.first = true;
                    size_t i = 0;
                    for( ; i < tmpSubLetter.size(); ++i ) {
                        if( tmpSubLetter.at( i ) == '!' )
                            parsedLabel.first = !parsedLabel.first;
                        else if( tmpSubLetter.at( i ) != ' ' )
                            break;
                    }
                    if( i < tmpSubLetter.size() && tmpSubLetter.at( i ) == 't' )
                        continue;
                    parsedLabel.second = std::stoll( tmpSubLetter.substr( i ) );
                }
                //std::cout << "  parsedLabel: " << parsedLabel.first << ", " << parsedLabel.second << std::endl;
                parsedLabels.insert( parsedLabel );
            }
            //PARSING target
            std::string trg;
            while( trg.empty() && std::getline( ssLine, trg, ' ' ) )
            {}
            target = std::stoll( trg );

            //PARSING acceptance
            ssLine.ignore( 1, '{' );
            std::string acceptance, tmpAcceptance;
            std::getline( ssLine, acceptance, '}' );
            std::stringstream ssAcceptance( acceptance );
            while( std::getline( ssAcceptance, tmpAcceptance, ' ' ) )
            {
                accepting.insert( std::stoll( tmpAcceptance ) );
            }
            Transition transition( target, parsedLabels, accepting);
            //std::cout << "Transition: " << transition << std::endl << std::endl;
            tgba2.states.at( stateId ).push_back( transition );
        }
    }
    //TODO: pro kazdy State: a hrany na radcich pod nim vytvorit vektor instanci tridy Transition a tento vektor pridat do vektoru states. Dale bude nutne naplnit tgba1, accSCC, allLiterals bez uziti formula. Promyslet poradi!

    std::cout << tgba2 << std::endl;
    //TODO use TGBA1 constructor with parameter this (for the embedded TGBA1 in this)
    //TODO implement the constructor
    return tgba2;
}



static inline TGBA1 ltlToTGBA1( LTLPtr _formula, bool negate )
{
    LTLPtr formula = _formula->normalForm( negate );
    uCount = formula->countAndLabelU();
    formula->computeUParents();

    NodePtr init = std::make_shared< Node >();

    init->next.insert( formula );
    std::set< StatePtr, State::Comparator > states;
    states = init->expand( states ); //Expanding the initial node

    TGBA1 tbga( formula, states );
    return tbga;
}

struct LTL2TGBA /* nodes, states, buchi */
{
    TEST(node_construction)
    {
        Node node1;
        Node node2;
        assert( node1.id != node2.id );
        Node node3 = Node( node2 );
        assert( node2.id != node3.id );

        std::string x("X a");
        LTLPtr next = LTL::parse( x );
        node1.toBeDone.insert( next );
        node2 = node1;
        node3 = Node( node1 );
        assert( node1.next == node2.next );
        assert( node1.next == node3.next );
    }
    TEST(state_construction) /*a && Xb*/
    {
        std::string a("a");
        LTLPtr atoma = LTL::parse( a );
        std::string b("b");
        LTLPtr atomb = LTL::parse( b );
        Node node;
        node.old.insert( atoma );
        node.next.insert( atomb );
        State state( &node );
        auto it1 = state.next.find( atomb );
        assert( it1 != state.next.end() );
        assert( !state.edgesIn.empty() );
        auto it2 = state.edgesIn.at(0).label.find( atoma );
        assert( it2 != state.edgesIn.at(0).label.end() );
    }
    TEST(node_findTwin)
    {
        std::string a("a");
        LTLPtr atoma = LTL::parse( a );
        std::string b("b");
        LTLPtr atomb = LTL::parse( b );
        Node node;
        node.old.insert( atoma );
        node.next.insert( atomb );

        StatePtr state = std::make_shared<State>( &node );

        std::set< StatePtr, State::Comparator > states;
        states.insert( state );
        assert( node.findTwin( states ) );
    }
    TEST(node_contradics)
    {
        LTLPtr a = LTL::parse( "a" );
        LTLPtr b = LTL::parse( "b" );
        LTLPtr avb = LTL::parse( "a || b" );
        LTLPtr bva = LTL::parse( "b || a" );
        LTLPtr ab = LTL::parse( "a && b" );
        LTLPtr aUb = LTL::parse( "a U b" );
        LTLPtr tUa = LTL::parse( "true U a" );
        Node node;
        node.old.insert( a );
        node.next.insert( b );
        assert( node.isinSI( a, node.old, node.next ) );
        assert( node.isinSI( avb, node.old, node.next ) );
        assert( node.isinSI( bva, node.old, node.next ) );
        assert( node.isinSI( tUa, node.old, node.next ) );
        assert( !node.isinSI( ab, node.old, node.next ) );
        assert( !node.isinSI( b, node.old, node.next ) );
        assert( !node.isinSI( aUb, node.old, node.next ) );

        assert( node.contradics( LTL::parse( "!a" ) ) );
        assert( node.contradics( LTL::parse( "! ( a || b )" ) ) );
        assert( node.contradics( LTL::make( Unary::Neg, bva ) ) );
        assert( node.contradics( LTL::make( Unary::Neg, tUa ) ) );
        assert( !node.contradics( LTL::make( Unary::Neg, ab ) ) );
        assert( !node.contradics( LTL::make( Unary::Neg, b ) ) );
        assert( !node.contradics( LTL::make( Unary::Neg, aUb ) ) );
    }
    TEST(node_isRedundant)
    {
        LTLPtr a = LTL::parse( "a" );
        LTLPtr b = LTL::parse( "b" );
        LTLPtr avb = LTL::parse( "a || b" );
        LTLPtr bva = LTL::parse( "b || a" );
        LTLPtr ab = LTL::parse( "a && b" );
        LTLPtr aUb = LTL::parse( "a U b" );
        LTLPtr tUa = LTL::parse( "true U a" );
        Node node1, node2;
        node1.old.insert( a );
        node1.next.insert( b );
        assert( node1.isRedundant( a ) );
        assert( node1.isRedundant( avb ) );
        assert( node1.isRedundant( bva ) );
        assert( node1.isRedundant( tUa ) );
        assert( node1.isRedundant( tUa ) );

        node2.old.insert( a );
        node2.next.insert( aUb );
        assert( node2.isinSI( a, node2.old, node2.next ) );
        assert( node2.isinSI( avb, node2.old, node2.next ) );
        assert( node2.isinSI( bva, node2.old, node2.next ) );
        assert( node2.isinSI( aUb, node2.old, node2.next ) );
        assert( !node2.isinSI( b, node2.old, node2.next ) );

        assert( node2.isRedundant( a ) );
        assert( node2.isRedundant( avb ) );
        assert( node2.isRedundant( bva ) );
        assert( !node2.isRedundant( aUb ) );
    }
    TEST(ltlToTGBA1_simple)
    {
        std::string u( " a U b " );
        std::string r("a R b");
        std::string x("X a");
        LTLPtr until = LTL::parse( u );
        LTLPtr release = LTL::parse( r );
        LTLPtr next = LTL::parse( x );
        auto tgba1 = ltlToTGBA1( next, false );

    }

};

}
}

#endif //LTL2C_BUCHI_H
