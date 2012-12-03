/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
 *
 * This file is part of the UPPAAL DBM library.
 *
 * The UPPAAL DBM library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * The UPPAAL DBM library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the UPPAAL DBM library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */

#include <iostream>
#include "debug/macros.h"
#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "infimum.h"
#include <stdexcept>
#include <stdlib.h>
#include <cstdlib>

/*
 * constraintValue(i,j)
 * pre: dbm with dim has to be defined and 0 <= i,j < dim
 * 
 * Returns c where x_i - x_j <= c in the dbm;
 */
#define constraintValue(i,j) (dbm_raw2bound(dbm[(i) * dim + (j)]))
#define b(i) (-rates[i]) // b is the notation for supply/demand in the network flow literature

/*
 * Shortcuts to the Node structure 
 */
#define index(n) ((n) - nodes)
#define pred(i) index(nodes[i].pred)
#define depth(i) nodes[i].depth
#define thread(i) index(nodes[i].thread)
#define inbound(i) nodes[i].inbound
#define flow(i) nodes[i].flow
#define potential(i) nodes[i].potential

/**
 * Tree structure:
 * Storing the information we need about nodes in the graph
 * spanning tree.
 */
struct Node
{
    Node *pred;      /**< Predecessor in the tree. */
    uint32_t depth;    /**< Depth from root (Always node0). */
    Node *thread;      /**< Next according to the preorder. */
    bool inbound;      /**< Is the arc pointing to me or away. */
    int32_t flow;      /**< Flow for the current solution. */
    int32_t potential; /**< Node potential for spanning tree solution. */
};

/*
 * Used to store the indices of the clock constraints
 * from the minimal constraint form.
 */
struct arc_t
{
    uint32_t i;
    uint32_t j;
};

#ifndef NDEBUG
static void printSimpleNodeInfo(Node *nodes, uint32_t i)
{
    std::cerr << "Index: " << i << ", pred: " << pred(i) << ", depth: " << depth(i) << ", thread: " << thread(i) << ",  inbound: " << inbound(i) << ", flow: " << flow(i) << ", potential: " << potential(i);
}

static void printNodeInfo(Node *nodes, const int32_t *rates, uint32_t i)
{
    printSimpleNodeInfo(nodes, i);
    std::cerr << ", s/d: " << b(i) << " ";
    std::cerr << std::endl;
}

static void printAllNodeInfo(Node *nodes, const int32_t *rates, uint32_t dim)
{
    std::cerr << "Nodes:" << std::endl;
    for (uint32_t i = 0; i < dim; i++)
    {
        printNodeInfo(nodes, rates, i);
    }
}

static void printSimpleAllNodeInfo(Node *nodes, uint32_t dim)
{
    std::cerr << "Nodes:" << std::endl;
    for (uint32_t i = 0; i < dim; i++)
    {
        printSimpleNodeInfo(nodes, i);
        std::cerr << std::endl;
    }
}

