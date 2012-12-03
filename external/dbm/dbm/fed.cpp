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

// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// Filename : fed.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: fed.cpp,v 1.66 2005/11/25 14:36:59 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#include <math.h>
#include <boost/bind.hpp>

#include "base/slist.h"
#include "base/bitstring.h"
#include "base/stats.h"
#include "base/doubles.h"
#include "dbm/mingraph.h"
#include "dbm/print.h"
#include "debug/macros.h"

#include "DBMAllocator.h"
#include "mingraph_coding.h"
#include "dbm.h"

// Always last.
#include "debug/new.h"

// This is to print how some DBM reductions perform.
#if defined(SHOW_STATS) && defined(VERBOSE)
#define CERR(S) (std::cerr<<S).flush()
#else
#define CERR(S)
#endif

// "Improved" may be more expensive sometimes.
#define IMPROVED_MERGE

// Shouldn't be necessary in practice.
//#define PARTITION_FIXPOINT

// Variants for subtractions:
// - desactive disjoint: #define NDISJOINT_SUBTRACTION
// - different algorithm: #define SUBTRACTION_ALGORITHM x
//   where x=0 => minimal graph only
//         x=1 => try to skip non intersecting facettes
//         x=2 => reordering of constraints
//         x=3 => reordering of constraints + skip intersecting facettes

//#define NDISJOINT_SUBTRACTION
//#define SUBTRACTION_ALGORITHM 0
//#define SUBTRACTION_ALGORITHM 1
//#define SUBTRACTION_ALGORITHM 2
//#define SUBTRACTION_ALGORITHM 3

// Default subtraction.
#ifndef SUBTRACTION_ALGORITHM
#define SUBTRACTION_ALGORITHM 3
#endif

namespace dbm
{
    const fdbm_t* fed_t::iterator::ENDF = NULL;
    const fed_t::const_iterator fed_t::const_iterator::ENDI(NULL);
    static bool restricted_merge = true;

#ifdef SHOW_STATS
#define INC() base::stats.count(MAGENTA(BOLD)"DBM: Subtraction splits", NULL)
#else
#define INC()
#endif

    /*****************************************
     * Functions to compute the subtraction.
     *****************************************/

#if SUBTRACTION_ALGORITHM == 1
#warning "Bad subtraction: Skip non intersecting facettes."

    // Approximate test.
    static inline
    bool isInside(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim, cindex_t i, cindex_t j)
    {
        cindex_t k = 0;
        size_t kdim = 0;
        size_t idim = i*dim;
        raw_t dbmij = dbm2[idim+j];

        assert(dim > 1 && i != j && dbmij != dbm_LS_INFINITY);

        do {
            if (k != i && k != j)
            {
                if (dbm2[kdim+j] != dbm_LS_INFINITY && dbm1[idim+k] != dbm_LS_INFINITY)
                {
                    if (dbmij - dbm2[kdim+j] > dbm1[idim+k] + 1) return false;
                }
                if (dbm2[idim+k] != dbm_LS_INFINITY && dbm1[kdim+j] != dbm_LS_INFINITY)
                {
                    if (dbmij - dbm2[idim+k] > dbm1[kdim+j] + 1) return false;
                }
            }
            ++k;
            kdim += dim;
        } while(k < dim);

        return true;
    }

    // Subtraction itself using constraints in the minimal graph (bits) only.
    // Return fdbm1 - dbm2, reading fdbm1 as one DBM and not a list.
    static
    dbmlist_t internSubtract(fdbm_t *fdbm1, const raw_t* dbm2, cindex_t dim,
                             const uint32_t *bits, size_t bitsSize, size_t nbConstraints)
    {
        assert(dim > 1);
        assertx(dbm_isValid(dbm2, dim));
        assert(nbConstraints && bits && fdbm1 && dbm2);
        assert(base_countBitsN(bits, bitsSize) == nbConstraints);

        uint32_t indices[nbConstraints];
        dbmlist_t result;
        raw_t* dbm1 = fdbm1->getMatrix();
        bool isMutable = false;
        uint32_t i = 0, j = 0, nb = 0;
        bool firstPass = true;

        for(;;)
        {
            /*********************** find constraint i,j to subtract ***************/
            uint32_t count = 32;
            for(uint32_t b = *bits++; b != 0; --count, ++j, b >>= 1)
            {
                for( ; (b & 1) == 0; --count, ++j, b >>= 1) // look for a bit
                {
                    assert(count && b);
                }
                FIX_IJ();
                do {
                    --nbConstraints;

                    assert(i != j && i < dim && j < dim);
                    assert(dbm2[i*dim+j] != dbm_LS_INFINITY); // in the minimal graph
                    
                    /******************* Subtraction for dbm2[i,j] *********************/
                    if (dbm2[i*dim+j] < dbm1[i*dim+j])
                    {
                        raw_t negConstraint = dbm_negRaw(dbm2[i*dim+j]);
                        if (negConstraint < dbm1[j*dim+i])
                        {
                            if (nb == 0 && nbConstraints == 0)
                            {
                                dbm_tighten(isMutable ? dbm1 : fdbm1->dbmt().getCopy(), dim, j, i, negConstraint);
                                INC();
                                return result.append(fdbm1);
                            }
                            if (firstPass && !isInside(dbm1, dbm2, dim, i, j))
                            {
                                indices[nb] = i | (j << 16);
                                nb++;
                            }
                            else
                            {
                                dbm_tighten(result.append(dbm1, dim), dim, j, i, negConstraint);
                                INC();
#ifndef NDISJOINT_SUBTRACTION
                                if (!isMutable)
                                {
                                    dbm1 = fdbm1->dbmt().getCopy();
                                    isMutable = true;
                                }
                                dbm_tighten(dbm1, dim, i, j, dbm2[i*dim+j]); // remainder
#else
#warning "Subtraction: Result not disjoint."
#endif
                            }
                        }
                        else
                        {
                            // dbm2[i,j] < dbm1[i,j] => dbm2 tightens dbm1
                            // -dbm2[i,j] >= dbm1[j,i] => substraction == remainder
                            return result.append(fdbm1);
                        }
                    }

                    /********************** Finish 1st pass or 2nd pass ************/
                    if (!nbConstraints)
                    {
                        if (nb == 0) goto finish_subtract;
                        firstPass = false;
                        nbConstraints = nb;
                        nb = 0;
                    }
                    if (!firstPass)
                    {
                        i = indices[nbConstraints - 1] & 0xffff;
                        j = indices[nbConstraints - 1] >> 16;
                    }
                } while(!firstPass);

                /******************* Finish to read constraints i,j ****************/
                assert(count);
            }
            j += count; // unread bits
        }

        /************** Deallocate remainder left and return result ****************/
    finish_subtract:
        fdbm1->dbmt().nil();
        fdbm1->remove();
        return result;
    }

#elif SUBTRACTION_ALGORITHM >= 2
    
#if SUBTRACTION_ALGORITHM == 3
    // Get the worst ordering value for a constraint.
    // Original test: -(zkj+zji') >= cik => outside
    // which translates to zij-zkj >= cik
    // if zkj == inf then inside
    // if cik == inf then inside
    // we want an ordering for the constraints
    // so we take zij-zkj-cik: high==outside low==inside
    // and we treat separately infinity to avoid overflow.
    // zxy stands for constraint xy of dbm2 and cxy for constraint
    // xy for dbm1. We treat the symmetric case as well.
    static
    int32_t worstValue(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim, cindex_t i, cindex_t j)
    {
        cindex_t k = 0;
        size_t kdim = 0;
        size_t idim = i*dim;
        raw_t dbmij = dbm2[idim+j] | 1;

        assert(dim > 1 && i != j && dbmij != dbm_LS_INFINITY);

        do {
            if (k != i && k != j)
            {
                if (dbm2[kdim+j] != dbm_LS_INFINITY && dbm1[idim+k] != dbm_LS_INFINITY)
                {
                    int32_t v = dbmij - ((dbm1[idim+k]|1) + (dbm2[kdim+j]|1));
                    if (v >= 0) return dbm_LS_INFINITY;
                }
                if (dbm2[idim+k] != dbm_LS_INFINITY && dbm1[kdim+j] != dbm_LS_INFINITY)
                {
                    int32_t v = dbmij - ((dbm1[kdim+j]|1) + (dbm2[idim+k]|1));
                    if (v >= 0) return dbm_LS_INFINITY;
                }
            }
            ++k;
            kdim += dim;
        } while(k < dim);

        return dbmij - dbm1[idim+j];
    }
#endif

    // Subtraction itself using constraints in the minimal graph (bits) only.
    // Return fdbm1 - dbm2, reading fdbm1 as one DBM and not a list.
    static
    dbmlist_t internSubtract(fdbm_t *fdbm1, const raw_t* dbm2, cindex_t dim,
                             const uint32_t *bits, size_t bitsSize, size_t nbConstraints)
    {
        assert(dim > 1);
        assertx(dbm_isValid(dbm2, dim));
        assert(nbConstraints && bits && fdbm1 && dbm2);
        assert(base_countBitsN(bits, bitsSize) == nbConstraints);

        uint32_t indices[nbConstraints];
        raw_t* dbm1 = fdbm1->getMatrix();
        dbmlist_t result;
        bool isMutable = false;
        uint32_t i = 0, j = 0, c = 0, k = nbConstraints;

        // Write all the indices once since we will use them several times
        // but choose only those that have some effect on the subtraction.
        for(;;)
        {
            uint32_t count = 32;
            for(uint32_t b = *bits++; b != 0; --count, ++j, b >>= 1)
            {
                for( ; (b & 1) == 0; --count, ++j, b >>= 1) // look for a bit
                {
                    assert(count && b);
                }
                FIX_IJ();
                if (dbm2[i*dim+j] < dbm1[i*dim+j]) // tighten?
                {
                    indices[c++] = i | (j << 16); // accept i,j
                }
                if (!--k) goto indices_read;
                assert(count);
            }
            j += count; // unread bits
        }
    indices_read:
        if (!c) goto finish_subtract;
        nbConstraints = c; // number of accepted constraints

        do {
            /*********************** find constraint i,j to subtract ***************/
            int32_t bestv = INT_MAX;
            k = 0;
            DODEBUG(c = ~0);
            do {
                cindex_t ci = indices[k] & 0xffff;
                cindex_t cj = indices[k] >> 16;
                // If dbm2 outside dbm1 then no more subtraction.
                assert(dbm2[ci*dim+cj] != dbm_LS_INFINITY); // because part of mingraph
                if (dbm_negRaw(dbm2[ci*dim+cj]) >= dbm1[cj*dim+ci])
                {
                    return result.append(fdbm1);
                }
                if (dbm2[ci*dim+cj] >= dbm1[ci*dim+cj])
                {
                    if (!--nbConstraints) goto finish_subtract;
                    // Playing with the order is sometimes better, sometimes worse.
                    indices[k] = indices[nbConstraints];
                    continue;
                }
                // need to recompute everytime because dbm1 changes
                if (bestv > -dbm_LS_INFINITY)
                {
                    if (dbm1[ci*dim+cj] == dbm_LS_INFINITY)
                    {
                        bestv = -dbm_LS_INFINITY;
                        i = ci;
                        j = cj;
                        c = k;
                        // Don't break the loop since the 1st test may
                        // cancel the split.
                    }
                    else
                    {
#if SUBTRACTION_ALGORITHM == 2
                        int32_t cv = dbm2[ci*dim+cj] - dbm1[ci*dim+cj];
#elif SUBTRACTION_ALGORITHM == 3
                        int32_t cv = worstValue(dbm1, dbm2, dim, ci, cj);
#else
#error "Invalid algorithm."
#endif
                        if (bestv > cv)
                        {
                            bestv = cv;
                            i = ci;
                            j = cj;
                            c = k;
                        }
                    }
                }
                k++;
            } while(k < nbConstraints);

            assert(c != (uint32_t) ~0 && c < nbConstraints);  // found one index
            assert(bestv != dbm_LS_INFINITY);                 // found one index
            indices[c] = indices[--nbConstraints];            // Swap with last

            assert(i != j && i < dim && j < dim);
            assert(dbm2[i*dim+j] != dbm_LS_INFINITY);

            /******************* Subtraction for dbm2[i,j] *********************/
            if(dbm2[i*dim+j] < dbm1[i*dim+j])
            {
                raw_t negConstraint = dbm_negRaw(dbm2[i*dim+j]);
                assert(negConstraint < dbm1[j*dim+i]); // checked before
                if (!nbConstraints) // is last constraint?
                {
                    dbm_tighten(isMutable ? dbm1 : fdbm1->dbmt().getCopy(), dim, j, i, negConstraint);
                    INC();
                    return result.append(fdbm1);
                }
                dbm_tighten(result.append(dbm1, dim), dim, j, i, negConstraint);
                INC();
#ifndef NDISJOINT_SUBTRACTION
                if (!isMutable)
                {
                    dbm1 = fdbm1->dbmt().getCopy();
                    isMutable = true;
                }
                dbm_tighten(dbm1, dim, i, j, dbm2[i*dim+j]); // remainder
#else
#warning "Subtraction: Result not disjoint."
#endif
            }
        } while(nbConstraints);

        /************** Deallocate remainder left and return result ****************/
    finish_subtract:
        fdbm1->dbmt().nil();
        fdbm1->remove();
        return result;
    }

#elif SUBTRACTION_ALGORITHM == 0
#warning "Bad subtraction: Minimal graph only."

