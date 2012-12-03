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
 * Filename : utils.c (debug)
 *
 * Implementation of debug utility functions.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.3 reviewed.
 * $Id: utils.cpp,v 1.7 2005/05/11 19:31:57 adavid Exp $
 *
 **********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "io/FileStreamBuffer.h"
#include "debug/macros.h"
#include "base/bitstring.h"


/* wrap FILE* out to std::ostream& os
 */
#define OUT2OS()              \
io::FileStreamBuffer fsb(out);\
std::ostream os(&fsb)


/* Straigth forward randomization of memory.
 */
void debug_randomize(void *data, size_t intsize)
{
    uint32_t *p = (uint32_t*) data;
    assert(data);
    while(intsize)
    {
        *p++ = RAND();
        --intsize;
    }
}


/* Call printBits for every int to print.
 */
std::ostream& debug_cppPrintBitstring(std::ostream& out, const uint32_t *s, size_t n)
{
    if (n)
    {
        assert(s);
        debug_cppPrintBits(out, *s);
        while(--n) debug_cppPrintBits(out << ' ', *++s);
    }
    return out;
}
void debug_printBitstring(FILE *out, const uint32_t *s, size_t n)
{
    OUT2OS();
    debug_cppPrintBitstring(os, s, n);
}
std::ostream& debug_cppPrintBitMatrix(std::ostream& out, const uint32_t *s, cindex_t dim)
{
    cindex_t i,j;
    for(i = 0; i < dim; ++i)
    {
        for(j = 0; j < dim; ++j)
        {
            out << base_getOneBit(s, i*dim+j) << ' ';
        }
        out << std::endl;
    }
    return out;
}
void debug_printBitMatrix(FILE* out, const uint32_t *s, cindex_t dim)
{
    OUT2OS();
    debug_cppPrintBitMatrix(os, s, dim);
}

/* Call printDiffBits for every int to print.
 */
std::ostream& debug_cppPrintDiffBitstrings(std::ostream& out,
                                           const uint32_t *s1, const uint32_t *s2, size_t n)
{
    if (n)
    {
        /* for 2nd pass: revert order of s1 s2
         */
        const uint32_t *ss1 = s2;
        const uint32_t *ss2 = s1;
        size_t nn = n;

        assert(s1 && s2);

        debug_cppPrintDiffBits(out, *s1, *s2);
        while(--n) debug_cppPrintDiffBits(out << ' ', *++s1, *++s2);
        debug_cppPrintDiffBits(out << '\n', *ss1, *ss2);
        while(--nn) debug_cppPrintDiffBits(out << ' ', *++ss1, *++ss2);
        out << '\n';
    }
    return out;
}
void debug_printDiffBitstrings(FILE *out,
                               const uint32_t *s1, const uint32_t *s2, size_t n)
{
    OUT2OS();
    debug_cppPrintDiffBitstrings(os, s1, s2, n);
}


/* Print all bits of an int starting from the lower bit.
 */
std::ostream& debug_cppPrintBits(std::ostream& out, uint32_t i)
{
    uint32_t n = 32; /* 32 bits to print */
    do {

        /* more readable string of bits
         */
#ifndef NPRETTY_COLORS
        if ((n & 7) == 4)
        {
            out << BLUE(THIN);
        }
        else if ((n & 7) == 0)
        {
            out << NORMAL;
        }
#endif

        out << (char)('0' + (i & 1)); /* character '0' + bit */
        i >>= 1;                      /* next bit */
    } while(--n);

#ifndef NPRETTY_COLOR
    out << NORMAL;
#endif
    return out;
}
void debug_printBits(FILE *out, uint32_t i)
{
    OUT2OS();
    debug_cppPrintBits(os, i);
}


/* Print all bits of the first int starting from the lower bit,
 * highlighting the differences with the second.
 */
std::ostream& debug_cppPrintDiffBits(std::ostream& out, uint32_t i, uint32_t j)
{
    uint32_t n = 32; /* 32 bits to print */
#ifndef NPRETTY_COLORS
    const char *color = NORMAL;
#endif

    do {
        bool same = ((i & 1) == (j & 1));

        /* more readable string of bits
         */
#ifndef NPRETTY_COLORS
        if ((n & 7) == 4)
        {
            color = BLUE(THIN);
        }
        else if ((n & 7) == 0)
        {
            color = NORMAL;
        }

        out << (same ? color : RED(THIN));
#endif

        out << (char)('0' + (i & 1)); /* character '0' + bit */

#ifdef NPRETTY_COLORS
        if (!same) out << '*';
#endif

        i >>= 1;                   /* next bit */
        j >>= 1;
    } while(--n);

#ifndef NPRETTY_COLOR
    out << NORMAL;
#endif

    return out;
}
void debug_printDiffBits(FILE *out, uint32_t i, uint32_t j)
{
    OUT2OS();
    debug_cppPrintDiffBits(os, i, j);
}