static bool checkTreeIntegrity(const raw_t *dbm, const int32_t *rates, Node *nodes, uint32_t dim)
{
    bool treeOK = true;
    //Check that all nodes get their flow
    int32_t sum[dim], total = 0;
    uint32_t i;
    sum[0] = 0;

    for (i = 1; i < dim; i++)
    {
        sum[0] -= b(i);
        total -= b(i);
        sum[i] = b(i);
    }
    for (i = 1; i < dim; i++)
    {
        if (inbound(i))
        {
            sum[i] += flow(i);
            sum[pred(i)] -= flow(i);
        }
        else
        {
            sum[pred(i)] += flow(i);
            sum[i] -= flow(i);
        }
    }
    for (i = 0; i < dim; i++)
    {
        if (sum[i] != 0)
        {
            treeOK = false;
            std::cerr << "sum[" << i << "] is corrupted: " << sum[i] << ", should be: " << (i == 0? total: b(i)) <<std::endl;
        }
    }
        
    //Check that the reduced costs are zero
    int32_t reduced_cost;
    for (i = 1; i < dim; i++)
    {
        if (potential(i) == dbm_INFINITY && pred(i) == 0)
        {

        }
        else
        {
            if (inbound(i))
                reduced_cost = constraintValue(pred(i),i) - potential(pred(i)) + potential(i);
            else
                reduced_cost = constraintValue(i,pred(i)) + potential(pred(i)) - potential(i);
            if (reduced_cost != 0)
            {
                treeOK = false;
                std::cerr << "Reduced cost not zero (" << reduced_cost << ") i (pot): " << i << " (" << potential(i) << "), pred(i) (pot): " << pred(i) << " (" << potential(pred(i)) << "), inbound: " << inbound(i) << std::endl;
            }
        }
    }

    //Check depth integrity
    for (i = 1; i < dim; i++)
    {
        if ((depth(pred(i)) + 1) != depth(i))
        {
            treeOK = false;
            std::cerr << "Depth of " << i << " is " << depth(i) << ", but depth of pred(i) " << pred(i) << " is " << depth(pred(i)) << std::endl;
        }
    }

    //Check thread integrity
    uint32_t allVisited[bits2intsize(dim)];
    base_resetBits(allVisited, bits2intsize(dim));
    base_setOneBit(allVisited, 0);
    uint32_t j = thread(0);
        
    for (i = 1; i < dim; i++)
    {
        base_setOneBit(allVisited, j);
        j = thread(j);
    }
    if (j != 0)
    {
        treeOK = false;
        std::cerr << "Thread doesn't return to the root!! Ended with: " << j << std::endl;
        printAllNodeInfo(nodes, rates, dim);
    }
    //For each node the subtree should match the thread.
    for (i = 1; i < dim; i++)
    {
        uint32_t size = bits2intsize(dim);
        uint32_t thread[size];
        uint32_t subtree[size];
        base_resetBits(thread, size);
        base_resetBits(subtree, size);
        j = thread(i);
        base_setOneBit(thread, i);
        base_setOneBit(subtree, i);
        //Getting the tread
        while (depth(j) > depth(i))
        {
            base_setOneBit(thread, j);
            j = thread(j);
        }
        //getting the subtree
        for (j = 1; j < dim; j++)
        {
            uint32_t k = j;
            while (k != 0)
            {
                if (k == i)
                {
                    base_setOneBit(subtree, j);
                }
                k = pred(k);
            }
        }
        for (j = 0; j < size; j++)
        {
            if (thread[j] != subtree[j])
            {
                treeOK = false;
                std::cerr << "Subtree of " << i << " doesn't match thread" << std::endl;
            }
        }
        for (i = 0; i < dim; i++)
        {
            if (!base_getOneBit(allVisited, i))
            {
                treeOK = false;
                std::cerr << "Node " << i << " not reach through thread!!" << std::endl;
            }
        }

    }
        
    return treeOK;
}

static inline bool nodeHasNoChildren(Node *node)
{
    return (node->thread->depth > node->depth);
}

static void printSolution(int32_t *valuation, const int32_t *rates, uint32_t dim)
{
    for (uint32_t i = 0; i < dim; i++)
    {
        std::cerr << "Val " << i << ": " << valuation[i] << " (rate: " << rates[i] << ")" << std::endl;
    }
}

static void printClockLowerBounds(const raw_t *dbm, uint32_t dim)
{
    for (uint32_t i = 1; i < dim; i++)
    {
        std::cerr << "Lower bound " << i << ": " << -constraintValue(0,i) << std::endl;
        ;
    }
}

static void printAllConstraints(const raw_t *dbm, uint32_t dim, arc_t *arcs, uint32_t nbConstraints)
{
    for (uint32_t i = 0; i < nbConstraints; i++)
    {
        std::cerr << "Constraint " << i << ": x_" << arcs[i].i << " - x_" << arcs[i].j << " <= " << constraintValue(arcs[i].i, arcs[i].j) << std::endl;
    }
}

