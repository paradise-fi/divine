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
 * Filename : doubles.h
 *
 * Basic stuff on doubles.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: doubles.h,v 1.2 2005/06/20 16:27:03 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_BASE_DOUBLES_H
#define INCLUDE_BASE_DOUBLES_H

#include <float.h>

/* Epsilon */
/* #define base_EPSILON (DBL_EPSILON*10)
 * BigDecimal in the GUI is lost beyond
 * 1e-6. In addition, the GUI computes middle
 * interval points so 2e-6 is the smallest
 * value compatible with the GUI. There is
 * an unavoidable problem if the user can
 * recursively choose middle points.
 * If the epsilon is too big then the point
 * inclusion check may fail.
 */
#define base_EPSILON 2e-6

#ifdef __cplusplus
extern "C" {
#endif

/* Add/subtract epsilon to/from value and
 * make sure the result is different from
 * the original (except if too big or too
 * small).
 */
double base_addEpsilon(double value, double epsilon);
double base_subtractEpsilon(double value, double epsilon);

#ifdef __cplusplus
}
#endif

/* Comparisons between doubles. */

/*
#define IS_GT(X,Y) ((X) > (Y)+DBL_EPSILON)
#define IS_LT(X,Y) ((X) < (Y)-DBL_EPSILON)
#define IS_GE(X,Y) ((X) > (Y)-DBL_EPSILON)
#define IS_LE(X,Y) ((X) < (Y)+DBL_EPSILON)
*/
#define IS_GT(X,Y) ((X)-(Y) >= 1e-14)
#define IS_LT(X,Y) IS_GT(Y,X)
#define IS_GE(X,Y) ((Y)-(X) <= 1e-14)
#define IS_LE(X,Y) IS_GE(Y,X)

#define IS_EQ(X,Y) (IS_GE(X,Y) && IS_LE(X,Y))

/*

Justification of 1e-14:

irb
irb(main):001:0> 7.613442-(8.613442-1)
=> 8.88178419700125e-16

1e-15 would be the immediate reasonable
choice, we take 1e-14 to be a little safer.

Of course that doesn't guarantee it will be
fine for all the other cases but it seems a
reasonable choice given the precision of the
points handled by the GUI.

*/

#endif // INCLUDE_BASE_DOUBLES_H
