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
 * Filename : constraints.h (dbm)
 * C header.
 *
 * Definition of type, constants, and basic operations for constraints.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.2 reviewed.
 * $Id: constraints.h,v 1.20 2005/09/15 12:39:22 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_DBM_CONSTRAINTS_H
#define INCLUDE_DBM_CONSTRAINTS_H

#include <limits.h>
#include <assert.h>
#include "base/inttypes.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

/** To distinguish normal integers and those
 * representing constraints. "raw" is used
 * because it represents an encoding and this
 * is the raw representation of it.
 */
typedef int32_t raw_t;


/** Basic clock constraint representation.
 * xi-xj <= value (with the special encoding)
 */
#ifdef __cplusplus
// C++ version: constructors
struct constraint_t
{
    constraint_t() {}
    constraint_t(const constraint_t &c)
        : i(c.i), j(c.j), value(c.value) {}
    constraint_t(cindex_t ci, cindex_t cj, raw_t vij)
        : i(ci), j(cj), value(vij) {}
    constraint_t(cindex_t ci, cindex_t cj, int32_t bound, bool isStrict)
        : i(ci), j(cj), value((bound << 1) | (isStrict ^ 1)) {}

    bool operator == (const constraint_t& b) const;

    cindex_t i,j;
    raw_t value;
};
#else
typedef struct
{
    cindex_t i,j; /**< indices for xi and xj */
    raw_t value;
} constraint_t;
#endif

/** *Bound* constants.
 */
enum
{
    dbm_INFINITY = INT_MAX >> 1, /**< infinity                           */
    dbm_OVERFLOW = INT_MAX >> 2  /**< to detect overflow on computations */
};


/** Bound *strictness*. Vital constant values *DO NOT CHANGE*.
 */
typedef enum
{
    dbm_STRICT = 0, /**< strict less than constraints:  < x */
    dbm_WEAK = 1    /**< less or equal constraints   : <= x */
}
strictness_t;


/** *Raw* encoded constants.
 */
enum
{
    dbm_LE_ZERO = 1,                       /**< Less Equal Zero                    */
    dbm_LS_INFINITY = (dbm_INFINITY << 1), /**< Less Strict than infinity          */
    dbm_LE_OVERFLOW = dbm_LS_INFINITY >> 1 /**< to detect overflow on computations */
};


/** Encoding of bound into (strict) less or less equal.
 * @param bound,strict: the bound to encode with the strictness.
 * @return encoded constraint ("raw").
 */
static inline
raw_t dbm_bound2raw(int32_t bound, strictness_t strict)
{
    return (bound << 1) | strict;
}


/** Encoding of bound into (strict) less or less equal.
 * @param bound,isStrict: the bound to encode with a flag
 * telling if the bound is strict or not.
 * if isStrict is TRUE then dbm_STRICT is taken,
 * otherwise dbm_WEAK.
 * @return encoded constraint ("raw").
 */
#ifdef __cplusplus
static inline
raw_t dbm_boundbool2raw(int32_t bound, bool isStrict)
{
    return (bound << 1) | (isStrict ^ 1);
}
#else
static inline
raw_t dbm_boundbool2raw(int32_t bound, BOOL isStrict)
{
    return (bound << 1) | (isStrict ^ 1);
}
#endif

/** Decoding of raw representation: bound.
 * @param raw: encoded constraint (bound + strictness).
 * @return the decoded bound value.
 */
static inline
int32_t dbm_raw2bound(raw_t raw)
{
    return (raw >> 1);
}

/** Make an encoded constraint weak.
 * @param raw: bound to make weak.
 * @pre raw != dbm_LS_INFINITY because <= infinity
 * is wrong.
 * @return weak raw.
 */
static inline
raw_t dbm_weakRaw(raw_t raw)
{
    assert(dbm_WEAK == 1);
    assert(raw != dbm_LS_INFINITY);
    return raw | dbm_WEAK; // set bit
}

/** Make an encoded constraint strict.
 * @param raw: bound to make strict.
 * @return strict raw.
 */