static void printAllRates(const int32_t *rates, uint32_t dim)
{
    for (uint32_t i = 0; i < dim; i++)
    {
        std::cerr << "Rate[" << i << "]: " << rates[i] << std::endl;
    }
}

#endif
//END DEBUG SECTION


/* 
 * Set up the initial solution by introducing artificial arcs as necessary.
 * Whenever dbm is without
 *  ---   x_0 - x_i <= c_0i and b(i) <  0, introduce x_0 - x_i <= 0;
 *  ---   x_i - x_0 <= c_i0 and b(i) >= 0, introduce x_i - x_0 <= dbm_INFINITY
 * To guarantee strongly feasible spanning trees - and thus termination - we
 * also need outbound arc from transshipment nodes!
 *
 * The initial solution includes computing the intial node potentials.
 * 
 */
static void findInitialSpanningTreeSolution(
    const raw_t *dbm, uint32_t dim, const int32_t *rates, Node *nodes)
{  
    Node *last = nodes + dim;
    Node *node = nodes;
    node->pred     = NULL;
    node->depth    = 0;
    node->thread   = nodes + 1;
    node->inbound  = 0;
    node->flow     = -1;
    node->potential= 0;

    for (node++; node != last; node++)
    {
        uint32_t i = index(node);
        node->pred = nodes;
        node->depth = 1;
        node->thread = nodes + ((i + 1) % dim);
        node->flow = abs(rates[i]);
          
        if (b(i) < 0) // -rate(i) < 0
        {
            node->inbound = true;
            node->potential = -dbm_raw2bound(dbm[i]);
        }
        else // -rate(i) >= 0
        {
            node->inbound = false;
            node->potential = dbm_raw2bound(dbm[i*dim]);
        }
    }
        //printAllNodeInfo(nodes, rates, dim);
}

/*
 * Update the node potential so that alle reduced costs in the spanning
 * tree are zero. The subtree with the root is already up-to-date, so
 * it suffices to update the other subtree rooted at leave.
 *
 * Pre: leave should be on the path from arcs[enter].{i,j} to the root. 
 */

static void updatePotentials(Node *leave, int32_t change)
{  
    /* Update the entire subtree rooted at leave with a change
     * corresponding the reduced cost of the entering arc.  This is
     * enougn to guarantee that the reduced cost in the new spanning
     * tree remains zero. Node that the node from the entering arc
     * that is in the subtree is updated as well.
     *
     * The potential at leave is not updated, as the arc is being
     * removed anyway.
     */
    Node *z = leave;
    uint32_t d = leave->depth;
    do
    {
        z->potential += change;
        z = z->thread;
    } while (z->depth > d);
}

/**
 * Returns the last node according to preorder before \a exclude but
 * not before \a node.
 *
 * Node that this might be startNode itself.
 */
static Node *findLastNodeBefore(Node *node, Node *exclude)
{
    Node *i;
    do
    {
        i = node;
        node = node->thread;
    } while (node != exclude);
    return i;
}

/**
 * Starting from \a node, returns the first node according to preorder
 * for which the preorder successor has a depth not higher than
 * \a depth.
 */
static Node *findLastNodeBefore(Node *node, uint32_t depth)
{
    Node *i;
    do
    {
        i = node;
        node = node->thread;
    } while (node->depth > depth);
    return i;
}

/**
 * Returns the \a n'th predecessor of \a node. Returns \a node if \a n
 * is zero or negative.
 */
static Node *findNthPredecessor(Node *node, int32_t n)
{
    while (n > 0) 
    {
        assert(node);
        node = node->pred;
        n--;
    }
    return node;
}

/**
 * Returns true if and only if n is on the path to the root from m.
 */
static inline bool isPredecessorOf(Node *n, Node *m)
{
    return n == findNthPredecessor(m, m->depth - n->depth);
}

/**
 * Update all node information in the subtree not containing the root.
 * I.e., pred, depth, and thread.
 * 
 */