/* Print with the format [a b c]
 */
std::ostream& debug_cppPrintVector(std::ostream &out, const int32_t *data, size_t size)
{
    assert(size == 0 || data);

    out << '(' << size << ")[";
    for(size_t i = 0; i < size; ++i)
    {
        out << ' ' << data[i];
    }
    return out << " ]";
}
void debug_printVector(FILE *out, const int32_t *data, size_t size)
{
    OUT2OS();
    debug_cppPrintVector(os, data, size);
}


/* same as debug_printVector but for double
 */
std::ostream& debug_cppPrintRealVector(std::ostream &out, const double *data, size_t size)
{
    assert(size == 0 || data);

    out << '(' << size << ")[";
    for(size_t i = 0; i < size; ++i)
    {
        out << ' ' << data[i];
    }
    return out << " ]";
}
void debug_printRealVector(FILE *out, const double *data, size_t size)
{
    OUT2OS();
    debug_cppPrintRealVector(os, data, size);
}

/* To print differences
 */
#ifndef NPRETTY_COLORS
#define PREDIFF  RED(BOLD)
#define POSTDIFF NORMAL
#else
#define PREDIFF
#define POSTDIFF "*"
#endif


/* Similar to dbm_printDiff print 2 vectors,
 * test diff and highlight it.
 */
std::ostream& debug_cppPrintDiffVectors(std::ostream &out,
                                        const int32_t *vec1,
                                        const int32_t *vec2, size_t size)
{
    size_t i;
    assert(size == 0 || (vec1 && vec2));

    out << '[';
    for(i = 0; i < size; ++i)
    {
        if (vec1[i] == vec2[i])
        {
            out << ' ' << vec1[i];
        }
        else
        {
            out << PREDIFF " " << vec1[i] << POSTDIFF;
        }
    }
    out << " ]\n[";
    for(i = 0; i < size; ++i)
    {
        if (vec1[i] == vec2[i])
        {
            out << ' ' << vec2[i];
        }
        else
        {
            out << PREDIFF " " << vec2[i] << POSTDIFF;
        }
    }
    return out << " ]\n";
}
void debug_printDiffVectors(FILE *out,
                            const int32_t *vec1,
                            const int32_t *vec2, size_t size)
{
    OUT2OS();
    debug_cppPrintDiffVectors(os, vec1, vec2, size);
}

/* Go through the bits and print the index of the
 * bits that are set.
 */
std::ostream& debug_cppPrintActiveSet(std::ostream& out, const uint32_t *bits, size_t intSize)
{
    uint32_t count32 = 0;
    out << '(' << intSize << "){";

    while(intSize)
    {
        uint32_t b, count;
        
        for(b = *bits++, count = count32; b != 0; b >>= 1, ++count)
        {
            if (b & 1) out << ' ' << count;
        }

        count32 += 32;
        intSize--;
    }

    return out << " }";
}

/* wrapper
 */
void debug_printActiveSet(FILE *out, const uint32_t *bits, size_t intSize)
{
    OUT2OS();
    debug_cppPrintActiveSet(os, bits, intSize);
}


/* Look for the end of string; then search backward for
 * module/tests names.
 */
const char* debug_shortName(const char *filename,
                            const char *test)
{
    const char *s = filename;
    assert(filename && test);

    /* go to end */
    while(*s) s++;

    /* last '/' */
    while(s > filename && s[-1] != '/') s--;
    if (s == filename) return s;
    
    /* module name or tests */
    --s;
    while(s > filename && s[-1] != '/') s--;
    if (s == filename ||
        strncmp(s, test, strlen(test)) != 0)
    {
        return s;
    }

    /* go back for module name */
    --s;
    while(s > filename && s[-1] != '/') s--;
    return s;
}


/* Algorithm:
 * - generate bits
 *   * if too many bits, reset them
 * - if bits left, set them
 */
void debug_generateBits(uint32_t *bits, size_t bitSize,
                        size_t nbits, BOOL bit1)
{
    size_t i;

    DODEBUG(size_t saveNbBits = nbits);
    assert(nbits <= 32*bitSize);

    if (!nbits)
    {
        base_resetBits(bits, bitSize);
        return;
    }

    for(i = 0 ; i < bitSize ; ++i)
    {
        uint32_t x = RAND() | bit1;
        size_t n = base_countBits(x);

        if (n > nbits)
        {
            /* too many bits set:
             * need to reset bits to 0
             */
            uint32_t nbResets = n - nbits;
            uint32_t mask = 0x80000000;

            while(nbResets)
            {
                if (x & mask) nbResets--;
                x &= ~mask;
                mask >>= 1;
            }
            n = nbits;
        }
        
        bits[i] = x; /* write random bits */

        assert(n <= nbits);
        nbits -= n;
    }

    if (nbits) /* unset bits left */
    {
        for(i = 0 ; nbits && i < bitSize ; ++i)
        {
            uint32_t mask = 1;
            while(mask && nbits)
            {
                if ((bits[i] & mask) == 0)
                {
                    bits[i] |= mask;
                    nbits--;
                }
                mask <<= 1;
            }
        }
    }

    assert(nbits == 0);
    assert(saveNbBits == base_countBitsN(bits, bitSize));
}


