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

#include "buchi.hpp"

namespace divine {
namespace ltl {

size_t uCount = 0;

size_t newNodeId()
{
    static size_t nodeIdCount = 0;
    return nodeIdCount++;
}

size_t newClassId()
{
    static size_t classIdCount = 0;
    return classIdCount++;
}

State::State( Node* node )
{
    assert( node );

    id = node->id;
    next = node->next;
    std::vector< bool > acc ( uCount );
    for( size_t i = 0; i < uCount; ++i )
        acc[i] = !node->untils[i] || node->rightOfUntils[i];
    std::set< LTLPtr, LTLComparator2 > label;
    for( auto l : node->old )
        label.insert( l );
    addEdge( node->incomingList, label, acc );
}

void State::merge( Node* node )
{
    std::vector< bool > acc( uCount );
    for( size_t i = 0; i < uCount; ++i )
        acc[i] = !node->untils[i] || node->rightOfUntils[i];
    std::set< LTLPtr, LTLComparator2 > nodeOld;
    for( auto l : node->old )
        nodeOld.insert( l );
    bool foundTrans = false;
    for( auto& edge : edgesIn )
        if( edge.label == nodeOld  && edge.accepting == acc ) {
            foundTrans = true;
            for( size_t inNode : node->incomingList )
                edge.sources.insert( inNode );
            break; //there will be no other such edge
        }
    if( !foundTrans )
        addEdge( node->incomingList, nodeOld, acc );
}

// inserts f in target unless node->old contains it ( and node is not nullptr )
bool inject( Node* node, std::set< LTLPtr, LTLComparator >& target, LTLPtr f )
{
    if( !node || node->old.count( f ) == 0 )
        return target.insert( f ).second;
    return false;
}

void fillSets( Node* node, LTLPtr form,
        std::set< LTLPtr, LTLComparator >& new1,
        std::set< LTLPtr, LTLComparator >& next1,
        std::set< LTLPtr, LTLComparator >& new2 )
{

    if( form->is< Binary >() ) {
        auto o = form->get< Binary >().op;

        LTLPtr phi = form->get< Binary >().left;
        LTLPtr psi = form->get< Binary >().right;

        switch( o ) {
            case Binary::Until:
                inject( node, new1, phi ); //proceeds IF node is nullptr OR phi is in not in old, then we insert it to new1, we want this for the second usage of this method in split
                next1.insert( form );
                inject( node, new2, psi );
                break;
            case Binary::Or:
                inject( node, new1, phi );
                inject( node, new2, psi );
                break;
            case Binary::And:
                inject( node, new1, LTL::make( false ) ); // note mistake in table 1 in Gianokopoulou/Lerda RIACS 2001
                inject( node, new2, phi );
                inject( node, new2, psi );
                break;
            case Binary::Release:
                inject( node, new1, psi );
                next1.insert( form );
                inject( node, new2, phi );
                inject( node, new2, psi );
                break;
            case Binary::WeakUntil:
            case Binary::Impl:
            case Binary::Equiv:
                assert(false);
        }
    }
    else if( form->is< Unary >() ) {
        auto o = form->get< Unary >().op;
        LTLPtr subExp = form->get< Unary >().subExp;
        switch( o ) {
            case Unary::Neg:
                inject( node, new1, form );
                inject( node, new2, LTL::make( false ) );
                break;
            case Unary::Global:
                inject( node, new1, subExp );
                inject( node, new2, LTL::make( false ) );
                next1.insert( form );
                break;
            case Unary::Future:
                next1.insert( form );
                inject( node, new2, subExp );
                break;
            case Unary::Next:
                next1.insert( subExp );
                inject( node, new2, LTL::make( false ) );
                break;
        }
    }
    else {
        assert( form->isAtomOrBooleanOrNeg() );
        inject( node, new1, form );
        inject( node, new2, LTL::make( false ) );
    }
}

bool ltlEquals( LTLPtr f1, LTLPtr f2 ) {
    LTLComparator c;
    return !c(f1, f2) && !c(f2, f1);
}

StatePtr Node::findTwin( const std::set< StatePtr, State::Comparator >& states )
{
    for ( auto state: states )
        if ( next.size() == state->next.size() && std::equal( next.begin(), next.end(), state->next.begin(), ltlEquals ) )
            return state;
    return nullptr;
}

//returns the new node that has been splitted from this
NodePtr Node::split( LTLPtr form ) {
    resetRightOfUntils();
    NodePtr node2 = std::make_shared< Node >( *this );
    node2->resetUntils();

    std::set< LTLPtr, LTLComparator > new1;
    std::set< LTLPtr, LTLComparator > next1;
    std::set< LTLPtr, LTLComparator > new2;

    fillSets( this, form, new1, next1, new2);

    node2->toBeDone.insert( new2.begin(), new2.end() );
    toBeDone.insert( new1.begin(), new1.end() );
    next.insert( next1.begin(), next1.end() );
    return node2;
}


void printSet( std::ostream &o, const std::set< LTLPtr, LTLComparator >& set )
{
    o << "    { ";
    for( auto ptr = set.begin(); ptr != set.end(); ) {
        o << (*ptr)->string();
        if( ++ptr != set.end() )
            o << ", ";
    }
    o << " }";
}

bool Node::isinSI( LTLPtr phi, const std::set< LTLPtr, LTLComparator >& A, const std::set< LTLPtr, LTLComparator >& B ) {

    if( phi->isAtomOrBooleanOrNeg() ) {
        if( phi->is< Boolean >() && phi->get< Boolean >().value )
            return true;
        return A.count( phi ) != 0;
    }
    if( phi->is< Unary >() && phi->get< Unary >().op == Unary::Neg ) {
        LTLPtr subExp = phi->get< Unary >().subExp;
        if( subExp->isAtomOrBooleanOrNeg() )
             return isinSI( subExp, A, B );
    }

    std::set< LTLPtr, LTLComparator > new1;
    std::set< LTLPtr, LTLComparator > next1;
    std::set< LTLPtr, LTLComparator > new2;

    fillSets( nullptr, phi, new1, next1, new2 );

    if( std::includes( B.begin(), B.end(), next1.begin(), next1.end(), LTLComparator() ) ) {
        bool new1UnderSI = true;
        for( LTLPtr n1 : new1 )
            new1UnderSI = new1UnderSI && isinSI( n1, A, B );
        if( new1UnderSI )
            return true;
    }

    bool new2UnderSI = true;
    for( LTLPtr n2 : new2 )
        new2UnderSI = new2UnderSI && isinSI( n2, A, B );
    return new2UnderSI;
}

//true if neg(phi) in SI(node.old, node.next)
bool Node::contradics( LTLPtr phi ) {
    LTLPtr negation = LTL::make( Unary::Neg, phi )->normalForm();
    bool result = isinSI( negation, old, next );
    return result;
}

bool Node::isRedundant( LTLPtr phi ) {
    if( phi->is< Binary >() && phi->get< Binary >().op == Binary::Until ) // phi is Until formula
        if ( !isinSI( phi->get< Binary >().right, old, next ) ) // additionally, phi.right is not in SI
            return false;
    bool result = isinSI( phi, old, next );
    return result;
}

void Node::print( std::ostream &o ) const
{
    o << "NODE " << id <<  ":  ToBeDone = ";
    printSet( o, toBeDone );

    o << std::endl << "         Old =           ";
    printSet( o, old );
    o << std::endl << "         Next =          ";
    printSet( o, next );
    o << std::endl << "         Untils =        ";
    for( auto b : untils )
        o << b;
    o << std::endl << "         rightOfUntils = ";
    for( auto b : rightOfUntils )
        o << b;
    o << std::endl;
}

size_t Node::depthOfRecursion = 0;

std::set< StatePtr, State::Comparator > Node::expand( std::set< StatePtr, State::Comparator >& states ) {
    if ( toBeDone.empty() )
    {
        StatePtr twin = findTwin( states );
        if ( twin )
        {
            twin->merge( this );
            return states;
        }
        else // there is no twin
        {
            states.insert( std::make_shared< State >( State( this ) ) );

            NodePtr newNode = std::make_shared< Node >();
            newNode->incomingList.insert( id );
            newNode->toBeDone.insert( next.begin(), next.end() );
            return newNode->expand( states );
        }
    }
    else
    {
        auto nfIterator = toBeDone.begin();
        LTLPtr nf = *nfIterator; //next formula
        toBeDone.erase( nfIterator );

        auto indexes = nf->indexesOfUParents();
        for( auto i : indexes )
            rightOfUntils[i] = true;
        if( contradics( nf ) ) // contradicting node gets discarded
            return states;
        if( isRedundant( nf ) ) // formula is redundant so no need to process it
            return expand( states );
        if( nf->isType( Binary::Until ) ) {
            assert( nf->get<Binary>().untilIndex < static_cast<int>( uCount ) );
            untils[nf->get<Binary>().untilIndex] = true;
        }
        // no contradictions && formula is not redundant
        if( !nf->isAtomOrBooleanOrNeg() ) {
            if( nf->is< Binary >() ) {
                if( nf->isType( Binary::Until ) || nf->isType( Binary::Or ) || nf->isType( Binary::Release ) ) {
                    NodePtr node2 = split( nf );
                    auto tmp = expand( states );
                    return node2->expand( tmp );
                }
                if( nf->isType( Binary::And ) ) {
                    if( old.count( nf->get< Binary >().left ) == 0 )
                        toBeDone.insert( nf->get< Binary >().left );
                    if( old.count( nf->get< Binary >().right ) == 0 )
                        toBeDone.insert( nf->get< Binary >().right );
                    return expand( states );
                }
                assert( false && "formula should have been in normal form!");
            }
            else if( nf->isType( Unary::Next ) ) {
                next.insert( nf->get< Unary >().subExp );
                return expand( states );
            }
            assert( false && "formula should have been in normal form!");
            return states;
        } else { //next formula is literal
            if( nf->is< Boolean >() && !nf->get< Boolean >().value ) //nf=False
                return states;
            old.insert( nf );
            return expand( states );
        }
    }
}

TGBA1::TGBA1( const TGBA2& _tgba2 )
        : formula( nullptr )
        , name( _tgba2.name )
        , allLiterals( _tgba2.allLiterals )
        , allTrivialLiterals( _tgba2.allTrivialLiterals )
    {
        for( size_t stateId = 0; stateId < _tgba2.states.size(); ++stateId )
            states.push_back( std::make_shared< State >( stateId ) );
        for( size_t stateId = 0; stateId < _tgba2.states.size(); ++stateId ) {
            assert( stateId < states.size() );
            std::set< size_t > sources;
            sources.insert( stateId );
            std::set< LTLPtr, LTLComparator2 > emptyLabel;
            for( const auto& transition : _tgba2.states.at( stateId ) ) {
                std::vector< bool > acc;
                acc.resize( _tgba2.nAcceptingSets, false );
                for( bool index : transition.accepting )
                    acc.at( index ) = true;
                states.at( transition.target )->addEdge( sources, emptyLabel, acc );
            }
        }
    }

} // ltl namespace
} // divine namespace