static void updateNonRootSubtree(Node *rootNode, Node *nonRootNode, Node *leave,
                                 bool sourceInRootSubtree, uint32_t flow)
{
    /*
     * Update thread information NOT COMPLETELY SURE ABOUT CORRECTNESS
     * BUT CONFIDENT ... and even more after testing
     *
     * Maybe it is about time I describe the idea - to come, Gerd.
     */

    //Find node that threads to leave;
    Node *pointToLeave = findLastNodeBefore(leave->pred, leave);
    Node *lastOut = findLastNodeBefore(nonRootNode, nonRootNode->depth);
    Node *preorderOut = lastOut->thread;
    Node *i = nonRootNode;
    while (i != leave)
    {
        Node *prev = i;
        i = lastOut->thread = i->pred;

        /*
         * Determine the point where thread(i) moves out of
         * i's subtrees and store the node in myPreorderOut.
         */
        lastOut = findLastNodeBefore(i, prev);

        if (i == preorderOut->pred)
        {
            lastOut->thread = preorderOut;
            //lastOut = findLastThreadedSubnode(nodes, preorderOut, 0);
            lastOut = findLastNodeBefore(preorderOut, i->depth);
            preorderOut = lastOut->thread;
        }
    }

    if (pointToLeave == rootNode)
    {
        /* 
         * If rootNode points to the leave subtree in 
         * preorder and is now changed to the root of 
         * the leave subtree, then preorderOut should
         * remain unchanged.
         */

        rootNode->thread = nonRootNode;
        lastOut->thread = preorderOut;
    }
    else
    {
        lastOut->thread = rootNode->thread;
        rootNode->thread = nonRootNode;
        pointToLeave->thread = preorderOut;
    }

    ASSERT(preorderOut->depth <= leave->depth, 
           std::cerr 
           << ", pointToLeave: " << pointToLeave 
           << ", lastOut: " << lastOut 
           << ", preorderOut: " << preorderOut 
           << ", depth(preorderOut): " 
           << preorderOut->depth 
           << ". depth(leave): " 
           << leave->depth);

    //Done updating thread information
  

    /*
     * Update the inbound, pred, and flow information
     * while tracing back the cycle. If (i,j) is still in the 
     * tree and nodes[i] had the information, then nodes[j] will
     * get that information, as nodes[k] will hold the information
     * about enter.
     */

    Node *tmppred1, *tmppred2, *newi;
    uint32_t tmpflow1, tmpflow2;
    bool tmpinbound1, tmpinbound2;
    tmppred1 = rootNode;
    tmpflow1 = flow;
    tmpinbound1 = !sourceInRootSubtree; //To handle the negation used later.

    newi = nonRootNode;
    do
    {
        i = newi;
        assert(i != NULL);
        //These are i's new values
        tmppred2 = tmppred1;
        tmpflow2 = tmpflow1;
        tmpinbound2 = tmpinbound1;
        //Save i's old values
        newi = i->pred;
        tmppred1 = i;
        tmpflow1 = i->flow;
        tmpinbound1 = i->inbound;
        //Update i's values
        i->pred = tmppred2;
        i->flow = tmpflow2; 
        i->inbound = !tmpinbound2;// NOTE the negation of the direction.
    } while (i != leave);

    // Done updating pred, inbound, and flow information

    /* 
     * Update depth information using the newly updated preorder 
     * thread. We reuse preorderOut as found when updating thread.
     * Requires that pred,thread, and depth have been updated 
     * correctly!
     */

    Node *stop = lastOut->thread;
    i = nonRootNode;
    do
    {
        i->depth = i->pred->depth + 1;
        i = i->thread;
    } while (i != stop);
}

/*
 * Update flow around the cycle introduced by enter, (k,l).
 * I.e., add flow to arcs in the direction of (k,l)
 * and subtract flow from arcs in the opposite direction
 * of (k,l).
 */
