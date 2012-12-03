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
/*********************************************************************
 *
 * Filename : mingraph.h (dbm)
 *
 * Minimal graph representation, encoded with the cheapest format
 * possible.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph.h,v 1.22 2005/09/29 16:10:43 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_DBM_MINGRAPH_H
#define INCLUDE_DBM_MINGRAPH_H

#include "base/c_allocator.h"
#include "base/intutils.h"
#include "dbm/dbm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************
 * For practical reasons we assume that DBM dimensions are
 * <= 2^16-1. This is large enough since the size needed is
 * dim*dim integers per DBM and for dim=2^16 we need 2^32 integers
 * for only 1 DBM, which go well beyond the addressable memory in 32 bits.
 *************************************************************************/


/**
 * @file 
 *
 * Support for minimum graph representation.
 *
 * The minimal graph is represented internally by the cheapest format
 * possible, and in some cases it may even not be reduced. In addition
 * to this, the size of the minimal graph depends on the actual input
 * DBM, which makes the size of the allocated memory unpredictable
 * from the caller point of view.
 *
 *
 *
 * @section allocation How allocation works
 *
 * The idea is to have generic implementation that can be used with
 * standard allocation schemes (malloc, new) and with custom
 * allocators. This interface is in C to make it easy to wrap to other
 * languages so we use a generic function to allocate memory.  The
 * type of this function is 
 * \code 
 * int32_t* function(uint32_t size, void *data) 
 * \endcode 
 * where: 
 * - \a size is the size in \c int to allocate, and it returns a
 *   pointer to a \c int32_t[size]
 * - \a data is other custom data.
 *
 * Possible wrappers are:
 * - for a custom allocator Alloc:
 *   \code
 *   int32_t *alloc(uint32_t size, void *data) {
 *     return ((Alloc*)data)->alloc(size);
 *   }
 *   \endcode
 *   defined as base_allocate in base/DataAllocator.h
 * - for malloc:
 *   \code
 *   int32_t *alloc(uint32_t size, void *) {
 *     return (int32_t*) malloc(size*sizeof(int32_t));
 *   }
 *   \endcode
 *   defined as base_malloc in base/c_allocator.h
 * - for new:
 *   \code
 *   int32_t *alloc(uint32_t size, void *) {
 *     return new int32_t[size];
 *   }
 *   \endcode
 *   defined as base_new in base/DataAllocator.h
 *
 * The allocator function and the custom data are packed
 * together inside the allocator_t type.
 *
 * @see base/c_allocator.h
 * @see dbm_writeToMinDBMWithOffset()
 */


/** Style typedef: to make the difference clear
 * between just allocated memory and the minimal
 * graph representation.
 */
typedef const int32_t* mingraph_t;



/** Save a DBM in minimal representation.
 *
 * The API supports allocation of larger data structures than needed
 * for the actual zone representation. When the \a offset argument is
 * bigger than zero, \a offset extra integers are allocated and the
 * zone is written with the given offset. Thus when \c
 * int32_t[data_size] is needed to represent the reduced zone, an \c
 * int32_t array of size \c offset+data_size\c is allocated. The
 * first \a offset elements can be used by the caller. It is important
 * to notice that the other functions typically expect a pointer to
 * the actual zone data and not to the beginning of the allocated
 * block. Thus in the following piece of code, most functions expect
 * \c mg and not \c memory:
 *
 * \code 
 * int32_t *memory = dbm_writeToMinDBMWithOffset(...);
 * mingraph_t mg = &memory[offset]; 
 * \endcode 
 * 
 * \b NOTES:
 * - if \a offset is 0 and \a dim is 1, NULL may be returned.
 *   NULL is valid as an input to the other functions.
 * - it could be possible to send as argument the maximal
 *   value of the constraints that can be deduced from
 *   the maximal constants but this would tie the algorithm
 *   to the extrapolation.
 *
 *
 * @param dbm: the DBM to save.
 * @param dim: its dimension.
 * @param minimizeGraph: activate minimized graph
 * reduction. If it is FALSE, then the DBM is copied
 * without its diagonal.
 * @param tryConstraints16: flag to try to save
 * constraints on 16 bits, will cost dim*dim time.
 * @param c_alloc: C allocator wrapper
 * @param offset: offset for allocation.
 * @return allocated memory.
 * @pre
 * - dbm is a raw_t[dim*dim]
 * - allocFunction allocates memory in integer units
 * - dbm is closed.
 * - dim > 0 (at least ref clock)
 * @post the returned memory is of size offset+something
 *  unknown from the caller point of view.
 */
int32_t* dbm_writeToMinDBMWithOffset(const raw_t* dbm, cindex_t dim,
                                     BOOL minimizeGraph,
                                     BOOL tryConstraints16,
                                     allocator_t c_alloc,
                                     size_t offset);