    // Subtraction itself using constraints in the minimal graph (bits) only.
    // Return fdbm1 - dbm2, reading fdbm1 as one DBM and not a list.
    static
    dbmlist_t internSubtract(fdbm_t *fdbm1, const raw_t* dbm2, cindex_t dim,
                             const uint32_t *bits, size_t bitsSize, size_t nbConstraints)
    {
        assert(dim > 1);
        assertx(dbm_isValid(dbm2, dim));
        assert(nbConstraints && bits && fdbm1 && dbm2);
        assert(base_countBitsN(bits, bitsSize) == nbConstraints);

        dbmlist_t result;
        raw_t* dbm1 = fdbm1->getMatrix();
        bool isMutable = false;
        uint32_t i = 0, j = 0, c = 0;
        bool hasMore = true;

        do {
            /*********************** find constraint i,j to subtract ***************/
            uint32_t count = 32;
            for(uint32_t b = *bits++; b != 0; --count, ++j, b >>= 1)
            {
                for( ; (b & 1) == 0; --count, ++j, b >>= 1) // look for a bit
                {
                    assert(count && b);
                }
                FIX_IJ();
                hasMore = ++c < nbConstraints;

                assert(i != j && i < dim && j < dim);
                assert(dbm2[i*dim+j] != dbm_LS_INFINITY); // in the minimal graph

                /******************* Subtraction for dbm2[i,j] *********************/
                if (dbm2[i*dim+j] < dbm1[i*dim+j])
                {
                    raw_t negConstraint = dbm_negRaw(dbm2[i*dim+j]);
                    if (negConstraint < dbm1[j*dim+i])
                    {
                        if (!hasMore) // is last constraint?
                        {
                            dbm_tighten(isMutable ? dbm1 : fdbm1->dbmt().getCopy(), dim, j, i, negConstraint);
                            INC();
                            return result.append(fdbm1);
                        }
                        dbm_tighten(result.append(dbm1, dim), dim, j, i, negConstraint);
                        INC();
#ifndef NDISJOINT_SUBTRACTION
                        if (!isMutable)
                        {
                            dbm1 = fdbm1->dbmt().getCopy();
                            isMutable = true;
                        }
                        dbm_tighten(dbm1, dim, i, j, dbm2[i*dim+j]); // remainder
#else
#warning "Subtraction: Result not disjoint."
#endif
                    }
                    else
                    {
                        // dbm2[i,j] < dbm1[i,j] => dbm2 tightens dbm1
                        // -dbm2[i,j] >= dbm1[j,i] => substraction == remainder
                        return result.append(fdbm1);
                    }
                }

                /******************* Finish to read constraints i,j ****************/
                assert(count);
            }
            j += count; // unread bits
        } while(hasMore);

        /************** Deallocate remainder left and return result ****************/
        fdbm1->dbmt().nil();
        fdbm1->remove();
        return result;
    }

#else
#error "Invalid algorithm."
#endif

    /***************
     * fdbm_t
     ***************/

    // Not inlined to reduce code size since fdbm_allocator expands a lot.
    fdbm_t* fdbm_t::create(fdbm_t* nxt)
    {
        assert(sizeof(alloc_fdbm_t) == sizeof(fdbm_t));
        fdbm_t *fdbm = reinterpret_cast<fdbm_t*>(fdbm_allocator.allocate());
        fdbm->next = nxt;
        return fdbm;
    }

    fdbm_t* fdbm_t::copy(const fdbm_t* start, fdbm_t* end)
    {
        fdbm_t *next = end;
        for(const fdbm_t* current = start; current != NULL; current = current->next)
        {
            next = fdbm_t::create(current->idbm, next);
        }
        return next;
    }

    void fdbm_t::removeAll(fdbm_t *fhead)
    {
        for(fdbm_t *next = fhead; next != NULL; )
        {
            fdbm_t *fdbm = next;
            next = next->next; // before removing!
            fdbm->idbm.nil();
            fdbm->remove();
        }
    }

    fdbm_t* fdbm_t::append(fdbm_t *start, fdbm_t *end)
    {
        if (!start) return end;
        if (end)
        {
            fdbm_t* fdbm;
            for(fdbm = start; fdbm->next; fdbm = fdbm->next);
            fdbm->next = end;
        }
        return start;
    }

    /**************
     * dbmlist_t
     **************/

    void dbmlist_t::removeIncluded(dbmlist_t& arg)
    {
        RECORD_STAT();
        for(fdbm_t **i = &fhead; *i != NULL; )
        {
            fdbm_t **j = &arg.fhead;
            if (!*j) break; // stop all
            do {
                switch((*i)->const_dbmt().relation((*j)->const_dbmt()))
                {
                case base_EQUAL:
                case base_SUBSET: // remove from i (this)
                    RECORD_SUBSTAT("<=");
                    *i = (*i)->removeAndNext();
                    decSize();
                    goto continue_i;
                case base_SUPERSET: // remove from j (arg)
                    RECORD_SUBSTAT(">");
                    *j = (*j)->removeAndNext();
                    arg.decSize();
                    break; // continue j
                case base_DIFFERENT: // next j
                    j = (*j)->getNextMutable();
                }
            } while(*j != NULL);
            i = (*i)->getNextMutable(); // next i
        continue_i:
            ;
        }
    }

    void dbmlist_t::reduce(cindex_t dim)
    {
        if (size() > 1)
        {
            RECORD_STAT();
            fdbm_t **head = &fhead;
            for(fdbm_t **fi = head; *fi != NULL; )
            {
                const raw_t *dbmi = (*fi)->const_dbmt().const_dbm();
                for(fdbm_t **fj = (*fi)->getNextMutable(); *fj != NULL; )
                {
                    switch(dbm_relation(dbmi, (*fj)->const_dbmt().const_dbm(), dim))
                    {
                    case base_DIFFERENT:
                        // next j
                        fj = (*fj)->getNextMutable();
                        break;
                    case base_SUPERSET:
                    case base_EQUAL:
                        RECORD_SUBSTAT("<=");
                        // remove j
                        *fj = (*fj)->removeAndNext();
                        decSize();
                        break;
                    case base_SUBSET:
                        RECORD_SUBSTAT("<");
                        // remove i
                        *fi = (*fi)->removeAndNext();
                        decSize();
                        goto reduce_abortj;
                    }
                }
                fi = (*fi)->getNextMutable();
            reduce_abortj: ;
            }
        }
    }
    
#ifdef IMPROVED_MERGE
    // returns true if no intersection for sure with weak constraints
    static inline
    bool fed_checkWeakAdd(raw_t cij, raw_t cji)
    {
        return (cij != dbm_LS_INFINITY &&
                cji != dbm_LS_INFINITY &&
                (cij + cji - (cij & cji & 1)) < dbm_LE_ZERO);
    }
#endif

    void dbmlist_t::mergeReduce(cindex_t dim, size_t jumpi, int level)
    {
        // at least 2 DBMs
        if (size() > 1)
        {
            RECORD_STAT();
            // side effect on all copies
            fdbm_t **head = &fhead;

            // Jump already (assumed) reduced head.
            fdbm_t **fi;
            for(fi = head; jumpi != 0; --jumpi)
            {
                if (!*fi) return;
                fi = (*fi)->getNextMutable();
            }

            // Continue.
            for(; *fi != NULL; )
            {
                const dbm_t& dbmi = (*fi)->const_dbmt();
                for(fdbm_t **fj = head; fj != fi; )
                {
                    const dbm_t& dbmj = (*fj)->const_dbmt();
                    const raw_t* dbm1 = dbmi.const_dbm();
                    const raw_t* dbm2 = dbmj.const_dbm();
                    cindex_t nbOK = (dim <= 2);
                    int superset = 1, subset = 1;

                    for(cindex_t i = 1; i < dim; ++i)
                    {
                        cindex_t constraintsOK = 0;
                        for(cindex_t j = 0; j < i; ++j)
                        {
                            uint32_t ij = i*dim+j;
                            uint32_t ji = j*dim+i;
                            
                            // No intersection for sure
#ifdef IMPROVED_MERGE
                            if (fed_checkWeakAdd(dbm1[ij], dbm2[ji]) ||
                                fed_checkWeakAdd(dbm1[ji], dbm2[ij]))
#else
                            if ((dbm1[ij] != dbm_LS_INFINITY &&
                                 dbm2[ji] != dbm_LS_INFINITY &&
                                 dbm_negRaw(dbm_weakRaw(dbm1[ij])) >= dbm_weakRaw(dbm2[ji]))
                                ||
                                (dbm1[ji] != dbm_LS_INFINITY &&
                                 dbm2[ij] != dbm_LS_INFINITY &&
                                 dbm_negRaw(dbm_weakRaw(dbm1[ji])) >= dbm_weakRaw(dbm2[ij])))
#endif
                            {
                                goto next_fj;
                            }
                            
                            // Relation by the way :)
                            subset &= dbm1[ij] <= dbm2[ij];
                            superset &= dbm1[ij] >= dbm2[ij];
                            subset &= dbm1[ji] <= dbm2[ji];
                            superset &= dbm1[ji] >= dbm2[ji];
                            
                            // Compatible constraints
                            constraintsOK = constraintsOK |
                                ((dbm1[ij] == dbm2[ij]) & // | possible
                                 (dbm1[ji] == dbm2[ji]));
                        }
                        nbOK += constraintsOK;
                    }
                    if (subset) // remove dbmi
                    {
                        RECORD_SUBSTAT("<=");
                        *fi = (*fi)->removeAndNext();
                        decSize();
                        goto continue_fi;
                    }
                    else if (superset) // remove dbmj
                    {
                        RECORD_SUBSTAT(">=");
                        if ((*fj)->hasNext(fi))
                        {
                            fi = fj; // otherwise segfault when reading *fi
                        }
                        *fj = (*fj)->removeAndNext();
                        decSize();
                        continue;
                    }
                    else if ((level || !restricted_merge) ? nbOK > 0 : nbOK + 2 >= dim)
                    {
                        RECORD_SUBSTAT("mergeable");
                        dbm_t convex = dbmi;
                        convex += dbmj;
                        
                        assert(((fed_t(convex) -= dbmi) <= dbmj) ==
                               ((fed_t(convex) -= dbmi) -= dbmj).isEmpty());
                        
                        //if ((fed_t(convex) -= dbmi) <= dbmj)
                        fed_t fc(convex);
                        bool safeMerge = (fc -= dbmi) <= dbmj;
                        if (!safeMerge && level)
                        {
                            fc -= dbmj;
                            assert(!fc.isEmpty());
                            if (level == 1)
                            {
                                // See if (convex union)-(dbmi|dbmj) is included somewhere.
                                for(fdbm_t **fk = head; *fk != NULL; fk = (*fk)->getNextMutable())
                                {
                                    if (fk != fi && fk != fj)
                                    {
                                        fc.removeIncludedIn((*fk)->const_dbmt());
                                        if (fc.isEmpty())
                                        {
                                            safeMerge = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                // Remove incrementally DBMs from (convex union)-(dbmi|dbmj)
                                // and check if the remaining becomes empty.
                                for(fdbm_t **fk = head; *fk != NULL; fk = (*fk)->getNextMutable())
                                {
                                    if (fk != fi && fk != fj && (fc -= (*fk)->const_dbmt()).isEmpty())
                                    {
                                        safeMerge = true;
                                        break;
                                    }
                                }
                            }
                            //(std::cout << (safeMerge ? GREEN(BOLD) : RED(BOLD)) << "*" NORMAL).flush();
                        }
                        if (safeMerge)
                        {
                            RECORD_SUBSTAT("merged");
                            (*fi)->dbmt().updateCopy(convex);
                            if ((*fj)->hasNext(fi))
                            {
                                fi = fj; // otherwise segfault when reading *fi
                            }
                            *fj = (*fj)->removeAndNext();
                            decSize();
                            fj = head; // restart from beginning for this merge
                            continue;
                        }
                    }
                next_fj:
                    fj = (*fj)->getNextMutable();
                }
                fi = (*fi)->getNextMutable();
            continue_fi: ;
            }
        }
    }

    dbmlist_t& dbmlist_t::intersection(const raw_t *arg, cindex_t dim)
    {
        assertx(dbm_isValid(arg, dim));
        bool copied = false;
        for(fdbm_t **i = &fhead; *i != NULL; )
        {
            if ((*i)->dbmt().ptr_intersectionIsArg(arg, dim))
            {
                if (copied) // copy arg only once
                {
                    (*i)->dbmt().nil();
                }
                else
                {
                    (*i)->dbmt().updateCopy(arg, dim);
                    copied = true;
                }
            }
            if ((*i)->dbmt().isEmpty())
            {
                *i = (*i)->removeEmptyAndNext();
                decSize();
            }
            else
            {
                i = (*i)->getNextMutable();
            }
        }
        return *this;
    }

    dbmlist_t& dbmlist_t::intersection(const dbm_t& arg, cindex_t dim)
    {
        assert(!arg.isEmpty() && arg.pdim() == dim);
        bool copied = false;
        for(fdbm_t **i = &fhead; *i != NULL; )
        {
            if ((*i)->dbmt().ptr_intersectionIsArg(arg.const_dbm(), dim))
            {
                if (copied) // copy arg only once
                {
                    (*i)->dbmt().nil();
                }
                else
                {
                    (*i)->dbmt().updateCopy(arg); // reference copy
                    copied = true;
                }
            }
            if ((*i)->dbmt().isEmpty())
            {
                *i = (*i)->removeEmptyAndNext();
                decSize();
            }
            else
            {
                i = (*i)->getNextMutable();
            }
        }
        return *this;
    }

#ifndef NDEBUG
    void dbmlist_t::print(std::ostream& os) const
    {
        int i = 0;
        for(const fdbm_t* f = fhead; f != NULL; f = f->getNext())
        {
            const dbm_t& d = f->const_dbmt();
            os << '[' << i++ << "]:\n";
            if (d.isEmpty())
            {
                os << "empty(" << d.getDimension() << ")\n";
            }
            else
            {
                os << d;
            }
        }
    }
    void dbmlist_t::err() const { print(std::cerr); }
    void dbmlist_t::out() const { print(std::cout); }
#endif

    /************
     * ifed_t
     ************/

    // Not inlined to reduce code size since ifed_allocator expands a lot.
    ifed_t* ifed_t::create(cindex_t dim, size_t size, fdbm_t* head)
    {
        assert(sizeof(ifed_t) == sizeof(alloc_ifed_t));
        ifed_t *ifed = reinterpret_cast<ifed_t*>(ifed_allocator.allocate());
        ifed->refCounter = 1;
        ifed->fedSize = size;
        ifed->dim = dim;
        ifed->fhead = head;
        return ifed;
    }

    void ifed_t::remove()
    {
        assert(refCounter == 0);
        fdbm_t::removeAll(fhead);
        ifed_allocator.deallocate(reinterpret_cast<alloc_ifed_t*>(this));
    }

    // compute a hash value from all its DBMs
    uint32_t ifed_t::hash(uint32_t seed) const
    {
        // The problem is here to return the same hash for several
        // fed_t that have the same dbm_t in different order.

        uint32_t hashValue[size()];
        uint32_t ddim = getDimension();
        uint32_t i = 0;
        ddim *= ddim;
        for(const fdbm_t* fdbm = fhead; fdbm; fdbm = fdbm->getNext())
        {
            hashValue[i++] = hash_computeI32(fdbm->const_dbmt().const_dbm(), ddim, ddim);
        }
        base_sort(hashValue, i);                    // independent from order of DBMs
        return hash_computeU32(hashValue, i, seed); // combine the hash values
    }

    /***************
     * fed_t
     ***************/
    
    void fed_t::heuristicMergeReduce(bool active)
    {
        restricted_merge = active;
    }

    std::string fed_t::toString(const ClockAccessor& access) const
    {
        if (isEmpty())
        {
            return "false";
        }
        std::string str;
        bool isFirst = true;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (!isFirst)
            {
                str += " || ";
            }
            str += i->toString(access);
            isFirst = false;
        }
        return str;
    }

    bool fed_t::hasZero() const
    {
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (i->hasZero())
            {
                return true;
            }
        }
        return false;
    }


    void fed_t::intern()
    {
        for(const_iterator iter = begin(); !iter.null(); ++iter)
        {
            // cheat on the const because this does not modify the DBM
            // itself. This pospones copies of ifed_t and dbm_t.
            const_cast<dbm_t&>(*iter).intern();
        }
    }

    void fed_t::setDimension(cindex_t dim)
    {
        assert(isOK());

        if (isMutable())
        {
            ifed()->setEmpty();
            ifed()->setDimension(dim);
        }
        else
        {
            decRefImmutable();
            ifedPtr = ifed_t::create(dim);
        }
    }

    void fed_t::setEmpty()
    {
        assert(isOK());

        if (!isEmpty())
        {
            if (isMutable())
            {
                ifed()->setEmpty();
            }
            else
            {
                decRefImmutable();
                ifedPtr = ifed_t::create(getDimension());
            }
        }
    }

    // Similar to dbm_t::getUpperMinimumCost.
    int32_t fed_t::getUpperMinimumCost(int32_t cost) const
    {
        assert(isOK());

        if (isEmpty())
        {
            return -dbm_INFINITY;
        }

        int32_t bound = -dbm_INFINITY;

        for(const_iterator i = begin(); !i.null(); ++i)
        {
            int32_t partial = i->getUpperMinimumCost(cost);
            if (partial < dbm_INFINITY)
            {
                bound = std::max(bound, partial);
            }
        }

        return bound == -dbm_INFINITY ? dbm_INFINITY : bound;
    }

    dbm_t fed_t::makeUnboundedFrom(const int32_t* low) const
    {
        assert(isOK());

        cindex_t dim = getDimension();
        dbm_t res = dbm_t::makeUnbounded(low, dim);
        raw_t *dbm = res.dbm();
        assert(!res.isEmpty());
        for(cindex_t i = 1; i < dim; ++i)
        {
            for(const_iterator j = begin(); !j.null(); ++j)
            {
                raw_t c = j->const_dbm()[i];
                if (dbm[i] > c)
                {
                    dbm[i] = c;
                }
            }
        }
        return res;
    }

    int32_t fed_t::maxOnZero(cindex_t x)
    {
        assert(isOK());
        assert(!isEmpty());
        
        for(iterator i = beginMutable(); !i.null(); )
        {
            if (i->intersectionAxis(x))
            {
                ++i;
            }
            else
            {
                i->nil();
                i.removeEmpty();
            }
        }

        int32_t max = 0;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            int32_t bound = dbm_raw2bound((*i)(x,0));
            if (bound > max)
            {
                max = bound;
            }
        }
        return max;
    }