static void updateFlowInCycle(Node *k, Node *l, Node *root, uint32_t flowToAugment)
{
    if (flowToAugment > 0)
    {
        while (k != root)
        {
            k->flow += k->inbound ? flowToAugment : -flowToAugment;
            ASSERT(k->flow >= 0, 
                   std::cerr << "flow(" << k << "): " << k->flow << std::endl);
            k = k->pred;
        }
        while (l != root)
        {
            l->flow += l->inbound ? -flowToAugment : flowToAugment;
            ASSERT(l->flow >= 0, 
                   std::cerr << "flow(" << l << "): " << l->flow << std::endl);
            l = l->pred;
        }
    }
}

/**
 * Update the spanning tree so the node information is updated
 * including flow and node potentials.
 */
static void updateSpanningTree(Node *k, Node *l, Node *leave, Node *root,
                               int32_t costEnter)
{
    int32_t reducedCostEnter = costEnter - k->potential + l->potential;
    uint32_t flowToAugment = leave->flow;
    updateFlowInCycle(k, l, root, flowToAugment);
    if (!isPredecessorOf(leave, k))
    {
        updatePotentials(leave, -reducedCostEnter);
        updateNonRootSubtree(k, l, leave, true, flowToAugment);
    }
    else
    {
        updatePotentials(leave, reducedCostEnter);
        updateNonRootSubtree(l, k, leave, false, flowToAugment);
    }
}


/* 
 * ENTERING ARC FUNCTION REQUIREMENTS
 *
 * Any entering arc function should return NULL to indicate that no
 * eligible arc exists, thus, signifiying optimality of the current
 * solution. Otherwise an arc in [firstArc, lastArc) is returned.
 */

/**
 * Danzig's entering rule chooses the eligible arc with the 
 * lowest cost.
 */
static arc_t *enteringArcDanzig(arc_t *firstArc, arc_t *lastArc, 
                              Node *nodes, const raw_t *dbm, uint32_t dim)
{
    arc_t *best = NULL;
    int32_t lowest_reduced_cost = 0;

    while (firstArc != lastArc) 
    {
        /* Check if reduced cost is negative, i.e.  c_ij - pi(i) +
         * pi(j) < 0 for any arcs in arcs
         */
        int32_t reduced_cost = constraintValue(firstArc->i, firstArc->j) 
            - potential(firstArc->i) + potential(firstArc->j);

        if (reduced_cost < lowest_reduced_cost)
        {
            lowest_reduced_cost = reduced_cost;
            best = firstArc;
        }
        firstArc++;
    }
    assert(lowest_reduced_cost < 0 || best == NULL);

    return best;
}

// static arc_t *enteringArcFirstEligible(
//     arc_t *firstArc, arc_t *lastArc,
//     Node *nodes, const raw_t *dbm, uint32_t dim)
// {
//     while (firstArc != lastArc)
//     {
//         /* Check if reduced cost is negative, i.e.  c_ij - pi(i) +
//          * pi(j) < 0 for any arcs in arcs.
//          */
//         int32_t reduced_cost = constraintValue(firstArc->i, firstArc->j) 
//             - potential(firstArc->i) + potential(firstArc->j);
//         if (reduced_cost < 0)
//         {
//             return firstArc;
//         }
//         firstArc++;
//     }
//     return NULL;
// }


/**
 * Returns the common ancestor of nodes \a k and \a l with the biggest
 * depth.
 */
static Node *discoverCycleRoot(Node *k, Node *l)
{
    int32_t diff = k->depth - l->depth;
    k = findNthPredecessor(k, diff);
    l = findNthPredecessor(l, -diff);
    while (k != l)
    {
        k = k->pred;
        l = l->pred;
    }
    return k;
}

/* 
 * @param (\a k, \a l) the entering arc
 * @param \a root is discoverCycleRoot(k, l);
 *
 * Find first blocking arc if we augment flow in the direction of
 * (k,l). I.e. the arc oppositely directed of (k,l) with the lowest
 * flow. In case of a tie, choose the last arc from root in the
 * direction of (k,l) to maintain a strongly feasible spanning tree.
 * 
 * If no such arc exists it means that the solution is unbounded, which in
 * our case of zones cant happen as all priced zones have an infimum cost.
 * 
 * Actually, the node in the spanning tree representation that mentions the
 * the arc is returned. This is to compensate for the fact that it could be
 * an artificial arc which is represented by NULL.
 */