/** Save a pre-analyzed DBM in minimal representation.
 * @param dbm: the DBM to save.
 * @param dim: its dimension.
 * @param bitMatrix: bit matrix resulting from the
 * analysis (ie dbm_analyzeForMinDBM).
 * @param nbConstraints: number of constraints in the
 * bit matrix.
 * @param tryConstraints16: flag to try to save
 * constraints on 16 bits, will cost dim*dim time.
 * @param allocFunction: the allocation function.
 * @param offset: offset for allocation.
 * @return allocated memory.
 * @pre
 * - dbm is a raw_t[dim*dim]
 * - allocFunction allocates memory in integer units
 * - dbm is closed.
 * - dim > 0 (at least ref clock)
 * - bitMatrix != NULL, obtained from dbm_analyzeForMinDBM
 * - nbConstraints = number of bits set in bitMatrix
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 * @post
 * - the returned memory is of size offset+something
 *   unknown from the caller point of view.
 * - bitMatrix is cleaned from the constraints xi >= 0
 */
int32_t* dbm_writeAnalyzedDBM(const raw_t *dbm, cindex_t dim,
                              uint32_t *bitMatrix,
                              size_t nbConstraints,
                              BOOL tryConstraints16,
                              allocator_t c_alloc,
                              size_t offset);


/** 
 * Analyze a DBM for its minimal graph representation. Computes the
 * smallest number of constraints needed to represent the same zone as
 * the full DBM in \a dbm. The result in returned in \a bitMatrix: If
 * the bit \f$ i \cdot dim + j\f$ is set, then the constraint
 * \f$(i,j)\f$ of \a dbm is needed.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param bitMatrix: bit matrix.
 * @return
 * - number of needed constraints to save
 * - bit matrix that marks which constraints belong to the minimal graph
 * @pre bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
size_t dbm_analyzeForMinDBM(const raw_t *dbm, cindex_t dim, uint32_t *bitMatrix);

/** @return TRUE if the mingraph contains zero, FALSE otherwise.
 */
BOOL dbm_mingraphHasZero(mingraph_t ming);

/**
 * This is a post-processing function for dbm_analyzeForMinDBM
 * to remove constraints of the form x>=0 that are part of the
 * minimal graph but that do not give much information.
 * @param dbm,dim: DBM of dimension dim
 * @param bitMatrix: bit matrix (already computed minimal graph)
 * @param nbConstraints: the number of constraints of the minimal graph.
 * @return the updated number of constraints of the modified minimal
 * graph where constraints x>=0 may have been removed.
 * @pre dbm_analyzeForMinDBM has been called before and nbConstraints
 * corresponds to the number of constraints of the minimal graph.
 */
size_t dbm_cleanBitMatrix(const raw_t *dbm, cindex_t dim, uint32_t *bitMatrix, size_t nbConstraints);


/**
 * Get back the minimal graph from the internal representation.
 * It might be the case that the minimal graph is recomputed if
 * the DBM was copied.
 * @param ming: internal representation of the minimal graph.
 * @param bitMatrix: bit matrix of the minimal graph to write.
 * @param isUnpacked: says if buffer contains the unpacked DBM already.
 * @param buffer: where to uncompress the full DBM if needed.
 * @return the number of constraints part of the minimal graph.
 * @pre let dim be the dimension of ming, bitMatrix is a uint32_t[bit2intsize(dim*dim)],
 * buffer is a raw_t[dim*dim], and if isUnpacked is true then buffer contains
 * the full DBM of ming.
 * @post if ming is the result of saving a given dbm A, then the resulting
 * bitMatrix is the same as the one given by dbm_analyzeForMinDBM(A,dim,bitMatrix)
 * + buffer always contains the unpacked mingraph.
 */
size_t dbm_getBitMatrixFromMinDBM(uint32_t *bitMatrix, mingraph_t ming,
                                  BOOL isUnpacked, raw_t *buffer);


/**
 * Convert a bit matrix marking constraints to an array of indices.
 * Encoding: i[k] = (index[k] & 0xffff) and j[k] = (index[k] >> 16)
 * @param bitMatrix: the bit matrix to convert.
 * @param nbConstraints: number of set bit in the matrix.
 * @param index: the index array to write.
 * @param dim: dimension of the bit matrix.
 * @pre index is a indexij_t[dim*(dim-1)] and bits on the
 * diagonal are not marked, nbConstraints <= dim*(dim-1).
 */
void dbm_bitMatrix2indices(const uint32_t *bitMatrix, size_t nbConstraints,
                           uint32_t *index, cindex_t dim);