static inline
raw_t dbm_strictRaw(raw_t raw)
{
    assert(dbm_WEAK == 1);
    return raw & ~dbm_WEAK; // set bit
}

/** Decoding of raw representation: strictness.
 * @param raw: encoded constraint (bound + strictness).
 * @return the decoded strictness.
 */
static inline
strictness_t dbm_raw2strict(raw_t raw)
{
    return (strictness_t)(raw & 1);
}


/** Tests of strictness.
 * @param raw: encoded constraint (bound + strictness).
 * @return TRUE if the constraint is strict.
 * dbm_rawIsStrict(x) == !dbm_rawIsEq(x)
 */
static inline
BOOL dbm_rawIsStrict(raw_t raw)
{
    return (BOOL)((raw & 1) ^ dbm_WEAK);
}


/** Tests of non strictness.
 * @param raw: encoded constraint (bound + strictness).
 * @return TRUE if the constraint is not strict.
 * dbm_rawIsStrict(x) == !dbm_rawIsEq(x)
 */
static inline
BOOL dbm_rawIsWeak(raw_t raw)
{
    return (BOOL)((raw & 1) ^ dbm_STRICT);
}


/** Negate the strictness of a constraint.
 * @param strictness: the flag to negate.
 */
static inline
strictness_t dbm_negStrict(strictness_t strictness)
{
    return (strictness_t)(strictness ^ 1);
}


/** Negate a constraint:
 * neg(<a) = <=-a
 * neg(<=a) = <-a
 * @param c: the constraint.
 */
static inline
raw_t dbm_negRaw(raw_t c)
{
    /* Check that the trick is correct
     */
    assert(1 - c ==
           dbm_bound2raw(-dbm_raw2bound(c),
                         dbm_negStrict(dbm_raw2strict(c))));
    return 1 - c;
}


/** "Weak" negate a constraint:
 * neg(<=a) = <= -a.
 * @pre c is weak.
 */
static inline
raw_t dbm_weakNegRaw(raw_t c)
{
    assert(dbm_rawIsWeak(c));
    assert(2 - c ==
           dbm_bound2raw(-dbm_raw2bound(c), dbm_WEAK));
    return 2 - c;
}


/** A valid raw bound should not cause overflow in computations.
 * @param x: encoded constraint (bound + strictness)
 * @return TRUE if adding this constraint to any constraint
 * does not overflow.
 */
static inline
BOOL dbm_isValidRaw(raw_t x)
{
    return (BOOL)(x == dbm_LS_INFINITY || (x < dbm_LE_OVERFLOW && -x < dbm_LE_OVERFLOW));
}


/** Constraint addition on raw values : + constraints - excess bit.
 * @param x,y: encoded constraints to add.
 * @return encoded constraint x+y.
 */
static inline
raw_t dbm_addRawRaw(raw_t x, raw_t y)
{
    assert(x <= dbm_LS_INFINITY);
    assert(y <= dbm_LS_INFINITY);
    assert(dbm_isValidRaw(x));
    assert(dbm_isValidRaw(y));
    return (x == dbm_LS_INFINITY || y == dbm_LS_INFINITY) ?
        dbm_LS_INFINITY : (x + y) - ((x | y) & 1);
}


/** Constraint addition:
 * @param x,y: encoded constraints to add.
 * @return encoded constraint x+y.
 * @pre y finite.
 */
static inline
raw_t dbm_addRawFinite(raw_t x, raw_t y)
{
    assert(x <= dbm_LS_INFINITY);
    assert(y < dbm_LS_INFINITY);
    assert(dbm_isValidRaw(x));
    assert(dbm_isValidRaw(y));
    return x == dbm_LS_INFINITY ? dbm_LS_INFINITY : (x + y) - ((x | y) & 1);
}

static inline
raw_t dbm_addFiniteRaw(raw_t x, raw_t y)
{
    return dbm_addRawFinite(y, x);
}


/** Constraint addition.
 * @param x,y: encoded constraints to add.
 * @return encoded constraint x+y.
 * @pre x and y finite.
 */