static Node *findLeavingArc(Node *k, Node *l, Node *root)
{  
    int32_t smallestFlow = INT_MAX;
    /* 
     * Node with the lowest depth of the leaving arc.
     */
    Node *smallestFlowNode = NULL;

    /*
     * Move towards the common root to find the
     * arc in the opposite direction of (k,l) with
     * the smallest flow.
     */
    while (k != root)
    {
        if (!k->inbound) // If not inbound then opposite of (i,j)
        {
            if (k->flow < smallestFlow) // < since we need the last along (i,j)
            {
                smallestFlow = k->flow;
                smallestFlowNode = k;
            }
        }
        k = k->pred;
    }
    while (l != root)
    {
        if (l->inbound) // If inbound then opposite of (i,j)
        {
            if (l->flow <= smallestFlow) // <= since we need the last along (i,j)
            {
                smallestFlow = l->flow;
                smallestFlowNode = l;
            }
        }
        l = l->pred;
    }
    ASSERT(smallestFlow != INT_MAX, 
           std::cerr  << "No oppositely directed arcs" << std::endl);

    return smallestFlowNode;
}

/* Verify that we have no artificial arcs involved, we know
 * they have zero flow, but they distrub the node potentials.
 * Not in the sense that the solution violates the constraints,
 * but it is in an artificial corner point generated by dbm_INFINITY
 * Since we want a trace through corner point, we will change it.
 *
 * If it is not a transshipment node, we need to follow the
 * thread to update the potentials. We do it by computing
 */
static void testAndRemoveArtificialArcs(const raw_t *dbm, const int32_t *rates,
                                        Node *nodes, uint32_t dim)
{
    for (uint32_t i = 1; i < dim; i++)
    {
        if (potential(i) == dbm_INFINITY && pred(i) == 0 && flow(i) == 0)
        {
            inbound(i) = true;
                        
            Node *tmp = nodes[i].thread;
            uint32_t rateSum = b(i);
                        
            int32_t minPotential = dbm_INFINITY + constraintValue(0,i);

            /*
             * Find the highest potential we can decrease i with and
             * maintain positive potentials.
             */
            while (tmp->depth > depth(i))
            {
                rateSum += b(index(tmp));
                if (tmp->potential < minPotential)
                    minPotential = tmp->potential;
                tmp = tmp->thread;
            }
            assert(rateSum == 0);

            /*
             * Update all potentials in the thread by subtracting
             * minPotential.
             *
             * Subtracting the same potetial maintains feasibility.
             */
            
            tmp = nodes + i;
            do
            {
                tmp->potential -= minPotential;
                tmp = tmp->thread;
            } while (tmp->depth > depth(i));
        }
    }
}

/**
 * Returns true if and only if all elements in [first, last) are
 * non-negative.
 */
static bool allPositive(const int32_t *first, const int32_t *last)
{
    while (first != last)
    {
        if (*first < 0)
        {
            return false;
        }
        first++;
    }
    return true;
}


/* 
 * Takes a priced zone and computes that infimum value of the dual
 * network flow problem. I.e. the cost of the offset and the costless
 * moves to the offset is not taken into account.
 */