void debug_fixGeneratedBits(uint32_t *bits, size_t bitSize, size_t bitStringSize)
{
    size_t index = bitStringSize >> 5;
    uint32_t mask = 1 << (bitStringSize & 31);
    uint32_t *start = bits + index;
    uint32_t *end = bits + bitSize;

    ASSERT(base_countBitsN(bits, bitSize) <= bitStringSize,
           fprintf(stderr, "Number of bits(%d) > requested bitstring size(%d)\n",
                   base_countBitsN(bits, bitSize), bitStringSize));
    
    if (start < end && bitSize)
    {
        uint32_t nbReset = 0;
        uint32_t b = *start;

        /* fix current batch of 32 bits */
        while(mask)
        {
            if (b & mask) /* needs to be fixed */
            {
                nbReset++;
                b &= ~mask;
            }
            mask <<= 1;
        }
        *start = b;

        /* fix remaining ints */
        while(++start < end)
        {
            nbReset += base_countBits(*start);
            *start = 0;
        }

        if (nbReset == 0) return; /* nothing to fix */

        /* try to set unset bits == move bits to beginning */
        mask = 1 << (bitStringSize & 31); /* this is a forbidden position */
        mask >>= 1;                       /* this is an allowed position */
        start = bits + index;
        do {
            b = *start;
            while(mask)
            {
                if ((b & mask) == 0)
                {
                    b |= mask;
                    if (--nbReset == 0) /* fixed */
                    {
                        *start = b;
                        return;
                    }
                }
                mask >>= 1;
            }
            mask = 0x80000000; /* for next iteration */
            *start = b;
        } while(--start >= bits);

        /* we should not get there: forbidden by the initial assert */
        assert(0);
    }
}


/* Apply base_bits2indexTable but test
 * instead of setting values.
 */
BOOL debug_bits2indexTableOK(const uint32_t *bits, size_t n,
                             const uint32_t *table, size_t nbSet)
{
    size_t index = 0;
    DODEBUG(size_t extraCheck = n == 0 ? 0 :
            base_countBitsN(bits, n));
    assert(table && bits);

    while(n)
    {
        uint32_t b = *bits++;
        const uint32_t *t = table;
        table += 32;
        while(b)
        {
            if ((b & 1) && *t != index++)
            {
                return FALSE;
            }
            t++;
            b >>= 1;
        }
        --n;
    }
    assert(index == extraCheck);

    return (BOOL)(index == nbSet);
}


/* Find order of magnitude and proper
 * unit, compute remaining and print.
 */
std::ostream& debug_cppPrintMemory(std::ostream &out, size_t i)
{
    static const char* stringUnits[] =
    {
        "B",
        "kB",
        "MB",
        "GB",
        "TB"
    };
    size_t u = 0; /* unit index  */
    size_t j = i; /* main number */

    /* find order of magnitude */
    while(j > 999)
    {
        u++;
        j >>= 10;
    }

    /* remaining */
    if (u)
    {
        i = (i - (j << 10*u))
            >> 10*(u-1);
        i = (i + 50) / 100; /* keep only highest digit */

        return out << j << '.' << i << stringUnits[u];
    }
    else /* no remaining, i = 0 */
    {
        return out << j << stringUnits[u];
    }
}
void debug_printMemory(FILE *out, size_t i)
{
    OUT2OS();
    debug_cppPrintMemory(os, i);
}


/* Dummy class for a dummy test: test if
 * we are writing to a dumb terminal, such
 * as emacs' terminal.
 */
class DumbTerminal
{
public:
    DumbTerminal()
    {
        const char *s = getenv("TERM");
        dumb = s && strcmp(s,"dumb") == 0;
    }
    bool isDumb() const { return dumb; }
private:
    bool dumb;
};

static DumbTerminal termTest;

#if HAVE_UNISTD_H
#include <unistd.h>
#define IF_TTY if (!termTest.isDumb() && isatty(fileno(out)))
#else
#define IF_TTY if (!termTest.isDumb())
#endif

void debug_spin(FILE *out)
{
    static const char SPINBAR[4] = { '|', '/', '-', '\\' };
    static uint32_t counter = 0;
    IF_TTY {
        fprintf(out, "%c\b", SPINBAR[counter++ & 3]);
        fflush(out);
    }
}

