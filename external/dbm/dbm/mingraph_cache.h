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
 * Filename : mingraph_cache.h
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_cache.h,v 1.1 2005/07/20 21:29:34 adavid Exp $
 *
 *********************************************************************/

#ifdef ENABLE_MINGRAPH_CACHE
#ifndef DBM_MINGRAPH_CACHE_H
#define DBM_MINGRAPH_CACHE_H

#include "dbm/constraints.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Get cached result of minimal graph analysis.
 * @return 0xffffffff if cache miss or number of constraints
 * if cache hit (and bitMatrix is updated).
 * @param dbm,dim: DBM of dimension dim
 * @param bitMatrix: where to write the minimal graph (if cache hit)
 * @param hashValue: hash value of the DBM to find it.
 */
size_t mingraph_getCachedResult(const raw_t *dbm, cindex_t dim, uint32_t *bitMatrix, uint32_t hashValue);

/** Put a cached result of minimal graph analysis in the cache.
 * @param dbm,dim: DBM of dimension dim
 * @param bitMatrix: result to store
 * @param hashValue: hash value of the DBM to find it.
 * @param cnt: number of constraints.
 */
void mingraph_putCachedResult(const raw_t *dbm, cindex_t dim, const uint32_t *bitMatrix, uint32_t hashValue, size_t cnt);

#ifdef __cplusplus
}
#endif

#endif /* DBM_MINGRAPH_CACHE_H */
#endif
