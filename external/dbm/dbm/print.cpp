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
 * Filename : print.cpp (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: print.cpp,v 1.5 2005/05/24 19:13:24 adavid Exp $
 *
 *********************************************************************/

#include <stdlib.h>
#include "dbm/fed.h"
#include "dbm/print.h"
#include "dbm/dbm.h"
#include "io/FileStreamBuffer.h"
#include "base/bitstring.h"
#include "debug/macros.h"
#include "debug/new.h"

/* For easy reading */
#define DBM(I,J) dbm[(I)*dim+(J)]
#define SRC(I,J) src[(I)*dim+(J)]
#define DST(I,J) dst[(I)*dim+(J)]


/* For debugging */

#define ASSERT_DIAG_OK(DBM,DIM) ASSERT(dbm_isDiagonalOK(DBM,DIM), dbm_print(stderr, DBM, DIM))
#define ASSERT_NOT_EMPTY(DBM, DIM) ASSERT(!dbm_debugIsEmpty(DBM, DIM), dbm_print(stderr, DBM, DIM))


/* For easy convertion FILE* to ostream */
#define OUT2OS()              \
io::FileStreamBuffer fsb(out);\
std::ostream os(&fsb)

static const char *_print_prefix = "";
static bool _ruby_format = false;

/* Wrappers */
namespace dbm
{    
    std::ostream& operator << (std::ostream& os, const dbm::dbm_t& dbm)
    {
        return dbm_cppPrint(os, dbm(), dbm.getDimension());
    }

    std::ostream& operator << (std::ostream& os, const dbm::fed_t& fed)
    {
        size_t size = fed.size();
        cindex_t dim = fed.getDimension();
        if (size == 0)
        {
            return _ruby_format
                ? os << "Fed(" << dim << ") {}"
                : os << size << " DBM " << dim << 'x' << dim << "{}";
        }
        if (_ruby_format)
        {
            os << "Fed(" << dim << ") {";
            if (size > 1)
            {
                os << "[\n" << _print_prefix;
            }
            os << " matrix\\\n";
        }
        else
        {
            os << size << (size > 1 ? " DBMs " : " DBM ")
               << dim << 'x' << dim << " {\n";
        }

        if (dim == 1)
        {
            assert(!fed.isEmpty());
            dbm_cppPrintRaw(os, dbm_LE_ZERO);
            if (_ruby_format)
            {
                return os << " }";
            }
            os << '\n';
        }
        else
        {
            for(dbm::fed_t::const_iterator iter(fed); !iter.null(); )
            {
                dbm_cppPrint(os, iter(), dim);
                ++iter;
                if (!iter.null())
                {
                    os << _print_prefix << (_ruby_format ? ",matrix\\\n" : ",\n");
                }
            }
        }
        
        return os << _print_prefix << (_ruby_format && size > 1 ? "]}" : "}");
    }
}

std::ostream& operator << (std::ostream& os, const constraint_t& c)
{
    return dbm_cppPrintRaw(os << 'x' << c.i << '-' << 'x' << c.j, c.value);
}

void dbm_setPrintPrefix(const char *prefix)
{
    if (prefix)
    {
        _print_prefix = prefix;
    }
    else
    {
        _print_prefix = "";
    }
}

void dbm_setRubyFormat(BOOL mode)
{
    _ruby_format = mode;
}

/* Call print raw on all constraints.
 */
std::ostream& dbm_cppPrint(std::ostream& out, const raw_t *dbm, cindex_t dim)
{
    cindex_t i,j;
    
    if (dbm)
    {
        for (i = 0; i < dim; ++i)
        {
            out << _print_prefix;
            for (j = 0; j < dim; ++j)
            {
                dbm_cppPrintRaw(out, DBM(i,j)) << '\t';
            }
            out << "\\\n";
        }
    }

    return out;
}
void dbm_print(FILE* out, const raw_t *dbm, cindex_t dim)
{
    if (dbm)
    {
        OUT2OS();
        dbm_cppPrint(os, dbm, dim);
    }
}


/* Shortcuts to print the difference highlight
 * depending on the activation of the colors.
 */
#ifndef NPRETTY_COLORS
#define PRE_DIFF()  if (diff) out << RED(BOLD)
#define POST_DIFF() if (diff) out << NORMAL
#else
#define PRE_DIFF()
#define POST_DIFF() if (diff) out << '*'
#endif


/* Similar to print but do it twice and mark
 * the difference between the DBMs.
 */
std::ostream& dbm_cppPrintDiff(std::ostream& out,
                               const raw_t *src, const raw_t *dst, cindex_t dim)
{
    cindex_t i,j;
    assert(src && dst);

    out << "DBM diff " << dim << 'x' << dim << ":\n";

    for (i = 0; i < dim; ++i)
    {
        raw_t diff = SRC(i,0) ^ DST(i,0);
        PRE_DIFF();
        dbm_cppPrintRaw(out, SRC(i,0));
        POST_DIFF();
        for (j = 1; j < dim; ++j)
        {
            out << '\t';
            diff = SRC(i,j) ^ DST(i,j);
            PRE_DIFF();
            dbm_cppPrintRaw(out, SRC(i,j));
            POST_DIFF();
        }
        out << '\n';
    }

    out << '\n';
    for (i = 0; i < dim; ++i)
    {
        raw_t diff = SRC(i,0) ^ DST(i,0);
        PRE_DIFF();
        dbm_cppPrintRaw(out, DST(i,0));
        POST_DIFF();
        for (j = 1; j < dim; ++j)
        {
            out << '\t';
            diff = SRC(i,j) ^ DST(i,j);
            PRE_DIFF();
            dbm_cppPrintRaw(out, DST(i,j));
            POST_DIFF();
        }
        out << '\n';
    }

    return out;
}
void dbm_printDiff(FILE *out,
                   const raw_t *src, const raw_t *dst, cindex_t dim)
{
    OUT2OS();
    dbm_cppPrintDiff(os, src, dst, dim);
}

#undef PRE_DIFF
#undef POST_DIFF


/* - copy the original DBM
 * - close the copy
 * - print the difference
 */
std::ostream& dbm_cppPrintCloseDiff(std::ostream& out, const raw_t *dbm, cindex_t dim)
{
    raw_t *copy = new raw_t[dim*dim];

    assert(dbm && dim);
    dbm_copy(copy, dbm, dim);
    dbm_close(copy, dim);
    if (dbm_isEmpty(copy, dim))
    {
        out << RED(BOLD) "Warning: empty DBM!" NORMAL " ";
    }
    dbm_cppPrintDiff(out, dbm, copy, dim);

    delete [] copy;

    return out;
}
void dbm_printCloseDiff(FILE *out, const raw_t *dbm, cindex_t dim)
{
    OUT2OS();
    dbm_cppPrintCloseDiff(os, dbm, dim);
}


/* check for infinity values
 * before printing.
 */
std::ostream& dbm_cppPrintRaw(std::ostream& out, raw_t c)
{
    return dbm_cppPrintBound(out << (dbm_rawIsWeak(c) ? "<=" : "<"), dbm_raw2bound(c));
}
void dbm_printRaw(FILE* out, raw_t c)
{
    OUT2OS();
    dbm_cppPrintRaw(os, c);
}


/* Test for infinity and valid values.
 */
std::ostream& dbm_cppPrintBound(std::ostream& out, int32_t bound)
{
    if (bound == dbm_INFINITY)
    {
        return out << "INF";
    }
    else if (bound == -dbm_INFINITY)
    {
        return out << "-INF";
    }
    else if (bound > dbm_INFINITY || bound < -dbm_INFINITY)
    {
        return out << "illegal";
    }
    else
    {
        return out << bound;
    }
}
void dbm_printBound(FILE *out, int32_t bound)
{
    OUT2OS();
    dbm_cppPrintBound(os, bound);
}


/* Format: [a b c]
 * Print 1st element separately and other preceded by ' '.
 */
std::ostream& dbm_cppPrintRaws(std::ostream& out, const raw_t *data, size_t size)
{
    assert(size == 0 || data);

    out << '[';
    if (size)
    {
        size_t i;
        dbm_cppPrintRaw(out, data[0]);
        for (i = 1; i < size; ++i)
        {
            dbm_cppPrintRaw(out << ' ', data[i]);
        }
    }    
    return out << ']';
}
void dbm_printRaws(FILE *out, const raw_t *data, size_t size)
{
    OUT2OS();
    dbm_cppPrintRaws(os, data, size);
}



/* Print with the format [a b c]
 */
std::ostream& dbm_cppPrintBounds(std::ostream& out, const int32_t *data, size_t size)
{
    assert(size == 0 || data);

    out << '[';
    for(size_t i = 0; i < size; ++i)
    {
        if (i > 0) out << ' ';
        dbm_cppPrintBound(out, data[i]);
    }
    return out << ']';
}
void dbm_printBounds(FILE *out, const int32_t *data, size_t size)
{
    OUT2OS();
    dbm_cppPrintBounds(os, data, size);
}

std::ostream& dbm_cppPrintConstraints(std::ostream& out, const constraint_t *c, size_t n)
{
    assert(c);
    for(size_t i = 0; i < n; ++i)
    {
        if (i > 0) out << ' ';
        dbm_cppPrintRaw(out << '(' << c[i].i << ")-(" << c[i].j << ')', c[i].value);
    }
    return out;
}
void dbm_printConstraints(FILE *out, const constraint_t *c, size_t n)
{
    OUT2OS();
    dbm_cppPrintConstraints(os, c, n);
}

/************************* temporarily here *******************/

#include "dbm/dbmfederation.h"

/* Print a DBM list, straightforward */

std::ostream& dbm_cppPrintFederation(std::ostream& out, const DBMList_t *dbmList, cindex_t dim)
{
    out << "{\n";
    while(dbmList)
    {
        dbm_cppPrint(out, dbmList->dbm, dim);
        dbmList = dbmList->next;
        if (dbmList) out << '\n';
    }
    return out << "}\n";
}
void dbm_printFederation(FILE *out, const DBMList_t *dbmList, cindex_t dim)
{
    OUT2OS();
    dbm_cppPrintFederation(os, dbmList, dim);
}
