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
 * Filename : print.h (dbm)
 * C/C++ header.
 * 
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: print.h,v 1.4 2005/05/24 19:13:24 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_DBM_PRINT_H
#define INCLUDE_DBM_PRINT_H

#include <stdio.h>
#include "dbm/constraints.h"

#ifdef __cplusplus

#include <iostream>

/** Pretty print of a DBM.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param out: output stream.
 */
std::ostream& dbm_cppPrint(std::ostream& out, const raw_t *dbm, cindex_t dim);


/** Pretty print of the difference between 2 DBMs:
 * prints 2 DBMS with the difference in color.
 * If color printing is desactivated then the
 * differences are marked with *.
 * @param dbm1,dbm2: DBMs.
 * @param dim: dimension.
 * @param out: output stream.
 * @pre same dimension for both DBMs.
 */
std::ostream& dbm_cppPrintDiff(std::ostream& out, const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Pretty print of the difference between a DBM
 * and its closure: shows the updates that will be
 * done by the closure.
 * @param out: output stream.
 * @param dbm: the DBM.
 * @param dim: dimension.
 */
std::ostream& dbm_cppPrintCloseDiff(std::ostream& out, const raw_t *dbm, cindex_t dim);


/** Pretty print of one clock constraint.
 * @param out: output stream.
 * @param c: the encoded constraint.
 */
std::ostream& dbm_cppPrintRaw(std::ostream& out, raw_t c);


/** Pretty print of one clock bound.
 * @param out: output stream.
 * @param b: the decoded bound.
 */
std::ostream& dbm_cppPrintBound(std::ostream& out, int32_t b);


/** Print a vector of constraints.
 * @param data: the vector of constraints
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
std::ostream& dbm_cppPrintRaws(std::ostream& out, const raw_t *data, size_t size);


/** Print a vector of bounds.
 * @param data: the vector of bounds.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
std::ostream& dbm_cppPrintBounds(std::ostream& out, const int32_t *data, size_t size);


/** Print constraints.
 * @param c: constraints to print.
 * @param n: number of constraints
 */
std::ostream& dbm_cppPrintConstraints(std::ostream& out, const constraint_t *c, size_t n);

#ifndef INCLUDE_DBM_FED_H
namespace dbm
{
    class dbm_t;
    class fed_t;
}
#endif

/** Operator overload.
 * @param c: constraint to print.
 */
std::ostream& operator << (std::ostream& os, const constraint_t& c);

namespace dbm // Needed for unidentifed reason.
{
/** Operator overload.
 * @param dbm: DBM to print.
 */
std::ostream& operator << (std::ostream& os, const dbm::dbm_t& dbm);

/** Operator overload.
 * @param fed: federation to print.
 */
std::ostream& operator << (std::ostream& os, const dbm::fed_t& fed);
}

extern "C" {

#endif

/** Print prefix for every line of DBM.
 *  Do not deallocate the string in argument
 *  since only a reference is kept. If you
 *  want to deallocate, then reset the library
 *  with NULL.
 */
void dbm_setPrintPrefix(const char *prefix);


/** Change printing format to match the
 *  input format of the Ruby binding.
 *  By default it is off.
 */
void dbm_setRubyFormat(BOOL mode);


/** Pretty print of a DBM.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param out: output stream.
 */
void dbm_print(FILE* out, const raw_t *dbm, cindex_t dim);


/** Pretty print of the difference between 2 DBMs:
 * prints 2 DBMS with the difference in color.
 * If color printing is desactivated then the
 * differences are marked with *.
 * @param dbm1,dbm2: DBMs.
 * @param dim: dimension.
 * @param out: output stream.
 * @pre same dimension for both DBMs.
 */
void dbm_printDiff(FILE *out, const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Pretty print of the difference between a DBM
 * and its closure: shows the updates that will be
 * done by the closure.
 * @param out: output stream.
 * @param dbm: the DBM.
 * @param dim: dimension.
 */
void dbm_printCloseDiff(FILE *out, const raw_t *dbm, cindex_t dim);


/** Pretty print of one clock constraint.
 * @param out: output stream.
 * @param c: the encoded constraint.
 */
void dbm_printRaw(FILE* out, raw_t c);


/** Pretty print of one clock bound.
 * @param out: output stream.
 * @param b: the decoded bound.
 */
void dbm_printBound(FILE* out, int32_t b);


/** Print a vector of constraints.
 * @param data: the vector of constraints
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
void dbm_printRaws(FILE *out, const raw_t *data, size_t size);


/** Print a vector of bounds.
 * @param data: the vector of bounds.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
void dbm_printBounds(FILE *out, const int32_t *data, size_t size);


/** Print constraints.
 * @param n: number of constraints to print.
 * @param c: constraint.
 * @param out: where to print.
 */
void dbm_printConstraints(FILE *out, const constraint_t *c, size_t n);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DBM_PRINT_H */