    fed_t& fed_t::operator = (const dbm_t& arg)
    {
        assert(isOK());
        
        if (arg.isEmpty())
        {
            setDimension(arg.edim());
        }
        else if (isMutable())
        {
            ifed()->setToDBM(arg);
        }
        else
        {
            decRefImmutable();
            ifedPtr = ifed_t::create(arg);
        }
        
        return *this;
    }

    fed_t& fed_t::operator = (const raw_t *arg)
    {
        assert(isOK());

        cindex_t dim = getDimension();
        if (isMutable())
        {
            if (size() == 1)
            {
                ifed()->update(arg, dim);
            }
            else
            {
                ifed()->setEmpty();
                ifed()->insert(arg, dim);
            }
        }
        else
        {
            decRefImmutable();
            ifedPtr = ifed_t::create(arg, dim);
        }
        return *this;
    }

    // Specialized version: look for one DBM of this s.t. <= arg
    // and terminate earlier.
    bool fed_t::operator <= (const dbm_t& arg) const
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (isEmpty())
        {
            return true;
        }
        else if (arg.isEmpty())
        {
            return false;
        }
        else
        {
            cindex_t dim = getDimension();
            for(const_iterator iter = begin(); !iter.null(); ++iter)
            {
                if (!dbm_isSubsetEq(iter->const_dbm(), arg.const_dbm(), dim))
                {
                    return false;
                }
            }
            return true;
        }
    }

    // Specialized version: look for one DBM of this s.t. >= arg
    // and terminate earlier.
    bool fed_t::operator >= (const dbm_t& arg) const
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (arg.isEmpty())
        {
            return true;
        }
        else if (isEmpty())
        {
            return false;
        }
        else
        {
            cindex_t dim = getDimension();
            for(const_iterator iter = begin(); !iter.null(); ++iter)
            {
                if (dbm_isSupersetEq(iter->const_dbm(), arg.const_dbm(), dim))
                {
                    return true;
                }
            }
            return false;
        }
    }
    
    // Algorithm:
    // - first part: get rid or the trivial cases
    // - second part:
    // check for SUBSET if all DBMs of this <= some DBM of arg
    // check for SUPERSET if all DBMs of arg <= some DBM of this
    // result = SUBSET|SUPERSET => can be EQUAL if both,
    // DIFFERENT if none, SUBSET, or SUPERSET.
    relation_t fed_t::relation(const fed_t& arg) const
    {
        assert(isOK());
        assert(arg.isOK());

        if (sameAs(arg))
        {
            return base_EQUAL;
        }
        cindex_t dim = getDimension();
        if (dim != arg.getDimension())
        {
            return base_DIFFERENT;
        }
        else if (isEmpty())
        {
            return arg.isEmpty() ? base_EQUAL : base_SUBSET;
        }
        else if (arg.size() <= 1)
        {
            return arg.size() < 1 ? base_SUPERSET : ptr_relation(arg.const_dbmt().const_dbm(), dim);
        }
        else
        {
            // Prepare for cross-relations
            size_t thisSize = size();
            size_t argSize = arg.size();
            size_t maxSize = thisSize < argSize ? argSize : thisSize;
            uint32_t crossRel[maxSize];
            base_resetSmall(crossRel, maxSize);
            assert(thisSize && argSize);
            
            //const raw_t* argDBM[argSize];
            //arg.toArray(argDBM);

            uint32_t i = 0;
            for(const_iterator iter = begin(); i < thisSize; ++i, ++iter)
            {
                uint32_t j = 0;
                //do {
                for(const_iterator jter = arg.begin(); j < argSize; ++j, ++jter)
                {
                    // if there is some information to gain
                    if ((base_sub2super((relation_t)crossRel[i]) & crossRel[j] & base_SUPERSET) == 0)
                    {
                        relation_t rel = dbm_relation(iter->const_dbm(), jter->const_dbm() /*argDBM[j]*/, dim);
                        crossRel[i] |= rel & base_SUBSET;   // all of this <= some of arg
                        crossRel[j] |= rel & base_SUPERSET; // all of arg <= some of this
                    }
                }
                //} while(++j < argSize);
            }
            
            // Compute global relation between federations
            uint32_t subset = base_SUBSET;
            uint32_t superset = base_SUPERSET;
            i = 0;
            do { subset &= crossRel[i]; } while(++i < thisSize && subset);
            i = 0;
            do { superset &= crossRel[i]; } while(++i < argSize && superset);
            
            // Result:
            // SUBSET if all DBMs of this <= some DBM of arg
            // SUPERSET if all DBMs of arg <= some DBM of this
            // EQUAL if both, DIFFERENT if none
            
            return (relation_t) (subset|superset);
        }
    }

    // similar to dbm_t::relation(dbm_t&) since we are using
    // the same encoding.
    relation_t fed_t::relation(const dbm_t& arg) const
    {
        assert(isOK());

        cindex_t dim = getDimension();
        if (dim != arg.getDimension())
        {
            return base_DIFFERENT;
        }
        else if (isEmpty())
        {
            return arg.isEmpty() ? base_EQUAL : base_SUBSET;
        }
        else if (arg.isEmpty())
        {
            return base_SUPERSET;
        }
        else
        {
            return ptr_relation(arg.const_dbm(), dim);
        }
    }

    relation_t fed_t::relation(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assertx(dbm_isValid(arg, dim));

        if (dim != getDimension())
        {
            return base_DIFFERENT;
        }
        else if (isEmpty())
        {
            return base_SUBSET;
        }
        else
        {
            return ptr_relation(arg, dim);
        }
    }

    // Real implementation of relation(dbm_t or raw_t*)
    relation_t fed_t::ptr_relation(const raw_t* arg, cindex_t dim) const
    {
        assert(!isEmpty());

        if (size() == 1)
        {
            return dbm_relation(const_dbmt().const_dbm(), arg, dim);
        }
        else
        {
            // subset: if subset for all DBMs => &=
            // superset: if superset for one DBM => |=
            uint32_t subset = base_SUBSET;
            uint32_t superset = 0;
            
            for(const_iterator iter = begin(); !iter.null(); ++iter)
            {
                relation_t rel = dbm_relation(iter->const_dbm(), arg, dim);
                subset &= rel;
                superset |= rel & base_SUPERSET;
            }
            return (relation_t) (subset|superset);
        }
    }

    // Similar to >= but with raw_t*
    bool fed_t::isSupersetEq(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assert(dim && getDimension() == dim);
        assertx(dbm_isValid(arg, dim));

        if (isEmpty())
        {
            return false;
        }
        else
        {
            for(const_iterator iter = begin(); !iter.null(); ++iter)
            {
                if (dbm_isSupersetEq(iter->const_dbm(), arg, dim))
                {
                    return true;
                }
            }
            return false;
        }
    }

    // similar to <= dbm_t, exact relation.
    bool fed_t::le(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assert(dim && dim == getDimension());
        assertx(dbm_isValid(arg, dim));

        if (isEmpty())
        {
            return true;
        }
        else
        {
            for(const_iterator iter = begin(); !iter.null(); ++iter)
            {
                if (!dbm_isSubsetEq(iter->const_dbm(), arg, dim))
                {
                    return false;
                }
            }
            return true;
        }
    }

    // 1 - trivial cases like relation(),
    // 2 - try the "easy" relation(),
    // 3 - check for superset.
    relation_t fed_t::exactRelation(const fed_t& arg) const
    {
        assert(isOK());
        assert(arg.isOK());

        if (sameAs(arg))
        {
            return base_EQUAL;
        }
        else if (getDimension() != arg.getDimension())
        {
            return base_DIFFERENT;
        }
        else
        {
            uint32_t thisIncluded = isSubtractionEmpty(arg);
            uint32_t argIncluded  = arg.isSubtractionEmpty(*this);
            
            // Accumulate results
            assert(base_SUPERSET == 1 && base_SUBSET == 2);
            assert((thisIncluded | argIncluded | 1) == 1);
            
            return (relation_t) ((thisIncluded << 1) | argIncluded);
        }
    }

    relation_t fed_t::exactRelation(const dbm_t& arg) const
    {
        assert(isOK());

        if (getDimension() != arg.getDimension())
        {
            return base_DIFFERENT;
        }
        else
        {
            uint32_t thisIncluded = isSubtractionEmpty(arg);
            uint32_t argIncluded  = arg.isSubtractionEmpty(*this);
            
            // Accumulate results
            assert(base_SUPERSET == 1 && base_SUBSET == 2);
            assert((thisIncluded | argIncluded | 1) == 1);
            
            return (relation_t) ((thisIncluded << 1) | argIncluded);
        }
    }

    relation_t fed_t::exactRelation(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assertx(dbm_isValid(arg, dim));

        if (dim != getDimension())
        {
            return base_DIFFERENT;
        }
        else
        {
            uint32_t thisIncluded = isSubtractionEmpty(arg, dim);
            uint32_t argIncluded  = isSubtractionEmpty(arg, dim, *this);
            
            // Accumulate results
            assert(base_SUPERSET == 1 && base_SUBSET == 2);
            assert((thisIncluded | argIncluded | 1) == 1);
            
            return (relation_t) ((thisIncluded << 1) | argIncluded);
        }
    }
    
    fed_t& fed_t::setZero()
    {
        assert(isOK());

        if (size() != 1 || !const_dbmt().isZero()) // if != zero
        {
            *this = dbm_t(getDimension()).setZero();
        }
        return *this;
    }

    fed_t& fed_t::setInit()
    {
        assert(isOK());

        if (size() != 1 || !const_dbmt().isInit()) // if != init
        {
            *this = dbm_t(getDimension()).setInit();
        }
        return *this;
    }
    
    // first DBM = convex union of all subsequent DBMs
    fed_t& fed_t::convexHull()
    {
        assert(isOK());

        if (size() > 1)
        {
            iterator iter = beginMutable();
            dbm_t& first = *iter;
            for(++iter; !iter.null(); iter.remove())
            {
                first += *iter;
            }
        }
        return *this;
    }

    fed_t& fed_t::driftWiden()
    {
        assert(isOK());
        
        fed_t copy(getDimension());

        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            copy.add(*i);
            i->driftWiden();
        }
        
        return append(copy);
    }

    fed_t& fed_t::operator |= (const fed_t& arg)
    {
        assert(isOK());
        assert(arg.isOK());
        assert(arg.getDimension() == getDimension());

        if (sameAs(arg))
        {
            return *this;
        }
        else if (isEmpty())
        {
            *this = arg;
        }
        else if (!arg.isEmpty())
        {
            setMutable();
            cindex_t dim = getDimension();
            for (const_iterator iter = arg.begin(); !iter.null(); ++iter)
            {
                if (removeIncludedIn(iter->const_dbm(), dim))
                {
                    ifed()->insert(*iter);
                }
            }
        }
        return *this;
    }

    fed_t& fed_t::operator |= (const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (isEmpty())
        {
            *this = arg;
        }
        else if (!arg.isEmpty())
        {
            setMutable();
            if (removeIncludedIn(arg.const_dbm(), getDimension()))
            {
                ifed()->insert(arg);
            }
        }
        return *this;
    }

    fed_t& fed_t::operator |= (const raw_t* arg)
    {
        assert(isOK());
        assertx(dbm_isValid(arg, getDimension()));

        if (isEmpty())
        {
            *this = arg;
        }
        else
        {
            setMutable();
            cindex_t dim = getDimension();
            if (removeIncludedIn(arg, dim))
            {
                ifed()->insert(arg, dim);
            }
        }
        return *this;
    }

    fed_t& fed_t::unionWith(fed_t& arg)
    {
        assert(getDimension() == arg.getDimension());
        assert(isOK());
        assert(arg.isOK());
        
        if (!arg.isEmpty())
        {
            setMutable();
            arg.setMutable();
            ifed()->unionWith(*arg.ifed());
            arg.ifed()->reset();
        }
        return *this;
    }

    fed_t& fed_t::append(fed_t& arg)
    {
        assert(getDimension() == arg.getDimension());
        assert(isOK());
        assert(arg.isOK());

        if (!arg.isEmpty())
        {
            setMutable();
            arg.setMutable();
            ifed()->append(*arg.ifed());
            arg.ifed()->reset();
        }
        return *this;
    }

    fed_t& fed_t::appendBegin(fed_t& arg)
    {
        assert(getDimension() == arg.getDimension());
        assert(isOK());
        assert(arg.isOK());

        if (!arg.isEmpty())
        {
            setMutable();
            arg.setMutable();
            ifed()->appendBegin(*arg.ifed());
            arg.ifed()->reset();
        }
        return *this;
    }

    fed_t& fed_t::appendEnd(fed_t& arg)
    {
        assert(getDimension() == arg.getDimension());
        assert(isOK());
        assert(arg.isOK());

        if (!arg.isEmpty())
        {
            setMutable();
            arg.setMutable();
            ifed()->appendEnd(*arg.ifed());
            arg.ifed()->reset();
        }
        return *this;
    }

    void fed_t::append(fdbm_t* arg)
    {
        assert(isOK() && arg);
        assert(arg->const_dbmt().getDimension() == getDimension());
        assert(!arg->const_dbmt().isEmpty());

        setMutable();
        ifedPtr->append(arg);
    }

    fed_t& fed_t::operator += (const fed_t& arg)
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());

        convexHull();
        if (!arg.isEmpty())
        {
            // Compute convex hull of the argument.
            // We could use += fed_t(arg).convexHull() but
            // we don't need the copy.

            dbm_t hull = arg.const_dbmt();
            for (const_iterator i = ++arg.begin(); !i.null(); ++i)
            {
                hull += *i;
            }
            if (isEmpty())
            {
                *this = hull;
            }
            else
            {
                setMutable();
                dbmt() += hull;
            }
        }
        return *this;
    }

    fed_t& fed_t::operator += (const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        convexHull();
        if (!arg.isEmpty())
        {
            if (isEmpty())
            {
                *this = arg;
            }
            else
            {
                setMutable();
                dbmt() += arg;
            }
        }
        return *this;
    }

    fed_t& fed_t::operator += (const raw_t* arg)
    {
        assert(isOK());
        assertx(dbm_isValid(arg, getDimension()));

        convexHull();
        if (isEmpty())
        {
            *this = arg;
        }
        else
        {
            setMutable();
            dbmt() += arg;
        }
        return *this;
    }

    fed_t& fed_t::operator &= (const fed_t& arg)
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());
        
        if (sameAs(arg))
        {
            return *this;
        }
        else if (arg.isEmpty()) // then intersection is empty
        {
            setEmpty();
        }
        else if (!isEmpty())
        {
            setMutable();
            dbmlist_t result; // empty to start with
            const_iterator i = arg.begin();
            const dbm_t& arg1 = *i; // treat 1st DBM last to avoid a copy
            cindex_t dim = getDimension();
            
            // compute result = union_i(this & arg[i]) with arg[i] = ith DBM of arg.
            size_t s;
            for(++i; !i.null(); ++i)
            {
                //result.unionWith(ifed()->copyList().intersection(*i, dim));
                s = result.size();
                result.appendEnd(ifed()->copyList().intersection(*i, dim)).mergeReduce(dim, s);
            }
            //ifed()->intersection(arg1, dim).unionWith(result);
            s = result.size();
            ifed()->intersection(arg1, dim).appendBegin(result).mergeReduce(dim, s);
        }
        return *this;
    }

    // get rid of trivial cases and call back intersection
    fed_t& fed_t::operator &= (const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (arg.isEmpty())
        {
            setEmpty();
        }
        else if (!isEmpty())
        {
            setMutable();
            ifed()->intersection(arg, getDimension());
            mergeReduce();
        }
        return *this;
    }

    fed_t& fed_t::operator &= (const raw_t* arg)
    {
        assert(isOK());
        assertx(dbm_isValid(arg, getDimension()));

        if (!isEmpty())
        {
            setMutable();
            ifed()->intersection(arg, getDimension());
            mergeReduce();
        }
        return *this;
    }

    fed_t& fed_t::operator -= (const fed_t& arg)
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());

        if (sameAs(arg))
        {
            setEmpty();
        }
        else if (arg.size() == 1)
        {
            return *this -= arg.const_dbmt();
        }
        else if (!isEmpty() && !arg.isEmpty())
        {
            setMutable();
#ifndef ENABLE_STORE_MINGRAPH
            cindex_t dim = getDimension();
#endif
            for(const_iterator i = arg.begin(); !i.null(); ++i)
            {
#ifdef ENABLE_STORE_MINGRAPH
                ptr_subtract(*i);
#else
                ptr_subtract(i->const_dbm(), dim);
#endif
                if (isEmpty())
                {
                    break;
                }
            }
        }
        return *this;
    }

    fed_t& fed_t::operator -= (const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (!(isEmpty() || arg.isEmpty()))
        {
            RECORD_STAT();
            RECORD_SUBSTAT(getDimension() == 1
                           ? "dim==1"
                           : (*this <= arg
                              ? "*this<=arg"
                              : "non-trivial"));

            if (getDimension() == 1)
            {
                setEmpty();
            }
            else
            {
                setMutable();
#ifdef ENABLE_STORE_MINGRAPH
                ptr_subtract(arg);
#else
                ptr_subtract(arg.const_dbm(), getDimension());
#endif
            }
        }
        return *this;
    }

    fed_t& fed_t::operator -= (const raw_t* arg)
    {
        assert(isOK());
        assertx(dbm_isValid(arg, getDimension()));

        cindex_t dim = getDimension();
        if (!isEmpty())
        {
            RECORD_STAT();
            RECORD_SUBSTAT(getDimension() == 1
                           ? "dim==1"
                           : (*this <= arg
                              ? "*this<=arg"
                              : "non-trivial"));

            if (dim == 1)
            {
                setEmpty();
            }
            else
            {
                setMutable();
                ptr_subtract(arg, dim);
            }
        }
        return *this;
    }
    
    fed_t& fed_t::subtractDown(const fed_t& arg)
    {
        assert(isOK());
        
        if (isEmpty())
        {
            return *this;
        }

        if (!arg.isEmpty())
        {
            setMutable();
            cindex_t dim = getDimension();
            for(const_iterator i = arg.begin(); !i.null(); ++i)
            {
                if (!canSkipSubtract(i->const_dbm(), dim))
                {
#if ENABLE_STORE_MINGRAPH
                    ptr_subtract(*i);
#else
                    ptr_subtract(i->const_dbm(), dim);
#endif
                    if (isEmpty())
                    {
                        return *this;
                    }
                }
            }
        }
        return down();
    }

    fed_t& fed_t::subtractDown(const dbm_t& arg)
    {
        assert(isOK());

        if (isEmpty())
        {
            return *this;
        }
     
        if (!arg.isEmpty())
        {
            cindex_t dim = getDimension();
            if (!canSkipSubtract(arg.const_dbm(), dim))
            {
                setMutable();
#ifdef ENABLE_STORE_MINGRAPH
                ptr_subtract(arg);
#else
                ptr_subtract(arg.const_dbm(), dim);
#endif
            }
        }
        return down();
    }

    fed_t& fed_t::subtractDown(const raw_t* arg)
    {
        assert(isOK());
        assertx(dbm_isValid(arg, getDimension()));

        if (isEmpty())
        {
            return *this;
        }
        cindex_t dim = getDimension();
        if (!canSkipSubtract(arg, dim))
        {
            setMutable();
            ptr_subtract(arg, dim);
        }
        return down();
    }

    bool fed_t::constrain(cindex_t i, int32_t value)
    {
        assert(isOK());
        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(i, value))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }

    bool fed_t::constrain(cindex_t i, cindex_t j, raw_t c)
    {
        assert(isOK());
        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(i, j, c))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }

    bool fed_t::constrain(const constraint_t *c, size_t n)
    {
        assert(isOK());

        // Very common (and simpler) case:
        if (n == 1)
        {
            assert(c);
            return constrain(c->i, c->j, c->value);
        }

        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(c, n))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }

    bool fed_t::constrain(const cindex_t *table, const constraint_t *c, size_t n)
    {
        assert(isOK());
        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(table, c, n))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }

    bool fed_t::constrain(const cindex_t *table, const base::pointer_t<constraint_t>& vec)
    {
        assert(isOK());
        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(table, vec.begin(), vec.size()))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }

    bool fed_t::constrain(const cindex_t *table, const std::vector<constraint_t>& vec)
    {
        assert(isOK());
        for(iterator it = beginMutable(); !it.null(); )
        {
            if (it->ptr_constrain(table, &vec[0], vec.size()))
            {
                ++it;
            }
            else
            {
                it.removeEmpty();
            }
        }
        return !isEmpty();
    }
    
    bool fed_t::intersects(const fed_t& arg) const
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());

        if (sameAs(arg))
        {
            return true;
        }
        else if (arg.isEmpty())
        {
            return false;
        }
        else
        {
            size_t argSize = arg.size();
            const raw_t* argDBM[argSize];
            arg.toArray(argDBM);
            
            // check cross-intersections
            cindex_t dim = getDimension();
            for(const_iterator i = begin(); !i.null(); ++i)
            {
                uint32_t j = 0;
                do {
                    if (dbm_haveIntersection(i->const_dbm(), argDBM[j], dim))
                    {
                        return true;
                    }
                } while(++j < argSize);
            }
            return false;
        }
    }

    bool fed_t::intersects(const dbm_t& arg) const
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());

        if (arg.isEmpty())
        {
            return false;
        }
        else
        {
            return intersects(arg.const_dbm(), getDimension());
        }
    }

    bool fed_t::intersects(const raw_t *arg, cindex_t dim) const
    {
        assert(isOK());
        assert(dim == getDimension());
        assertx(dbm_isValid(arg, dim));

        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (dbm_haveIntersection(i->const_dbm(), arg, dim))
            {
                return true;
            }
        }
        return false;
    }

    fed_t& fed_t::up()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_up();
        }
        return *this;
    }

    fed_t& fed_t::upStop(const uint32_t *stopped)
    {
        assert(isOK());

        if (!stopped)
        {
            return up();
        }

        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_upStop(stopped);
        }
        return *this;
    }

    fed_t& fed_t::downStop(const uint32_t *stopped)
    {
        assert(isOK());

        if (!stopped)
        {
            return down();
        }

        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_downStop(stopped);
        }
        return *this;
    }

    fed_t& fed_t::down()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_down();
        }
        return *this;
    }

    fed_t& fed_t::freeClock(cindex_t clock)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_freeClock(clock);
        }
        return *this;
    }

    fed_t& fed_t::freeUp(cindex_t clock)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_freeUp(clock);
        }
        return *this;
    }

    fed_t& fed_t::freeDown(cindex_t clock)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_freeDown(clock);
        }
        return *this;
    }

    fed_t& fed_t::freeAllUp()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_freeAllUp();
        }
        return *this;
    }

    fed_t& fed_t::freeAllDown()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_freeAllDown();
        }
        return *this;
    }

    void fed_t::updateValue(cindex_t x, int32_t v)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_updateValue(x, v);
        }
    }

    void fed_t::updateClock(cindex_t x, cindex_t y)
    {
        assert(isOK());
        if (x == y) return;
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_updateClock(x, y);
        }
    }

    void fed_t::updateIncrement(cindex_t x, int32_t v)
    {
        assert(isOK());
        if (v == 0) return;
        cindex_t dim = getDimension();
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            dbm_updateIncrement(i->getCopy(), dim, x, v);
        }
    }

    void fed_t::update(cindex_t x, cindex_t y, int32_t v)
    {
        assert(isOK());
        if (v == 0)
        {
            updateClock(x, y);
        }
        else if (x == y)
        {
            updateIncrement(x, v);
        }
        else
        {
            for(iterator i = beginMutable(); !i.null(); ++i)
            {
                i->ptr_update(x, y , v);
            }
        }
    }

    bool fed_t::satisfies(cindex_t i, cindex_t j, raw_t c) const
    {
        assert(isOK());
        assert(i != j && i < getDimension() && j < getDimension());

        for(const_iterator it = begin(); !it.null(); ++it)
        {
            if (it->satisfies(i, j, c))
            {
                return true;
            }
        }
        return false;
    }

    bool fed_t::isUnbounded() const
    {
        assert(isOK());
        cindex_t dim = getDimension();
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (dbm_isUnbounded(i->const_dbm(), dim))
            {
                return true;
            }
        }
        return false;
    }

    fed_t fed_t::getUnbounded() const
    {
        fed_t result(getDimension());
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (i->isUnbounded())
            {
                result |= *i;
            }
        }
        return result;
    }

    fed_t fed_t::getBounded() const
    {
        fed_t result(getDimension());
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (!i->isUnbounded())
            {
                result |= *i;
            }
        }
        return result;
    }

    fed_t& fed_t::relaxUpClock(cindex_t clock)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_relaxUpClock(clock);
        }
        return *this;
    }

    fed_t& fed_t::relaxDownClock(cindex_t clock)
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_relaxDownClock(clock);
        }
        return *this;
    }

    fed_t& fed_t::relaxAll()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_relaxAll();
        }
        return *this;
    }

    fed_t& fed_t::tightenDown()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); )
        {
            i->ptr_tightenDown();
            if (i->isEmpty())
            {
                i.removeEmpty();
            }
            else
            {
                ++i;
            }
        }
        return *this;
    }

    fed_t& fed_t::tightenUp()
    {
        assert(isOK());
        for(iterator i = beginMutable(); !i.null(); )
        {
            i->ptr_tightenUp();
            if (i->isEmpty())
            {
                i.removeEmpty();
            }
            else
            {
                ++i;
            }
        }
        return *this;
    }

    // Keep DBMs that are not included in others
    // and remove DBMs that are included.
    fed_t& fed_t::expensiveReduce()
    {
        assert(isOK());

        // at least 2 DBMs
        if (size() > 1)
        {
            // side effect on all copies
            fdbm_t **fdbm = ifed()->atHead();
            do {
                assert(size() > 1);

                // remove fdbm from the list
                fdbm_t *current = *fdbm;
                *fdbm = current->getNext();
                ifedPtr->decSize();

                // this - current = empty <=> this <= current
                if (isSubtractionEmpty(current->const_dbmt()))
                {
                    // this = current, finished!
                    ifedPtr->setDBM(current);
                    break;
                }
                else if (current->const_dbmt().isSubtractionEmpty(*this))
                {
                    // current - this = empty <=> current <= this
                    // deallocate current
                    current->dbmt().nil();
                    current->remove();
                    if (size() <= 1) break;
                }
                else
                {
                    *fdbm = current;                  // put it back
                    ifedPtr->incSize();
                    fdbm = current->getNextMutable(); // and continue
                }
            } while(*fdbm);
        }
        return *this;
    }

    fed_t& fed_t::convexReduce()
    {
        assert(isOK());
        // at least 2 DBMs
        if (size() > 1)
        {
            DODEBUGX(fed_t checkFed = *this);
            DODEBUGX(checkFed.setMutable());
            fdbm_t **head = ifed()->atHead(); // side effect on all copies
            cindex_t dim = getDimension();
            CERR("["<<size()<<":");
            for(fdbm_t **fi = head; *fi != NULL; )
            {
            next_fi:
                fed_t removedj(dim);
            new_convexi:
                assert(*fi);
                const dbm_t& dbmi = (*fi)->const_dbmt();
            compute_convexi:
                dbm_t convexi = dbmi;
                const raw_t *dbm1 = dbmi.const_dbm();
                for(fdbm_t **fj = (*fi)->getNextMutable(); *fj != NULL; )
                {
                    const dbm_t& dbmj = (*fj)->const_dbmt();
                    const raw_t *dbm2 = dbmj.const_dbm();
                    int subset = 1, superset = 1;
                    int compatible = (dim <= 2);
                    int symCompatible = compatible;
                    for(cindex_t i = 1; i < dim; ++i)
                    {
                        for(cindex_t j = 0; j < i; ++j)
                        {
                            uint32_t ij = i*dim+j;
                            uint32_t ji = j*dim+i;
                            
#ifndef IMPROVED_MERGE
                            // No intersection for sure
                            if ((dbm1[ij] != dbm_LS_INFINITY &&
                                 dbm2[ji] != dbm_LS_INFINITY &&
                                 dbm_negRaw(dbm_weakRaw(dbm1[ij])) >= dbm_weakRaw(dbm2[ji]))
                                ||
                                (dbm1[ji] != dbm_LS_INFINITY &&
                                 dbm2[ij] != dbm_LS_INFINITY &&
                                 dbm_negRaw(dbm_weakRaw(dbm1[ji])) >= dbm_weakRaw(dbm2[ij])))
                            {
                                goto DifferentDBMs;
                            }
#else
                            // Will try only weakly non disjoint DBMs only.
                            // No intersection for sure
                            if (fed_checkWeakAdd(dbm1[ij], dbm2[ji]) ||
                                fed_checkWeakAdd(dbm1[ji], dbm2[ij]))
                            {
                                goto DifferentDBMs;
                            }
#endif
                            // Check DBMs, no jump everything goes in the pipeline.
                            int comp1 = dbm1[ij] == dbm2[ij];
                            compatible |= comp1;
                            subset &= dbm1[ij] <= dbm2[ij];
                            superset &= dbm1[ij] >= dbm2[ij];
                            int comp2 = dbm1[ji] == dbm2[ji];
                            compatible |= comp2;
                            symCompatible |= comp1 & comp2;
                            subset &= dbm1[ji] <= dbm2[ji];
                            superset &= dbm1[ji] >= dbm2[ji];
                        }
                    }
                    if (subset) // fi <= fj -> remove fi, put back removedj, reloop
                    {
                        CERR(GREEN(THIN)"X"NORMAL);
                        *fi = (*fi)->removeAndNext();
                        ifedPtr->decSize();
                        ifedPtr->stealFromToEnd(fi, *removedj.ifedPtr);
                        goto new_convexi;
                    }
                    else if (superset) // fi >= fj -> remove fj, continue
                    {
                        CERR(GREEN(THIN)"x"NORMAL);
                        *fj = (*fj)->removeAndNext();
                        ifedPtr->decSize();
                    }
                    else if (symCompatible)
                    {
                        // try merge 2 by 2
                        dbm_t tryMerge = dbmi;
                        tryMerge += dbmj;
                        if (((fed_t(tryMerge) -= dbmi) -= dbmj).isEmpty())
                        {
                            // if success, update fi, remove fj, reloop
                            CERR(MAGENTA(THIN)"m"NORMAL);
                            (*fi)->dbmt().updateCopy(tryMerge);
                            *fj = (*fj)->removeAndNext();
                            ifedPtr->decSize();
                            // put back removedj since convexi will be recomputed
                            ifedPtr->stealFromToEnd(fi, *removedj.ifedPtr);
                            goto compute_convexi;
                        }
                        else
                        {
                            goto OnlyCompatible;
                        }
                    }
                    else if (compatible)
                    {
                    OnlyCompatible:
                        CERR(YELLOW(THIN)"+"NORMAL);
                        convexi += dbmj;
                        removedj.ifedPtr->steal(fj, *ifedPtr);
                    }
                    else
                    {
                    DifferentDBMs:
                        // ignore fj
                        fj = (*fj)->getNextMutable();
                    }
                } // for(fj..)
                                
                if (removedj.size() > 0) // otherwise convexi == dbmi
                {
                    // 2nd pass to compare only with convexi
                    for(fdbm_t** fj = (*fi)->getNextMutable(); *fj != NULL; )
                    {
                        const dbm_t& dbmj = (*fj)->const_dbmt();
                        //if (dbmj <= convexi)
                        if (dbm_isSubsetEq(dbmj.const_dbm(), convexi.const_dbm(), dim))
                        {
                            CERR(YELLOW(BOLD)"+"NORMAL);
                            removedj.ifedPtr->steal(fj, *ifedPtr);
                        }
                        else
                        {
                            fj = (*fj)->getNextMutable();
                        }
                    }
                    // Check the convex hull.
                    fed_t tooMuch = convexi;
                    (tooMuch -= dbmi) -= removedj.mergeReduce();
                    if (tooMuch.isEmpty())
                    {
                        CERR(CYAN(BOLD)"C("<<removedj.size()<<")"NORMAL);
                        // Replace fi by convexi
                        (*fi)->dbmt().updateCopy(convexi);
                        if (size() == 1) break;
                        goto next_fi;
                    }
                    else
                    {
                        // Note that convexi == convexHull(removedj)+dbmi
                        fed_t newFed = convexi;
                        convexi.nil();
                        (newFed -= tooMuch.mergeReduce()).mergeReduce();
                        assert(dbmi.le(newFed));
                        if (newFed.size() <= removedj.size()) // <= because dbmi not in removedj
                        {
                            CERR(GREEN(BOLD)"R("<<(1+removedj.size()-newFed.size())<<")"NORMAL);
                            *fi = (*fi)->removeAndNext(); // remove fi since inside newFed
                            ifedPtr->decSize();
                            ifedPtr->stealFromToEnd(fi, *newFed.ifedPtr);
                            goto next_fi;
                        }
                        assert(removedj.isMutable());
#if 1
                        // Try expensiveReduce on removedj
                        if (removedj.add(dbmi).expensiveReduce().ifedPtr->const_head()->const_dbmt().sameAs(dbmi))
                        {
                            CERR(RED(THIN)"B("<<(newFed.size()-1-removedj.size())<<")"NORMAL);
                            // dbmi still there, but in fi too
                            removedj.ifedPtr->removeHead();
                            fi = (*fi)->getNextMutable();
                        }
                        else
                        {
                            CERR(MAGENTA(THIN)"R"NORMAL);
                            // dbmi was reduced, don't need in fi
                            *fi = (*fi)->removeAndNext();
                            ifedPtr->decSize();
                        }
                        // Put back removedj to end (don't mess-up loop on fi).
                        ifedPtr->stealFromToEnd(fi, *removedj.ifedPtr);
                        goto next_fi;
                        
#else
                        CERR(RED(THIN)"B("<<(newFed.size()-1-removedj.size())<<")"NORMAL);
                        // Put back removedj to end (don't mess-up loop on fi).
                        ifedPtr->stealFromToEnd(fi, *removedj.ifedPtr);
#endif
                    }
                }
                fi = (*fi)->getNextMutable();
            } // for(fi..)
            CERR(":"<<size()<<"]");
            assertx(eq(checkFed));
        }
        return *this;
    }
    
    fed_t& fed_t::expensiveConvexReduce()
    {
        assert(isOK());
        
        if (size() > 1)
        {
            const_iterator i = begin();
            dbm_t c = *i;
            for(++i; !i.null(); ++i)
            {
                c += *i;
            }
            fed_t newFed(c);
            fed_t excess = fed_t(c) -= *this;
            c.nil();
            
            // Simpler to take all-excess with excess=all-this
            // but it is not always better.
            //fed_t newFed(getDimension());
            //newFed.setInit();
            //fed_t excess = fed_t(newFed) -= *this;

            if (excess.size() > 5*size() || // heuristic to abort before mergeReduce
                excess.mergeReduce().size() > size())
            {
                return *this; // Abort.
            }

            newFed -= excess;
            if (newFed.size() < 3*(excess.size() + size()) && // another heuristic :)
                newFed.mergeReduce().size() < size())
            {
                CERR(GREEN(BOLD)"["<<(size()-newFed.size())<<"]"NORMAL);
                ifedPtr->swap(*newFed.ifedPtr); // win
            }
            else // lose
            {
                CERR(RED(BOLD)"["<<(newFed.size()-size())<<"]"NORMAL);
            }
        }
        return *this;
    }

    fed_t& fed_t::partitionReduce()
    {
        assert(isOK());
        if (size() == 2)
        {
            return convexReduce();
        }
        else if (size() > 2)
        {
            cindex_t dim = getDimension();
            fed_t reducedFed(dim), partition(dim);
            dbm_t tmp(dim);
            tmp.getDBM(); // allocate a DBM
            CERR("<"<<size()<<":");

            // Loop on all DBMs D of this federation:
            //  Init partition with D, remove D from this federation
            //  Loop on all DBMs i of partition:
            //   Loop on all DBMs j of this federation
            //    if i&j != empty (weak constraints), move j to end of partition

            for(fdbm_t **head = ifed()->atHead(); *head != NULL; )
            {
                // Partition initialized with D
                partition.ifedPtr->steal(head, *ifedPtr);

                // Loop on DBMs i of partition
                fdbm_t *fi, **endi = partition.ifedPtr->atHead();
                for(fi = *endi, endi = (*endi)->getNextMutable() ; fi != NULL; fi = fi->getNext())
                {
                    const raw_t *dbmi = fi->const_dbmt().const_dbm();
                    assert(*endi == NULL); // end of partition
                    // Loop on DBMs j of this federation
                    for(fdbm_t **fj = head; *fj != NULL; )
                    {
                        if (dbm_relaxedIntersection(tmp.dbm(), dbmi, (*fj)->const_dbmt().const_dbm(), dim))
                        {
                            // move fj to end of partition, update end
                            endi = partition.ifedPtr->steal(endi, fj, *ifedPtr);
                        }
                        else
                        {
                            fj = (*fj)->getNextMutable();
                        }
                    }
                }
                // Reduction!
                //CERR(MAGENTA(THIN)<<partition.size()<<":"NORMAL);
#ifdef PARTITION_FIXPOINT
                for(size_t s1 = partition.convexReduce().size(); s1 > 2; )
                {
                    size_t s2 = partition.expensiveConvexReduce().size();
                    if (s2 <= 2 || s2 >= s1) break;
                    s1 = s2;
                    s2 = partition.convexReduce().size();
                    if (s2 >= s1) break; // not better
                    s1 = s2;
                }
#else
                partition.convexReduce().expensiveConvexReduce();
#endif
                reducedFed.append(partition);

                assert(partition.isEmpty());
            }
            // Transfer result to this federation
            assert(isEmpty());
            ifedPtr->copyRef(*reducedFed.ifedPtr);
            reducedFed.ifedPtr->reset();

            CERR(":"<<size()<<">");
        }
        return *this;
    }

    bool fed_t::contains(const int32_t* point, cindex_t dim) const
    {
        assert(isOK());
        assert(dim == getDimension());
        
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (dbm_isPointIncluded(point, i->const_dbm(), dim))
            {
                return true;
            }
        }
        return false;
    }

    bool fed_t::contains(const double* point, cindex_t dim) const
    {
        assert(isOK());
        assert(dim == getDimension());

        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (dbm_isRealPointIncluded(point, i->const_dbm(), dim))
            {
                return true;
            }
        }
        return false;
    }
    
    static inline
    double fed_diff(double value, raw_t low)
    {
        return value - -dbm_raw2bound(low) - (dbm_rawIsStrict(low) ? 0.5 : 0.0);
    }

    double fed_t::possibleBackDelay(const double* point, cindex_t dim) const
    {
        assert(isOK());
        assert(dim == getDimension());
        assert(contains(point, dim));
        
        double totalDelay = 0.0;
        if (dim > 1)
        {
            const raw_t *dbm1 = NULL;
            // First find a DBM containing this point
            for(const_iterator i = begin(); !i.null(); ++i)
            {
                if (i->contains(point, dim))
                {
                    dbm1 = i->const_dbm();
                    break;
                }
            }
            if (dbm1)
            {
            retry:
                // Possible backward delay of point in dbm1
                double delay = fed_diff(point[1] - totalDelay, dbm1[1]);
                for(cindex_t j = 2; j < dim; ++j)
                {
                    delay = std::min(delay, fed_diff(point[j] - totalDelay, dbm1[j]));
                }
                if (totalDelay < delay)
                {
                    totalDelay = delay;

                    // Find dbm2 to continue the delay and retry with a new dbm1
                    const raw_t *dbm2 = NULL;
                    for(const_iterator i = begin(); !i.null(); ++i)
                    {
                        dbm2 = i->const_dbm();
                        if (dbm1 != dbm2)
                        {
                            // Check if we can continue with the delay through
                            // another DBM: forall i, -dbm1.lower[i] <= dbm2.upper[i]
                            cindex_t j;
                            for(j = 2; j < dim; ++j)
                            {
                                if (dbm_negRaw(dbm1[j]) > dbm2[j*dim] || // DBM 'continuous'
                                    point[j] > (dbm_raw2bound(dbm2[j*dim]) + 0.5))
                                {
                                    break; // cannot
                                }
                            }
                            if (j == dim) // all lower1 <= upper2
                            {
                                dbm1 = dbm2;        // continue on dbm2
                                goto retry;
                            }
                        }
                    }
                }
            }
        }
        return totalDelay;
    }

    bool fed_t::getMinDelay(const double* point, cindex_t dim, double* t,
                            double *minVal, bool *isStrict, const uint32_t *stopped) const
    {
        assert(isOK());
        assert(dim == getDimension());

        // Special case.
        if (dim == 1 && !isEmpty())
        {
            *t = 0.0;
            if (minVal && isStrict)
            {
                *minVal = 0;
                *isStrict = false;
            }
            return true;
        }

        *t = HUGE_VAL;
        if (minVal && isStrict)
        {
            *minVal = HUGE_VAL;
            *isStrict = true;
        }
        double di = HUGE_VAL;
        double bound;
        bool strict;
        double *boundPtr = minVal ? &bound : NULL;
        bool *strictPtr = isStrict ? &strict : NULL;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            i->getMinDelay(point, dim, &di, boundPtr, strictPtr, stopped);
            if (di < *t)
            {
                *t = di;
                if (minVal && isStrict)
                {
                    *minVal = bound;
                    *isStrict = strict;
                }
            }
        }
        return (*t < HUGE_VAL);
    }

    bool fed_t::getMaxBackDelay(const double* point, cindex_t dim, double* t, double max) const
    {
        assert(isOK());
        assert(dim == getDimension());

        *t = 0.0;
        double di = 0.0;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            i->getMaxBackDelay(point, dim, &di, max);
            *t = std::max(*t, di);
        }
        return (*t > 0.0);
    }

    bool fed_t::isConstrainedBy(cindex_t i, cindex_t j, raw_t c) const
    {
        for(const_iterator k = begin(); !k.null(); ++k)
        {
            if (k->isConstrainedBy(i,j,c))
            {
                return true;
            }
        }
        return false;
    }

    bool fed_t::getDelay(const double* point, cindex_t dim, double* min, double* max,
                         double *minVal, bool *minStrict, double *maxVal, bool *maxStrict,
                         const uint32_t* stopped) const
    {
        assert(isOK());
        assert(dim == getDimension());

        // Get min delay first.
        if (!getMinDelay(point, dim, min, minVal, minStrict, stopped))
        {
            *max = 0.0;
            if (maxVal && maxStrict)
            {
                *maxVal = 0.0;
                *maxStrict = false;
            }
            return false;
        }

        *max = *min;
        assert(*min >= 0.0);
        if (maxVal && maxStrict)
        {
            *maxVal = *max;
            *maxStrict = false;
        }

        bool interval = maxVal && maxStrict && minVal && minStrict;
        if (interval)
        {
            if (*minStrict)
            {
                *maxVal = base_addEpsilon(*minVal, base_EPSILON);
                *maxStrict = true;
            }
            else
            {
                *maxVal = *minVal;
                *maxStrict = false;
            }
        }

        double pt[dim];
        pt[0] = point[0];
        double currentDelay = *min;
        double waitMaxValue = minVal && minStrict ? *minVal : *min;
        bool waitMaxStrict = false;
        // If we need to wait at least >min or >=min, then at most <=max to start with.

        const raw_t* dbm1 = NULL;
    retry:
        for(const_iterator k = begin(); !k.null(); ++k)
        {
            for(cindex_t i = 1; i < dim; ++i)
            {
                pt[i] = stopped && base_readOneBit(stopped, i)
                    ? point[i]
                    : point[i] + currentDelay;
            }
            const raw_t* dbm = k->const_dbm();
            if (dbm != dbm1 && dbm_isRealPointIncluded(pt, dbm, dim))
            {
                // Then try to delay and stay inside k.
                double d = HUGE_VAL;
                double value = d;
                bool isStrict = true;
                for(cindex_t i = 1; i < dim; ++i)
                {
                    if (dbm[i*dim] < dbm_LS_INFINITY)
                    {
                        double di     = (double)dbm_raw2bound(dbm[i*dim]) - (pt[i] - pt[0]);
                        double valuei = (double)dbm_raw2bound(dbm[i*dim]) - (point[i] - point[0]);
                        bool isStricti =  dbm_rawIsStrict(dbm[i*dim]);
                        assert(di >= 0.0);
                        if (isStricti)
                        {
                            double di0 = di;
                            double subEpsilon = base_EPSILON;
                            do {
                                di = base_subtractEpsilon(di0, subEpsilon);
                                if (di >= 0.0)
                                {
                                    break;
                                }
                                subEpsilon *= 0.1;
                            } while(subEpsilon >= DBL_EPSILON);
                            // If we couldn't get a proper max now, anything we try will be wrong.
                        }
                        if (di < d)
                        {
                            d = di;
                            value = valuei;
                            isStrict = isStricti;
                        }
                    }
                }
                if (d == HUGE_VAL)
                {
                    *max = d;
                    if (interval)
                    {
                        *maxVal = d;
                        *maxStrict = true;
                    }
                    break;
                }
                double newMax = currentDelay+d;
                if (newMax > *max)
                {
                    dbm1 = dbm;
                    *max = newMax;
                    if (interval && value >= waitMaxValue)
                    {
                        *maxVal = waitMaxValue = value;
                        *maxStrict = waitMaxStrict = isStrict;
                    }
                    currentDelay = newMax;
                    goto retry;
                }
            }
        }
        
        return true;
    }

    void fed_t::extrapolateMaxBounds(const int32_t *max)
    {
        assert(isOK());
        cindex_t dim = getDimension();
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            dbm_extrapolateMaxBounds(i->getCopy(), dim, max);
        }
    }

    void fed_t::diagonalExtrapolateMaxBounds(const int32_t *max)
    {
        assert(isOK());
        cindex_t dim = getDimension();
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            dbm_diagonalExtrapolateMaxBounds(i->getCopy(), dim, max);
        }
    }

    void fed_t::extrapolateLUBounds(const int32_t *lower, const int32_t *upper)
    {
        assert(isOK());
        cindex_t dim = getDimension();
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            dbm_extrapolateLUBounds(i->getCopy(), dim, lower, upper);
        }
    }

    void fed_t::diagonalExtrapolateLUBounds(const int32_t *lower, const int32_t *upper)
    {
        assert(isOK());
        cindex_t dim = getDimension();
        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            dbm_diagonalExtrapolateLUBounds(i->getCopy(), dim, lower, upper);
        }
    }

    // Helper function for std::transform.
    static inline
    constraint_t sat_collect(const fdbm_t *f, const constraint_t& c)
    {
        return (f->const_dbmt().satisfies(c) ? c : !c);
    }

    // Helper function for std::for_each.
    static inline
    void sat_assert(fdbm_t *f, const constraint_t& c)
    {
        DODEBUG(bool check =) f->dbmt().constrain(c);
        assert(check);
    }

    void fed_t::splitExtrapolate(const constraint_t *begin, const constraint_t *end,
                                 const int32_t *max)
    {
        assert(isOK());
        
        // Don't bother trying to avoid copying with this extrapolation.
        setMutable();

        // 1) Split for every diagonal constraint.
        
        for(const constraint_t *c = begin; c != end; c++)
        {
            fdbm_t **next;
            for(fdbm_t **fi = ifed()->atHead(); *fi != NULL; fi = next)
            {
                next = (*fi)->getNextMutable();
            
                // If the constraint splits the DBM.
                if ((*fi)->const_dbmt().satisfies(c->i, c->j, c->value) &&
                    (*fi)->const_dbmt().satisfies(c->j, c->i, dbm_negRaw(c->value)))
                {
                    // Split *fi with c and not(c).
                    fdbm_t *copy = (*fi)->create((*fi)->const_dbmt(), *next);
                    DODEBUG(bool check =) copy->dbmt().constrain(c->i, c->j, c->value);
                    assert(check);
                    DODEBUG(check =) (*fi)->dbmt().constrain(c->j, c->i, dbm_negRaw(c->value));
                    assert(check);
                    // Insert new DBM and skip it for next iteration.
                    *next = copy;
                    next = copy->getNextMutable();
                    ifed()->incSize();
                }
            }
        }

        // 2) For every DBM:
        //    a) Collect relevant diagonal constraints.
        //    b) Apply extrapolation.
        //    c) Re-constrain to ensure that diagonal constraints are not crossed.
        
        for(fdbm_t *fi = ifed()->head(); fi != NULL; fi = fi->getNext())
        {
            // a)
            std::slist<constraint_t> sat;
            transform(begin, end, front_inserter(sat), bind(sat_collect, fi, _1));
            // b)
            fi->dbmt().extrapolateMaxBounds(max);
            // c)
            std::for_each(sat.begin(), sat.end(), bind(sat_assert, fi, _1));        
        }
        
        // 3) Try to merge back if possible.
        mergeReduce();
    }

    // similar to dbm_t::resize with the difference that we compute the index
    // table only once and we update the DBMs with precomputed data (only once too).
    void fed_t::resize(const uint32_t *bitSrc, const uint32_t *bitDst,
                       size_t bitSize, cindex_t *table)
    {
        assert(isOK());
        assert(bitSrc && bitDst && table);
        assert(*bitSrc & *bitDst & 1); // ref clock in both

        if (!base_areBitsEqual(bitSrc, bitDst, bitSize)) // pre-condition
        {
            cindex_t newDim = base_countBitsN(bitDst, bitSize);
            if (isEmpty())
            {
                base_bits2indexTable(bitDst, bitSize, table);
                setDimension(newDim);
            }
            else if (newDim <= 1)
            {
                *this = dbm_allocator.dbm1();
                table[0] = 0;
            }
            else
            {
                cindex_t cols[newDim];
                cindex_t oldDim = getDimension();
                dbm_computeTables(bitSrc, bitDst, bitSize, table, cols);
                for(iterator i = beginMutable(); !i.null(); ++i)
                {
                    dbm_t old = *i;                // *i is not mutable
                    dbm_updateDBM(i->inew(newDim), // can call inew
                                  old.const_dbm(),
                                  newDim, oldDim, cols);
                }
                ifed()->updateDimension(newDim);
            }
        }
    }

    // similar to dbm_t::changeClocks
    void fed_t::changeClocks(const cindex_t *cols, cindex_t newDim)
    {
        if (isEmpty())
        {
            setDimension(newDim);
        }
        else if (newDim <= 1)
        {
            *this = dbm_allocator.dbm1();
        }
        else
        {
            cindex_t oldDim = getDimension();
            for(iterator i = beginMutable(); !i.null(); ++i)
            {
                dbm_t old = *i;
                dbm_updateDBM(i->setNew(newDim),
                              old.const_dbm(),
                              newDim, oldDim, cols);
            }
            ifed()->setDimension(newDim);
        }
    }

    void fed_t::swapClocks(cindex_t x, cindex_t y)
    {
        assert(isOK());

        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            i->ptr_swapClocks(x, y);
        }
    }

    // wrapper to throw exceptions
    DoubleValuation& fed_t::getValuation(DoubleValuation& cval, bool *freeC) const
        throw(std::out_of_range)
    {
        assert(isOK());
        if (isEmpty())
        {
            throw std::out_of_range("No clock valuation for empty federations");
        }
        return const_dbmt().getValuation(cval, freeC);
    }

    // predt(union good, union bad) = union_good intersection_bad predt(good, bad)
    fed_t& fed_t::predt(const fed_t& bad, const raw_t* restrict)
    {
        assert(isOK());
        assert(bad.isOK());
        assert(getDimension() == bad.getDimension());

        if (sameAs(bad))
        {
            setEmpty();
            return *this;
        }
        else if (bad.size() == 1)
        {
            return predt(bad.const_dbmt(), restrict);
        }
        else if (bad.isEmpty())
        {
            return restrict ? down() &= restrict : down();
        }
        else if (!isEmpty())
        {
            fed_t result(getDimension());
            for(const_iterator goods = begin(); !goods.null(); ++goods)
            {
                // Predt for 1st bad.
                const_iterator bads = bad.begin();
                dbm_t downGood = *goods;
                downGood.down();
                if (restrict)
                {
                    downGood &= restrict;
                }
                fed_t intersecPredt = downGood;
                if (downGood.intersects(*bads))
                {
                    dbm_t downBad = *bads;
                    downBad.down();
                    if (restrict)
                    {
                        intersecPredt -= (downBad &= restrict);
                        intersecPredt.steal(((downBad &= *goods) - *bads).down() &= restrict);
                    }
                    else
                    {
                        intersecPredt -= downBad;
                        intersecPredt.steal(((downBad &= *goods) - *bads).down());
                    }
                }
                // Intersection with other predt.
                for(++bads; !bads.null() && !intersecPredt.isEmpty(); ++bads)
                {
                    if (downGood.intersects(*bads))
                    {
                        dbm_t downBad = *bads;
                        downBad.down();
                        fed_t part = downGood;
                        if (restrict)
                        {
                            part -= (downBad &= restrict);
                            part.steal(((downBad &= *goods) - *bads).down() &= restrict);
                        }
                        else
                        {
                            part -= downBad;
                            part.steal(((downBad &= *goods) - *bads).down());
                        }
                        intersecPredt &= part;
                    }
                }
                // Union of partial predt.
                result.steal(intersecPredt);
            }

            // Swap federations.
            ifed_t *ifed = ifedPtr;
            ifedPtr = result.ifedPtr;
            result.ifedPtr = ifed;
        }
        return *this;
    }

    // Simple implementation for bad being convex:
    // predt = (down(G)-down(B)) union down((G inter down(B))-B)
    // where this = G. 
    fed_t& fed_t::predt(const dbm_t& bad, const raw_t *restrict)
    {
        assert(isOK());
        assert(getDimension() == bad.getDimension());

        if (bad.isEmpty())
        {
            return restrict ? down() &= restrict : down();
        }
        else if (!isEmpty())
        {
            dbm_t downBad = bad;
            bool downBadOK = false;

            fed_t result(getDimension());
            for(const_iterator goods = begin(); !goods.null(); ++goods)
            {
                dbm_t downGood = *goods;
                downGood.down();
                if (restrict)
                {
                    downGood &= restrict;
                }
                fed_t intersecPredt = downGood;
                if (downGood.intersects(bad))
                {
                    if (!downBadOK)
                    {
                        downBad.down();
                        if (restrict)
                        {
                            downBad &= restrict;
                        }
                        downBadOK = true;
                    }
                    intersecPredt -= downBad;
                    if (restrict)
                    {
                        intersecPredt.steal(((fed_t(downBad) &= *goods) -= bad).down() &= restrict);
                    }
                    else
                    {
                        intersecPredt.steal(((fed_t(downBad) &= *goods) -= bad).down());
                    }
                }
                result.steal(intersecPredt);    
            }

            // Swap federations.
            ifed_t *ifed = ifedPtr;
            ifedPtr = result.ifedPtr;
            result.ifedPtr = ifed;
        }
        return *this;
    }

    // similar to predt(dbm_t) but if we reuse the code:
    // 1) we may copy bad although we don't need to, 2) we need to
    // test for emptiness.
    fed_t& fed_t::predt(const raw_t* bad, cindex_t dim, const raw_t *restrict)
    {
        assert(isOK());
        assert(dim == getDimension());
        assertx(dbm_isValid(bad, dim));

        if (!isEmpty())
        {
            dbm_t downBad;
            bool downBadOK = false;

            fed_t result(dim);
            for(const_iterator goods = begin(); !goods.null(); ++goods)
            {
                dbm_t downGood = *goods;
                downGood.down();
                if (restrict)
                {
                    downGood &= restrict;
                }
                fed_t intersecPredt = downGood;
                if (downGood.intersects(bad, dim))
                {
                    if (!downBadOK)
                    {
                        downBad.copyFrom(bad, dim);
                        downBad.down();
                        if (restrict)
                        {
                            downBad &= restrict;
                        }
                        downBadOK = true;
                    }
                    intersecPredt -= downBad;
                    if (restrict)
                    {
                        intersecPredt.steal(((fed_t(downBad) &= *goods) -= bad).down() &= restrict);
                    }
                    else
                    {
                        intersecPredt.steal(((fed_t(downBad) &= *goods) -= bad).down());
                    }
                }
                result.steal(intersecPredt);
            }

            // Swap federations.
            ifed_t *ifed = ifedPtr;
            ifedPtr = result.ifedPtr;
            result.ifedPtr = ifed;
        }
        return *this;
    }

    bool fed_t::isIncludedInPredt(const fed_t& good, const fed_t& bad) const
    {
        assert(isOK());
        assert(good.isOK());
        assert(bad.isOK());
        assert(getDimension() == bad.getDimension());
        assert(getDimension() == good.getDimension());

        if (isEmpty())
        {
            return true;
        }
        else if (good.isEmpty() || good.sameAs(bad))
        {
            return false;
        }
        // gooddies
        fed_t downGood = good;
        downGood.down();
        bool ledg = *this <= downGood; //le(downGood);
        if (bad.isEmpty()) // then predt(good,bad) == down(good)
        {
            return ledg;
        }
        else if (!ledg) // since le(predt(good,bad)) => le(down(good))
        {
            return false;
        }
        else
        {
            // baddies
            const_iterator bads = bad.begin();
            
            // predt for 1st bad
            dbm_t downBad = *bads;
            downBad.down();
            fed_t goodAndDownBad = good;
            goodAndDownBad &= downBad;
            fed_t result = downGood;
            result -= downBad;
            if (!goodAndDownBad.isEmpty())
            {
                result.unionWith((goodAndDownBad -= *bads).down());
            }
            
            if (!(++bads).null())
            {
                // false for sure
                if (!(*this <= result)) //(!le(result))
                {
                    return false;
                }
            
                // now intersection with predt(other bads)
                do {
                    if (result.isEmpty())
                    {
                        return false;
                    }
                    downBad = *bads;
                    downBad.down();
                    goodAndDownBad = good;
                    goodAndDownBad &= downBad;
                    
                    fed_t fed = downGood;
                    fed -= downBad;
                    if (!goodAndDownBad.isEmpty())
                    {
                        fed.unionWith((goodAndDownBad -= *bads).down());
                    }
                    // false for sure
                    if (!(*this <= fed)) // (!le(fed))
                    {
                        return false;
                    }
                    result &= fed; // predt(good,union bad) = intersec predt(good,bad)
                } while(!(++bads).null());
            }

            // exact relation wanted
            return le(result);
        }
    }
    
    bool fed_t::has(const dbm_t& arg) const
    {
        assert(isOK());

        if (arg.getDimension() != getDimension())
        {
            return false;
        }
        else if (arg.isEmpty())
        {
            return true;
        }
        else
        {
            for(const_iterator i = begin(); !i.null(); ++i)
            {
                if (*i == arg)
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool fed_t::has(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assertx(dbm_isValid(arg, dim));
        
        if (dim != getDimension())
        {
            return false;
        }
        else
        {
            for(const_iterator i = begin(); !i.null(); ++i)
            {
                if (*i == arg)
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool fed_t::hasSame(const dbm_t& arg) const
    {
        assert(isOK());

        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (i->sameAs(arg))
            {
                return true;
            }
        }
        return false;
    }

    void fed_t::removeIncludedIn(const fed_t& arg)
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());

        if (sameAs(arg))
        {
            // This may not be what you want but
            // it obeys to the API.
            setEmpty();
        }
        else if (!arg.isEmpty())
        {
            for(iterator i = beginMutable(); !i.null(); )
            {
                switch(arg.relation(*i))
                {
                case base_EQUAL:     // this dbm == arg
                case base_SUPERSET:  // this dbm < arg
                    i.remove();
                    break;
                case base_SUBSET:    // this dbm > arg
                case base_DIFFERENT: // not comparable
                    ++i;
                    break;
                }
            }
        }
    }

    bool fed_t::removeIncludedIn(const raw_t* arg, cindex_t dim)
    {
        assert(isOK());
        assert(dim == getDimension());
        assertx(dbm_isValid(arg, dim));

        bool argNotIncluded = true;
        for(iterator i = beginMutable(); !i.null(); )
        {
            switch(dbm_relation(i->const_dbm(), arg, dim))
            {
            case base_EQUAL:     // this dbm == arg
            case base_SUBSET:    // this dbm < arg
                i.remove();
                break;
            case base_SUPERSET:  // this dbm > arg
                argNotIncluded = false;
            case base_DIFFERENT: // not comparable
                ++i;
                break;
            }
        }
        return argNotIncluded;
    }

    // similar to <= with more pre-conditions
    bool fed_t::isSubtractionEmpty(const raw_t* arg, cindex_t dim) const
    {
        assert(isOK());
        assert(getDimension() == dim);
        assertx(dbm_isValid(arg, dim));
        
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (dbm_isSubsetEq(i->const_dbm(), arg, dim))
            {
                return false;
            }
        }
        return true;
    }

    // this - arg
    bool fed_t::isSubtractionEmpty(const fed_t& arg) const
    {
        assert(isOK());
        assert(arg.isOK());
        assert(getDimension() == arg.getDimension());

        if (isEmpty() || sameAs(arg))
        {
            return true;
        }
        else if (arg.isEmpty())
        {
            return false;
        }
        else if (size() == 1)
        {
            return const_dbmt().isSubtractionEmpty(arg);
        }
        else
        {
            cindex_t dim = getDimension();

            // test with 1st DBM of arg: this fed <= 1st DBM of arg
            if ((ptr_relation(arg.begin()->const_dbm(), dim) & base_SUBSET) != 0)
            {
                return true;
            }
            else if (arg.size() == 1)
            {
                return false; // then we know the result
            }
            else
            {
                return (fed_t(*this) -= arg).isEmpty();
            }
        }
    }

    // dbm - fed
    bool fed_t::isSubtractionEmpty(const raw_t* dbm, cindex_t dim, const fed_t& fed)
    {
        assert(fed.isOK());
        assert(fed.getDimension() == dim);
        assertx(dbm_isValid(dbm, dim));

        if (fed.isEmpty())
        {
            return false;
        }
        else if (dim <= 1)
        {
            return true;
        }
#ifndef DISABLE_RELATION_BEFORE_SUBTRACTION
        // full relation test
        else if (fed.isSupersetEq(dbm, dim))
        {
            return true;
        }
#else
        // limited relation test: test if this DBM <= fed's 1st DBM
        else if (dbm_isSubsetEq(dbm, fed.begin()->const_dbm(), dim))
        {
            return true;
        }
#endif
        // trivial: one DBM only -> we know the final result
        else if (fed.size() == 1)
        {
            return false;
        }
        else // not simple
        {
            return (fed_t(dbm_t(dbm, dim)) -= fed).isEmpty();
        }
    }
    
    
    /* - evaluate if substraction should be computed or not
     *   if not: return a copy
     * - compute minimal form of dbm2
     * - for all constraints in the minimal form:
     *   - negate the constraint
     *   - tighten a copy of dbm1 with the negated constraint
     */
    fed_t fed_t::subtract(const raw_t* arg1, const raw_t* arg2, cindex_t dim)
    {
        assertx(dbm_isValid(arg1, dim) && dbm_isValid(arg2, dim));

        if (dim <= 1)
        {
            return fed_t(dim);
        }
        else
        {
            // Real test would be to compute the intersection and check
            // it's not empty but that costs dim^3.
            if (dbm_haveIntersection(arg1, arg2, dim))
            {
                size_t minSize = bits2intsize(dim*dim);
                uint32_t minDBM[minSize];
                size_t nb = dbm_cleanBitMatrix(arg2, dim, minDBM,
                                               dbm_analyzeForMinDBM(arg2, dim, minDBM));
                if (nb == 0)
                {
                    // arg2 is unconstrained => result = empty
                    return fed_t(dim);
                }
                else
                {
                    return fed_t(ifed_t::create(dim,
                                                internSubtract(fdbm_t::create(arg1, dim),
                                                               arg2, dim, minDBM, minSize, nb)));
                }
            }
            // arg1 - arg2 = arg1
            return fed_t(arg1, dim);
        }
    }
    
    // Makes use of getMinDBM(size).
    fed_t fed_t::subtract(const dbm_t& arg1, const dbm_t& arg2)
    {
        assert(arg1.getDimension() == arg2.getDimension());

#ifdef ENABLE_STORE_MINGRAPH
        cindex_t dim = arg1.getDimension();

        if (arg1.isEmpty() || dim <= 1 || arg1.sameAs(arg2))
        {
            return fed_t(dim);
        }
        else if (!arg2.isEmpty() &&
                 dbm_haveIntersection(arg1.const_dbm(), arg2.const_dbm(), dim))
        {
            size_t minSize = bits2intsize(dim*dim);
            size_t nb;
            const uint32_t *minDBM = arg2.getMinDBM(&nb);
            return fed_t(ifed_t::create(dim,
                                        internSubtract(fdbm_t::create(arg1),
                                                       arg2.const_dbm(), dim,
                                                       minDBM, minSize, nb)));
        }
        else
        {
            // arg1 - arg2 = arg1
            return fed_t(arg1);
        }
#else
        return arg2.isEmpty()
            ? fed_t(arg1)
            : arg1-arg2.const_dbm();
#endif
    }

    // similar to raw_t* - raw_t* but avoid a copy if possible
    fed_t fed_t::subtract(const dbm_t& arg1, const raw_t* arg2)
    {
        cindex_t dim = arg1.getDimension();
        assertx(dbm_isValid(arg2, dim));
        
        if (arg1.isEmpty() || dim <= 1)
        {
            return fed_t(dim);
        }
        else
        {
            // Real test would be to compute the intersection and check
            // it's not empty but that costs dim^3.
            if (dbm_haveIntersection(arg1.const_dbm(), arg2, dim))
            {
                size_t minSize = bits2intsize(dim*dim);
                uint32_t minDBM[minSize];
                size_t nb = dbm_cleanBitMatrix(arg2, dim, minDBM,
                                               dbm_analyzeForMinDBM(arg2, dim, minDBM));
                if (nb == 0)
                {
                    // arg2 is unconstrained => result = empty
                    return fed_t(dim);
                }
                else
                {
                    return fed_t(ifed_t::create(dim,
                                                internSubtract(fdbm_t::create(arg1),
                                                               arg2, dim, minDBM, minSize, nb)));
                }
            }
            // arg1 - arg2 = arg1
            return fed_t(arg1);
        }
    }

    raw_t fed_t::getMaxUpper(cindex_t clock) const
    {
        assert(clock > 0 && clock < getDimension());
        raw_t max = INT_MIN;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            raw_t val = i(clock, 0);
            if (val > max)
            {
                max = val;
            }
        }
        return max;
    }

    raw_t fed_t::getMaxLower(cindex_t clock) const
    {
        assert(clock > 0 && clock < getDimension());
        raw_t max = INT_MIN;
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            raw_t val = i(0,clock);
            if (val > max)
            {
                max = val;
            }
        }
        return max;
    }

    fed_t fed_t::toLowerBounds() const
    {
        cindex_t dim = getDimension();
        if (isEmpty() || dim <= 1)
        {
            return *this;
        }

        fed_t result(dim);
        for(const_iterator k = begin(); !k.null(); ++k)
        {
            fed_t part(dim);
            const raw_t *dbm = k();

            for(cindex_t j = 1; j < dim; ++j)
            {
                raw_t low = dbm[j];
                if (dbm_rawIsWeak(low))
                {
                    dbm_t copy(*k);
                    if (copy.constrain(j, 0, dbm_weakNegRaw(low)))
                    {
                        part.add(copy);
                    }
                }
            }
            result.steal(part);
        }

        if (size() > 1)
        {
            for(const_iterator k = begin(); !k.null() && !result.isEmpty(); ++k)
            {
                dbm_t copy(*k);
                copy.tightenDown();
                result -= copy;
            }
        }

        return result;
    }

    fed_t fed_t::toUpperBounds() const
    {
        cindex_t dim = getDimension();
        if (isEmpty())
        {
            return *this;
        }

        fed_t result(dim);
        if (dim <= 1)
        {
            return result;
        }

        for(const_iterator k = begin(); !k.null(); ++k)
        {
            fed_t part(dim);
            const raw_t *dbm = k();

            for(cindex_t i = 1; i < dim; ++i)
            {
                raw_t up = dbm[i*dim];
                if (dbm_rawIsWeak(up))
                {
                    dbm_t copy(*k);
                    if (copy.constrain(0, i, dbm_weakNegRaw(up)))
                    {
                        part.add(copy);
                    }
                }
            }
            result.steal(part);
        }

        if (size() > 1)
        {
            for(const_iterator k = begin(); !k.null() && !result.isEmpty(); ++k)
            {
                dbm_t copy(*k);
                copy.tightenUp();
                result -= copy;
            }
        }

        return result;
    }

    void fed_t::removeEmpty()
    {
        for(iterator i = beginMutable(); !i.null(); )
        {
            if (i->isEmpty())
            {
                i.remove();
            }
            else
            {
                ++i;
            }
        }
    }

    bool fed_t::hasEmpty() const
    {
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            if (i->isEmpty())
            {
                return true;
            }
        }
        return false;
    }

    // Dump its list to mem
    size_t fed_t::write(fdbm_t** mem)
    {
        assert(isOK());

        size_t n = 0;
        if (!isEmpty())
        {
            assert(mem);
            setMutable();
            for(fdbm_t* f = ifed()->head(); f != NULL; f = f->getNext())
            {
                mem[n++] = f;
            }
            ifed()->reset();
        }
        return n;
    }

    // Read its list from mem
    void fed_t::read(fdbm_t** fed, size_t size)
    {
        assert(isOK());
        assert(fed && size && *fed);

        // set new list
        setEmpty();
        setMutable();
        ifed()->reset(*fed, size);

        // re-link
        size_t i;
        for(i = 0; i+1 < size; ++i)
        {
            fed[i]->setNext(fed[i+1]);
        }
        fed[i]->setNext(NULL); // end of list
    }

    bool fed_t::removeThisDBM(const dbm_t &dbm)
    {
        assert(isOK());

        for(iterator i = beginMutable(); !i.null(); ++i)
        {
            if (i->sameAs(dbm))
            {
                i.remove();
                return true;
            }
        }
        return false;
    }

    // Dump the DBMs from the list to an array
    void fed_t::toArray(const raw_t** ar) const
    {
        assert(isOK());
        for(const_iterator i = begin(); !i.null(); ++i)
        {
            *ar++ = i->const_dbm();
        }
    }

    // Implementation of subtraction of a DBM:
    // since we are going to subtract arg several times,
    // compute its minimal graph only once.
    void fed_t::ptr_subtract(const raw_t* arg, cindex_t dim)
    {
        assert(isOK());
        assert(dim == getDimension() && !isEmpty());
        assertx(dbm_isValid(arg, dim));
        assert(isMutable());

        size_t minSize = bits2intsize(dim*dim);
        uint32_t minDBM[minSize];
        size_t nb = 0;
        bool mingraph = false; // Not computed.

        if (dim <= 1)
        {
            ifed()->setEmpty();
        }
        else
        {
            dbmlist_t result;
            fdbm_t *next, *current = ifed()->head();
            do {
                next = current->getNext();
                // Real test would be to compute the intersection and check
                // it's not empty but that costs dim^3.
                if (dbm_haveIntersection(current->dbmt().const_dbm(), arg, dim))
                {
                    if (!mingraph) // Then we need to compute it!
                    {
                        mingraph = true; // Don't compute twice!
                        nb = dbm_cleanBitMatrix(arg, dim, minDBM,
                                                dbm_analyzeForMinDBM(arg, dim, minDBM));
                        assert(nb == base_countBitsN(minDBM, minSize));
                        if (!nb) // That means we remove everything.
                        {
                            ifed()->setEmpty();
                            return;
                        }
                    }
                    // current "disappears" in internSubtract
                    dbmlist_t partial = internSubtract(current, arg, dim, minDBM, minSize, nb);
                    result.unionWith(partial);
                }
                else // current - arg = current
                {
                    result.append(current);
                }
                current = next;
            } while(next);
            ifed()->reset(result);
        }
    }

