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
 * Filename : relation.h (base)
 *
 * Definition of partial order relations between sets or other.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: relation.h,v 1.2 2005/04/22 15:20:10 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_BASE_RELATION_H
#define INCLUDE_BASE_RELATION_H

/** Partial order relations between two sets:
 * the values depend on "exactness" of relations.
 */
typedef enum
{                       /*   EXACT relation   | NON EXACT relation */
                        /*--------------------|--------------------*/
    base_DIFFERENT = 0, /**< incomparable     | not (set1 <= set2) */
    base_SUPERSET = 1,  /**< set1 > set2      | not used           */
    base_GREATER = 1,   /**< same as superset |                    */
    base_SUBSET = 2,    /**< set1 < set2      | set1 <= set2       */
    base_LESS = 2,      /**< same as subset   |                    */
    base_EQUAL = 3      /**< set1 == set2     | not used           */
}
relation_t;

/* Note: testing <=, ie, relation == base_EQUAL || relation == base_SUBSET
 * is equivalent to testing (relation & base_SUBSET). The same applies to
 * >=. It is also a good practice to use parenthesis since '&' has a low
 * precedence.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @return the symmetric relation (useful when
 * swapping arguments for a relation):
 * different -> different
 * superset -> subset
 * subset -> superset
 * equal -> equal
 * Implementation: invert the superset and subset bits.
 * @param rel: relation to invert.
 */
static inline
relation_t base_symRelation(relation_t rel)
{
    return (relation_t) (((rel >> 1) | (rel << 1)) & 3);
}

/** Convertion from subset to superset */
static inline
relation_t base_sub2super(relation_t rel)
{
    return (relation_t) (rel >> 1);
}

/** Conversion from superset to subset */
static inline
relation_t base_super2sub(relation_t rel)
{
    return (relation_t) ((rel & 1) << 1);
}

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_BASE_RELATION_H