/** Read a DBM from its minimal DBM representation.
 * @param dbm: where to write.
 * @param minDBM: the minimal representation to read from.
 * @pre
 * - dbm is a raw_t[dim*dim] where dim = dbm_getDimOfMinDBM(minDBM)
 * - minDBM points to the data of the minDBM directly. There is no
 *   offset.
 * @return dimension of DBM and unpacked DBM in dbm.
 */
cindex_t dbm_readFromMinDBM(raw_t *dbm, mingraph_t minDBM);


/** Dimension of a DBM from its packed minimal representation.
 * @param minDBM: the minimal DBM data directly, without offset.
 * @return dimension of DBM.
 */
cindex_t dbm_getDimOfMinDBM(mingraph_t minDBM);


/** Size of the representation of the MinDBM.
 * This is the size of the allocated memory (without offset) for the
 * minimal representation. It is at most dim*(dim-1)+1 because in the
 * worst case the DBM is copied without its diagonal and we need one
 * integer to store the size.
 * @param minDBM: minimal DBM representation (without offset).
 * @return size in int32_t allocated for the representation.
 * NOTE: the write function allocates exactly
 * int32_t[offset+dbm_getSizeOfMinDBM(minDBM)]
 */
size_t dbm_getSizeOfMinDBM(mingraph_t minDBM);


/** Equality test with a full DBM.
 * Unfortunately, this may be expensive (dim^3) if the
 * minimal graph format is used.
 * @param dbm: full DBM.
 * @param dim: dimension of dbm.
 * @param minDBM: minimal DBM representation (without offset).
 * @pre dbm is a raw_t[dim*dim] and dim > 0 (at least ref clock)
 * @return TRUE if the DBMs are the same, FALSE otherwise.
 */
BOOL dbm_isEqualToMinDBM(const raw_t *dbm, cindex_t dim, mingraph_t minDBM);


/** Equality test between 2 minimal graphs (saved
 * with the same function and flags).
 * @param mg1,mg2: minimal graph arguments.
 * @return TRUE if the graphs are the same, provided
 * they were saved with the same flags, FALSE otherwise.
 */
static inline
BOOL dbm_areMinDBMVerbatimEqual(mingraph_t mg1, mingraph_t mg2)
{
    assert(mg1 && mg2); /* mingraph_t == const int* */
    return (BOOL)
        (*mg1 == *mg2 && base_areEqual(mg1+1, mg2+1, dbm_getSizeOfMinDBM(mg1)-1));
}


/** Equality test with a full DBM.
 * This variant of the equality test may be used if the
 * DBM has already been analyzed or if you need to reuse
 * the result of the analysis.
 * @param dbm: full DBM.
 * @param dim: dimension of dbm.
 * @param bitMatrix: bit matrix resulting from the analysis
 * of dbm (dbm_analyzeforMinDBM).
 * @param nbConstraints: number of constraints marked in
 * the bit matrix = number of bits set.
 * @param minDBM: minimal DBM representation (without offset).
 * @pre
 * - dbm is a raw_t[dim*dim] and dim > 0 (at least ref clock)
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 * @return TRUE if the DBMs are the same, FALSE otherwise.
 * @post the bitMatrix may have the bits for the constraints on
 * the 1st row cleaned and nbConstraints will be updated accordingly.
 */
BOOL dbm_isAnalyzedDBMEqualToMinDBM(const raw_t *dbm, cindex_t dim,
                                    uint32_t *bitMatrix,
                                    size_t *nbConstraints,
                                    mingraph_t minDBM);


/** Another variant for equality checking:
 * this one may unpack the minimal graph if needed.
 * It needs a buffer as an input to do so.
 * @param dbm, dim: full DBM of dimension dim.
 * @param minDBM: minimal DBM representation (without offset).
 * @param buffer: buffer to unpack the DBM if needed
 * @pre
 * - dbm is a raw_t[dim*dim] and dim > 0 (at leat ref clock)
 * - buffer != NULL is a raw_t[dim*dim]
 * @post
 * - buffer may be written or not. If you want to know
 *   it, you can set buffer[0] = 0, and test afterwards
 *   if buffer[0] == 0. If mingraph is unpacked then it
 *   is guaranteed that buffer[0] == dbm_LE_ZERO, otherwise
 *   buffer is untouched.
 * @return TRUE if the DBMs are the same, FALSE otherwise.
 */
BOOL dbm_isUnpackedEqualToMinDBM(const raw_t *dbm, cindex_t dim,
                                 mingraph_t minDBM, raw_t *buffer);


