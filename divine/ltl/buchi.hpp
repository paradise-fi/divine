/*
 * (c) 2016 Viktória Vozárová <>
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
#include <vector>
#include <string>
#include <algorithm>
#include <set>

#ifndef LTL2C_BUCHI_H
#define LTL2C_BUCHI_H

namespace divine {
namespace ltl {

int getId()
{
    static int idCount = 0;
    idCount++;
    return idCount;
}

struct Buchi
{
    struct Edge
    {
        int from, to;
        LTLPtr guard;

        Edge( int _from, int _to, LTLPtr _guard ) :
            from( _from ), to( _to ), guard( _guard )  {}
    };

    std::vector< Edge > edges;

    Buchi(std::vector< Edge > _edges ) : edges( _edges ) {}
};
struct Node;
using NodePtr = std::shared_ptr< Node >;

struct Node
{
    struct Comparator
    {
        bool operator()( NodePtr nodeA, NodePtr nodeB ) const
        {
            return nodeA->id == nodeB->id;
        }
    };

    int id; // unique identifier for the node
    std::set< NodePtr, Comparator > incomingList; // the list of predecessor nodes
    std::set< LTLPtr, LTL::Comparator > oldList; // subformulas of Phi already processed for this node
    std::set< LTLPtr, LTL::Comparator > newList; // subformulas of Phi yet to be processed
    std::set< LTLPtr, LTL::Comparator > nextList; // subformulas of Phi satifsying following nodes

    Node( ) : id( getId() ) {}
    Node( int _id ) : id( _id ) {}
};

NodePtr findTwin( NodePtr nodeP, std::set< NodePtr > list )
{
    for (auto nodeR: list)
        if ( ( nodeP->oldList == nodeR->oldList ) && ( nodeP->nextList == nodeR->nextList ) )
            return nodeP;
    return nullptr;
}

/*
 *   Process formula
 */

void copy( NodePtr to, NodePtr from )
{
    to->incomingList.insert( from->incomingList.begin(), from->incomingList.end() );
    to->oldList.insert( from->oldList.begin(), from->oldList.end() );
    to->newList.insert( from->newList.begin(), from->newList.end() );
    to->nextList.insert( from->nextList.begin(), from->nextList.end() );
}

int process( Atom phi, NodePtr node, std::set< NodePtr > result )
{
    LTLPtr negPhi = phi.normalForm( true );
    if ( ( phi.string() == "false" ) || ( node->oldList.find( negPhi ) != node->oldList.end() ) )
    {
        return 0;
    }
    else
    {
        NodePtr nodeR = std::make_shared< Node >();
        copy( nodeR, node );
        result.insert( nodeR );
        return 1;
    }
}

int process( Unary, NodePtr, std::set< NodePtr > )
{
    /*
    switch ( phi.op )
    {
        case Unary::Neg:
            return 0;
        case Unary::Next:

    }*/
    return 0;
}

int process( Binary, NodePtr, std::set< NodePtr > )
{
    return 0;
}

int process( LTLPtr phi, NodePtr node, std::set< NodePtr > result )
{
    return phi->apply( [&] ( auto phiP ) -> int { return process( phiP, node, result ); } ).value();
}


/*
 *   Expand node
 */

void expand( NodePtr nodeQ, std::set< NodePtr > list )
{
    if ( nodeQ->newList.empty() )
    {
        NodePtr nodeR = findTwin( nodeQ, list );
        if ( nodeR )
        {
            nodeR->incomingList.insert( nodeQ->incomingList.begin(), nodeQ->incomingList.end() );
            return;
        }
        else
        {
            list.insert( nodeQ );
            NodePtr nodeP = std::make_shared< Node >();
            nodeP->incomingList.insert( nodeQ );
            nodeP->newList.insert( nodeQ->nextList.begin(), nodeQ->nextList.end() );
            expand( nodeP, list );
            return;
        }
    }
    else
    {
        LTLPtr phi = *nodeQ->newList.begin();
        nodeQ->newList.erase( phi );
        if ( nodeQ->oldList.find( phi ) != nodeQ->oldList.end() )
        {
            expand( nodeQ, list );
            return;
        }
        else
        {
            LTLPtr negPhi = phi->normalForm( true );

            std::set< NodePtr > result;
            process( phi, nodeQ, result );

            if ( ( phi->string() == "false" ) || ( nodeQ->oldList.find( negPhi ) != nodeQ->oldList.end() ) )
            {
                return;
            }
            else
            {
                NodePtr nodeR = std::make_shared< Node >();
                nodeR->incomingList.insert( nodeQ->incomingList.begin(), nodeQ->incomingList.end() );
                nodeR->oldList.insert( nodeQ->oldList.begin(), nodeQ->oldList.end() );
                nodeR->newList.insert( nodeQ->newList.begin(), nodeQ->newList.end() );
                nodeR->nextList.insert( nodeQ->nextList.begin(), nodeQ->nextList.end() );
                nodeR->oldList.insert( phi );
            }
        }
    }
}

std::shared_ptr< Buchi > ltlToBuchi( LTLPtr _formula )
{
    LTLPtr formula = _formula->normalForm();

    NodePtr init = std::make_shared< Node >();

    std::set< NodePtr, Node::Comparator  > nodes;

    NodePtr node = std::make_shared< Node >();
    node->incomingList.insert( init );
    node->newList.insert( formula );

    return nullptr;
}

}
}

#endif //LTL2C_BUCHI_H