static void infimumNetSimplex(const raw_t *dbm, uint32_t dim,
                              const int32_t *rates, Node *nodes)
{
    /* Find and store the minimal set of arcs. The netsimplex
     * algorithm is polynomial in the number of arcs.
     */
    uint32_t bitMatrix[bits2intsize(dim * dim)];
    uint32_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, bitMatrix);
    arc_t arcs[nbConstraints];
    arc_t *arc = arcs;
    for (uint32_t i = 0; i < dim; i++)
    {
        for (uint32_t j = 0; j < dim; j++)
        {
            if (base_readOneBit(bitMatrix, i * dim + j))
            {
                arc->i = i;
                arc->j = j;
                arc++;
            }
        }
    }
    assert(arc - arcs == (int32_t)nbConstraints);

    findInitialSpanningTreeSolution(dbm, dim, rates, nodes);

    ASSERT(checkTreeIntegrity(dbm, rates, nodes, dim), 
           printAllNodeInfo(nodes, rates, dim));

    /* 
     * Run the network simplex algorithm on the dual problem of the
     * zones. If z is the infimium value in the dbm with rates, then 
     * the network simplex algorithm computes -z. However, the value
     * of the node potential at the point of termination, matches 
     * precisely the infimum point of the zone.
     */
    while ((arc = enteringArcDanzig(arcs, arcs + nbConstraints, nodes, dbm, dim)))
    {
        Node *k = nodes + arc->i;
        Node *l = nodes + arc->j;

        /* Find common root in the cycle induced by introducing the
         * entering arc in the spanning tree.
         */
        Node *root = discoverCycleRoot(k, l);

        /* Leave will get the node that mentions the leaving arc.
         */
        Node *leave = findLeavingArc(k, l, root);

        updateSpanningTree(k, l, leave, root, 
                           constraintValue(arc->i, arc->j));

        ASSERT(checkTreeIntegrity(dbm, rates, nodes, dim), 
               printAllNodeInfo(nodes, rates, dim));
        //printAllNodeInfo(nodes, rates, dim);
    }

    /*
     * Get rid of artificial arcs.
     */
    testAndRemoveArtificialArcs(dbm, rates, nodes, dim);
}


/* 
 * Computes and returns the infimum over the zone given the cost rates.
 * At termination, valuation holds the clock valuation corresponding to
 * the infimum-achieving point in the zone.
 * 
 */
int32_t pdbm_infimum(const raw_t *dbm, uint32_t dim, uint32_t offsetCost, 
                     const int32_t *rates)
{
    if (allPositive(rates, rates + dim)) 
    {
        return offsetCost;
    }

    Node nodes[dim]; 
    infimumNetSimplex(dbm, dim, rates, nodes);

    int32_t solution = offsetCost;
    for (uint32_t i = 0; i < dim; i++)
    {
                ASSERT(potential(i) >= 0, std::cerr << "Node: " << i << 
                           std::endl; printAllNodeInfo(nodes, rates, dim); 
                           printClockLowerBounds(dbm, dim));
                /*
                 * Check if best solution has positive flow on artificial arc
                 * if so, return minus infinity as the solution is unbounded.
                 */
                if (potential(i) == dbm_INFINITY && pred(i) == 0 && flow(i) > 0)
                {
                        return -dbm_INFINITY;
                }
                solution += rates[i] * (potential(i) + constraintValue(0,i));
    }

    return solution;
}

void pdbm_infimum(const raw_t *dbm, uint32_t dim, uint32_t offsetCost, 
                  const int32_t *rates, int32_t *valuation)
{
    if (allPositive(rates, rates + dim)) 
    {
        valuation[0] = 0;
        for (uint32_t i = 1; i < dim; i++) {
            valuation[i] = -constraintValue(0,i);
        }
    } 
    else
    {
        Node nodes[dim]; 
        infimumNetSimplex(dbm, dim, rates, nodes);

        /* 
         * Assign the potentials to the best solution.
         */
        valuation[0] = 0;
        for (uint32_t i = 1; i < dim; i++)
        {
            ASSERT(potential(i) >= 0, std::cerr << "Node: " << i << std::endl; printAllNodeInfo(nodes, rates, dim); printSolution(valuation, rates, dim); printClockLowerBounds(dbm, dim));
                /*
                 * Check if best solution has positive flow on artificial arc
                 * if so, infimum is not well defined as it is minus infinity 
                 * and we throw an exception.
                 */
                if (potential(i) == dbm_INFINITY && pred(i) == 0 && flow(i) > 0)
                {
                        throw std::domain_error("Infimum is downward unbound, thus, no well-defined infimum valuation.");
                }

            valuation[i] = potential(i);
        }
    }
}