/** Relation between a full DBM and a minimal representation
 * DBM. The relation may be exact or not:
 * if the relation is exact then
 *   dbm_EQUAL is returned if dbm == minDBM
 *   dbm_SUBSET is returned if dbm < minDBM
 *   dbm_SUPERSET is returned if dbm > minDBM
 *   dbm_DIFFERENT is returned if dbm not comparable with minDBM
 * else
 *   dbm_SUBSET is returned if dbm <= minDBM
 *   dbm_DIFFERENT otherwise
 *
 * @param dbm: full DBM to test.
 * @param dim: its dimension.
 * @param minDBM: minimal DBM representation.
 * @param unpackBuffer: memory space to unpack.
 *  if unpackBuffer == NULL then the relation is not exact
 *  if unpackBuffer != NULL then assume that buffer = raw_t[dim*dim]
 *  to be able to unpack the DBM and compute an exact relation.
 *
 * NOTE: unpackBuffer is sent as an argument to avoid stackoverflow
 * if using stack allocation. The needed size is in dim*dim*sizeof(int).
 * It is NOT guaranteed that unpackBuffer will be written. The dbm
 * MAY be unpacked only if relation != subset and unpackBuffer != NULL.
 *
 * @pre
 * - dbm is closed and not empty
 * - dbm is a raw_t[dim*dim] (full DBM)
 * - dim > 0 (at least ref clock)
 * @return relation as described above.
 */
relation_t dbm_relationWithMinDBM(const raw_t *dbm, cindex_t dim,
                                  mingraph_t minDBM, raw_t *unpackBuffer);


/** Variant of the previous relation function.
 * This may be cheaper if what count is to know
 * (subset or equal) or different or superset.
 * if unpackBuffer != NULL then
 *   dbm_EQUAL or dbm_SUBSET is returned if dbm == minDBM
 *   dbm_SUBSET is returned if dbm < minDBM
 *   dbm_SUPERSET is returned if dbm > minDBM
 *   dbm_DIFFERENT is returned if dbm not comparable with minDBM
 * else
 *   dbm_SUBSET is returned if dbm <= minDBM
 *   dbm_DIFFERENT is returned otherwise
 *
 * @param dbm: full DBM to test.
 * @param dim: its dimension.
 * @param minDBM: minimal DBM representation.
 * @param unpackBuffer: memory space to unpack.
 *  if unpackBuffer == NULL then the relation is not exact
 *  if unpackBuffer != NULL then assume that buffer = raw_t[dim*dim]
 *  to be able to unpack the DBM and compute an exact relation.
 * @pre
 * - dbm is closed and not empty
 * - dbm is a raw_t[dim*dim] (full DBM)
 * - dim > 0 (at least ref clock)
 * @return relation as described above.
 */
relation_t dbm_approxRelationWithMinDBM(const raw_t *dbm, cindex_t dim,
                                        mingraph_t minDBM, raw_t *unpackBuffer);


/** Convex union.
 * This may cost dim^3 in time since the minimal DBM has
 * to be unpacked to a full DBM.
 * Computes dbm = dbm + minDBM
 * @param dbm: dbm to make the union with.
 * @param dim: its dimension.
 * @param minDBM: minDBM to add to dbm (without offset).
 * @param unpackBuffer: memory space to unpack the DBM.
 * @pre
 * - dbm is a raw_t[dim*dim] and dim > 0 (at least ref clock).
 * - DBMs have the same dimensions
 * - unpackBuffer != NULL and is a raw_t[dim*dim]
 */
void dbm_convexUnionWithMinDBM(raw_t *dbm, cindex_t dim,
                               mingraph_t minDBM, raw_t *unpackBuffer);


/** Simple type to allow for statistics on the different internal
 * formats used. The format are not user controllable and should
 * not be read from outside. For the tuple representation, it is
 * not said here if it is (i,j,c_ij) or a bunch of c_ij and (i,j).
 */
typedef enum
{
    dbm_MINDBM_TRIVIAL = 0,  /**< only clock ref, dim == 1           */
    dbm_MINDBM_COPY32,       /**< 32 bits, dbm copy without diagonal */
    dbm_MINDBM_BITMATRIX32,  /**< 32 bits, c_ij and a bit matrix     */
    dbm_MINDBM_TUPLES32,     /**< 32 bits, c_ij and tuples (i,j)     */
    dbm_MINDBM_COPY16,       /**< 16 bits, dbm copy without diagonal */
    dbm_MINDBM_BITMATRIX16,  /**< 16 bits, c_ij and a bit matrix     */
    dbm_MINDBM_TUPLES16,     /**< 16 bits, c_ij and tuples (i,j)     */
    dbm_MINDBM_ERROR         /**< should never be the case */
}
representationOfMinDBM_t;


/** @return the type of the internal format used.
 * @param minDBM: minimal representation (without offset)
 * to read.
 */
representationOfMinDBM_t dbm_getRepresentationType(mingraph_t minDBM);
    


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DBM_MINGRAPH_H */
