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
 * Filename : dbm.h -- private API
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: dbm.h,v 1.3 2005/11/22 12:18:59 adavid Exp $
 *
 **********************************************************************/

#ifndef DBM_DBM_H
#define DBM_DBM_H

#include "base/inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Internal function: compute redirection tables
 * table and cols (for update of DBM).
 * @see dbm_shrinkExpand for table
 * cols redirects indices from the source DBM to the
 * destination DBM for copy purposes:
 * copy row i to destination[i] from source[cols[i]]
 * @param bitSrc: source array bits
 * @param bitDst: destination array bits
 * @param bitSize: size of the arrays
 * @param table, cols: tables to compute
 * @return dimension of the new DBM
 */
cindex_t dbm_computeTables(const uint32_t *bitSrc,
                          const uint32_t *bitDst,
                          size_t bitSize,
                          cindex_t *table,
                          cindex_t *cols);


/** Internal function: update a DBM (copy & and insert new rows).
 * @param dbmDst, dimDst: destination DBM of dimension dimDst
 * @param dbmSrc, dimSrc: source DBM of dimension dimSrc
 * @param cols: indirection table to copy the DBM, ie,
 * copy row i to destination[i] from source[cols[i]]
 * @pre cols computed from ext_computeTables, 1st element
 * of dbmDst is written
 */
void dbm_updateDBM(raw_t *dbmDst, const raw_t *dbmSrc,
                   cindex_t dimDst, cindex_t dimSrc,
                   const cindex_t *cols);


#ifndef NCLOSELU

/** Specialized close for extrapolation: can skip
 * outer loop k if lower[k] == upper[k] == infinity.
 */
void dbm_closeLU(raw_t *dbm, cindex_t dim, const int32_t *lower, const int32_t *upper);

#endif

/* Add bits of constraints, skip the
 * case of infinity. Used to compute the
 * range of bits needed (32/16 bits).
 */
#define ADD_BITS(BITS, VALUE)   \
if ((VALUE) != dbm_LS_INFINITY) \
{                               \
    BITS |= base_absNot(VALUE); \
}

#ifdef __cplusplus
}
#endif

#endif