#ifdef ENABLE_STORE_MINGRAPH
    void fed_t::ptr_subtract(const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());
        assert(!isEmpty() && !arg.isEmpty());
        assert(isMutable());

        cindex_t dim = getDimension();

        if (dim <= 1)
        {
            ifed()->setEmpty();
        }
        else
        {
            size_t minSize = bits2intsize(dim*dim);
            dbmlist_t result;
            fdbm_t *next, *current = ifed()->head();
            do {
                next = current->getNext();
                if (dbm_haveIntersection(current->dbmt().const_dbm(), arg.const_dbm(), dim))
                {
                    size_t nb;
                    const uint32_t *minDBM = arg.getMinDBM(&nb); // skip cleanBitMatrix
                    dbmlist_t partial = internSubtract(current, arg.const_dbm(), dim,
                                                       minDBM, minSize, nb);
                    result.unionWith(partial);
                }
                else // current - arg = current
                {
                    result.append(current);
                }
                current = next;
            } while(next);
            ifed()->reset(result);
        }
    }
#endif

    // Find one DBM (dbm1) of *this s.t. all upper bounds of dbm2
    // are strictly less than those of dbm1.

    bool fed_t::canSkipSubtract(const raw_t* dbm2, cindex_t dim) const
    {
        RECORD_STAT();
        for(const_iterator k = begin(); !k.null(); ++k)
        {
            const raw_t* dbm1 = k->const_dbm();
            for(cindex_t i = 1; i < dim; ++i)
            {
                if (dbm1[i*dim] <= dbm2[i*dim])
                {
                    goto next_k;
                }
            }
            RECORD_SUBSTAT("skip");
            return true;
        next_k: ;
        }
        return false;
    }
}