static inline
raw_t dbm_addFiniteFinite(raw_t x, raw_t y)
{
    assert(x < dbm_LS_INFINITY);
    assert(y < dbm_LS_INFINITY);
    assert(dbm_isValidRaw(x));
    assert(dbm_isValidRaw(y));
    return (x + y) - ((x | y) & 1);
}


/** Specialized constraint addition.
 * @param x,y: finite encoded constraints to add.
 * @pre x & y finite, x or y weak.
 * @return encoded constraint x+y
 */
static inline
raw_t dbm_addFiniteWeak(raw_t x, raw_t y)
{
    assert(x < dbm_LS_INFINITY);
    assert(y < dbm_LS_INFINITY);
    assert(dbm_isValidRaw(x));
    assert(dbm_isValidRaw(y));
    assert((x | y) & 1);
    return x + y - 1;
}


/** Raw constraint increment:
 * @return constraint + increment with test infinity
 * @param c: constraint
 * @param i: increment
 */
static inline
raw_t dbm_rawInc(raw_t c, raw_t i)
{
    return c < dbm_LS_INFINITY ? c + i : c;
}


/** Raw constraint decrement:
 * @return constraint + decremen with test infinity
 * @param c: constraint
 * @param d: decrement
 */
static inline
raw_t dbm_rawDec(raw_t c, raw_t d)
{
    return c < dbm_LS_INFINITY ? c - d : c;
}


/** Convenience function to build a constraint.
 * @param i,j: indices.
 * @param bound: the bound.
 * @param strictness: strictness of the constraint.
 */
static inline
constraint_t dbm_constraint(cindex_t i, cindex_t j,
                            int32_t bound, strictness_t strictness)
{
#ifdef __cplusplus
    return constraint_t(i, j, dbm_bound2raw(bound, strictness));
#else
    constraint_t c =
    {
        i:i,
        j:j,
        value:dbm_bound2raw(bound, strictness)
    };
    return c;
#endif
}


/** 2nd convenience function to build a constraint.
 * @param i,j: indices.
 * @param bound: the bound.
 * @param isStrict: true if constraint is strict
 */
static inline
constraint_t dbm_constraint2(cindex_t i, cindex_t j,
                             int32_t bound, BOOL isStrict)
{
#ifdef __cplusplus
    return constraint_t(i, j, dbm_boundbool2raw(bound, isStrict));
#else
    constraint_t c =
    {
        i:i,
        j:j,
        value:dbm_boundbool2raw(bound, isStrict)
    };
    return c;
#endif
}


/** Negation of a constraint.
 * Swap indices i,j, negate value, and toggle the strictness.
 * @param c: constraint to negate.
 * @return negated constraint.
 */
static inline
constraint_t dbm_negConstraint(constraint_t c)
{
    cindex_t tmp = c.i;
    c.i = c.j;
    c.j = tmp;
    c.value = dbm_negRaw(c.value);
    return c;
}


/** Equality of constraints.
 * @param c1, c2: constraints.
 * @return TRUE if c1 == c2.
 */
static inline
BOOL dbm_areConstraintsEqual(constraint_t c1, constraint_t c2)
{
    return (BOOL)(c1.i == c2.i &&
                  c1.j == c2.j &&
                  c1.value == c2.value);
}


#ifdef __cplusplus

/** Comparison operator < defined if C++
 * @param a,b: constraints to compare.
 * @return true: if a < b
 */
static inline
bool operator < (const constraint_t& a, const constraint_t& b)
{
    return (a.i < b.i)
        || (a.i == b.i && a.j < b.j)
        || (a.i == b.i && a.j == b.j && a.value < b.value);
}

/** Negation operator for constraint_t.
 */
static inline
constraint_t operator !(const constraint_t& c)
{
    return constraint_t(c.j, c.i, dbm_negRaw(c.value));
}

/** Equality operator for constraint_t.
 */
inline
bool constraint_t::operator == (const constraint_t& b) const
{
    return i == b.i && j == b.j && value == b.value;
}

}
#endif

#endif /* INCLUDE_DBM_CONSTRAINTS_H */
