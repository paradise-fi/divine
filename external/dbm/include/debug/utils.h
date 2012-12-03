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
 * Filename : utils.h (debug)
 * C header.
 * 
 * Utility functions for debugging.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.3 reviewed.
 * $Id: utils.h,v 1.17 2005/05/11 19:08:14 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_DEBUG_UTILS_H
#define INCLUDE_DEBUG_UTILS_H

#include <stdio.h>
#include "base/inttypes.h"

#ifdef __cplusplus
#include <iostream>


/** Tabulation: print n spaces.
 * @param n: size of the tabulation.
 * @param out: output stream where to print (cerr, cout)
 */
static inline
std::ostream& debug_cpptab(std::ostream& os, size_t n)
{
    while(n) { os << ' '; --n; }
    return os;
}


/** Print bitstring, lower bits first.
 * @param s: start of the string.
 * @param n: size in int of the string to
 * @param out: output stream where to print (cerr,cout)
 * print. int[n] will be printed as bits.
 */
std::ostream& debug_cppPrintBitstring(std::ostream& out, const uint32_t *s, size_t n);


/** Print a matrix of bit.
 * @param out: output stream.
 * @param s,dim: bit matrix dimxdim.
 * @pre s is a uint32_t[bits2intsize(dim*dim)]
 */
std::ostream& debug_cppPrintBitMatrix(std::ostream& out, const uint32_t *s, cindex_t dim);


/** Print diff between bitstrings, lower bits first.
 * @param s1,s2: start of the strings.
 * @param n: size in int of the string to
 * @param out: output stream where to print (cerr,cout)
 * print. int[n] will be printed as bits.
 */
std::ostream& debug_cppPrintDiffBitstrings(std::ostream& out,
                                           const uint32_t *s1, const uint32_t *s2, size_t n);


/** Print bits, lower bits first.
 * @param i: the int to print.
 * @param out: output stream where to print (cerr,cout)
 */
std::ostream& debug_cppPrintBits(std::ostream& out, uint32_t i);


/** Print diff bits of i with j, lower bits first.
 * @param i,j: the ints to print.
 * @param out: output stream where to print (cerr,cout)
 */
std::ostream& debug_cppPrintDiffBits(std::ostream& out, uint32_t i, uint32_t j);


/** Print a vector of ints.
 * @param data: the vector of ints.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
std::ostream& debug_cppPrintVector(std::ostream& out, const int32_t *data, size_t size);


/** Print a vector of ints.
 * @param data: the vector of ints.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
std::ostream& debug_cppPrintRealVector(std::ostream& out, const double *data, size_t size);


/** Print 2 vectors and highlight
 * the difference between them.
 * @param vec1,vec2: the vectors to print.
 * @param size: size of the vectors.
 * @param out: where t print.
 * @pre vec1 and vec2 are int32_t[size]
 */
std::ostream& debug_cppPrintDiffVectors(std::ostream& out,
                                        const int32_t *vec1,
                                        const int32_t *vec2, size_t size);


/** Print memory quantity in human
 * readable format with B,MB,GB units.
 * @param mem: memory to print
 * @param out: where to print.
 */
std::ostream& debug_cppPrintMemory(std::ostream& out, size_t mem);


/** Print the set of active "things" according
 * to a bit vector that marks which ones are active.
 * @param out: where to print.
 * @param bits: bit array.
 * @param intSize: size in ints of the array.
 */
std::ostream& debug_cppPrintActiveSet(std::ostream& out, const uint32_t *bits, size_t intSize);

extern "C" {

#endif /* __cplusplus */


/** Randomize memory.
 * Write random numbers (rand()) in memory.
 * NOTE: one should call seed(something) to initialize
 * the random generator beforehand.
 * @param data: where to write.
 * @param intSize: size to write in int. Will
 * write int[intSize].
 * @post ((int*)data)[intSize] is randomized.
 */
void debug_randomize(void *data, size_t intSize);


/** Tabulation: print n spaces.
 * @param n: size of the tabulation.
 * @param out: output stream where to print (stderr,stdout)
 */
static inline
void debug_tab(FILE* out, size_t n)
{
    while(n) { fputc(' ', out); --n; }
}


/** Print bitstring, lower bits first.
 * @param s: start of the string.
 * @param n: size in int of the string to
 * @param out: output stream where to print (stderr,stdout)
 * print. int[n] will be printed as bits.
 */
void debug_printBitstring(FILE* out, const uint32_t *s, size_t n);


/** Print a bit matrix.
 * @param s,dim: bit matrix dimxdim.
 * @param out: output stream.
 * @pre s is a uint32_t[bits2intsize(dim*dim)]
 */
void debug_printBitMatrix(FILE* out, const uint32_t *s, cindex_t dim);


/** Print diff between bitstrings, lower bits first.
 * @param s1,s2: start of the strings.
 * @param n: size in int of the string to
 * @param out: output stream where to print (stderr,stdout)
 * print. int[n] will be printed as bits.
 */
void debug_printDiffBitstrings(FILE* out,
                               const uint32_t *s1, const uint32_t *s2, size_t n);


/** Print bits, lower bits first.
 * @param i: the int to print.
 * @param out: output stream where to print (stderr,stdout)
 */
void debug_printBits(FILE *out, uint32_t i);


/** Print diff bits of i with j, lower bits first.
 * @param i,j: the ints to print.
 * @param out: output stream where to print (stderr,stdout)
 */
void debug_printDiffBits(FILE *out, uint32_t i, uint32_t j);


/** Print a vector of ints.
 * @param data: the vector of ints.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
void debug_printVector(FILE *out, const int32_t *data, size_t size);


/** Print a vector of ints.
 * @param data: the vector of ints.
 * @param size: size of the vector.
 * @param out: where to print.
 * @pre data is a int32_t[size]
 */
void debug_printRealVector(FILE *out, const double *data, size_t size);


/** Print 2 vectors and highlight
 * the difference between them.
 * @param vec1,vec2: the vectors to print.
 * @param size: size of the vectors.
 * @param out: where t print.
 * @pre vec1 and vec2 are int32_t[size]
 */
void debug_printDiffVectors(FILE *out,
                            const int32_t *vec1,
                            const int32_t *vec2, size_t size);

/** Print the set of active "things" according
 * to a bit vector that marks which ones are active.
 * @param out: where to print.
 * @param bits: bit array.
 * @param intSize: size in ints of the array.
 */
void debug_printActiveSet(FILE *out, const uint32_t *bits, size_t intSize);

/** Return sub-string of filename.
 * No allocation, it is just a pointer offset.
 * @param filename: filename to truncate.
 * @param test: test directory name (typically "tests/")
 * NOTE: the test is done as a strncmp(xx, test, strlen(test))
 * so if '/' is ommitted, then we test for the beginning of
 * the name only.
 * @pre filename != NULL && test != NULL
 * @return pointer in filename to truncate from
 * absolute path to module path.
 * e.g.:
 * "somewhere/hop/foo.c" -> "hop/foo.c"
 * "somewhere/tests/foo.c" -> "somewhere/tests/foo.c"
 * "long/path/here/foo.cpp" -> "here/foo.cpp"
 */
const char* debug_shortName(const char *filename,
                            const char *test);

static inline
const char* debug_shortSource(const char *filename)
{
    return debug_shortName(filename, "tests/");
}


/** Generate random bits. To the diffence of
 * randomize, this function allows specifying
 * the number of bits we want to be set.
 * @param bits: bit string to write.
 * @param bitSize: size in int of the bit string.
 * @param nbits: nb of bits to set.
 * @param bit1: to force the 1st bit to be set,
 * if == 0, does not force anything, if == 1
 * force 1st bit to 1.
 * @pre:
 * - nbits <= 32*bitSize otherwise it is not
 *   possible to meet the specification
 * - bits is a uint32_t[bitSize]
 */
void debug_generateBits(uint32_t *bits, size_t bitSize,
                        size_t nbits, BOOL bit1);



/** Fix generated bits (typically by debug_generateBits)
 * so that the index of the highest bit < bitStringSize
 * @param bits,bitSize: bitstring of size bitSize (ints)
 * @param bitStringSize: size in bits of the bitstring
 * @pre bitStringSize <= number of bits to keep == 1, otherwise
 * bits will be lost.
 */
void debug_fixGeneratedBits(uint32_t *bits, size_t bitSize, size_t bitStringSize);


/** Test if a the unpacking of a bit table
 * gives the given index table as a result.
 * @param table: index redirection table
 * @param nbSet: number of used indices
 * (check that = number of bits in 'bits')
 * @param bits: bit array
 * @param n: size in int of the bit array
 * @return true if table matches bits.
 * @pre
 * - table is large enough: at least
 *   uint32_t[number of bits read == n*32]
 * - bits is at least a uint32_t[n]
 */
BOOL debug_bits2indexTableOK(const uint32_t *bits, size_t n,
                             const uint32_t *table, size_t nbSet);


/** Print memory quantity in human
 * readable format with B,MB,GB units.
 * @param mem: memory to print
 * @param out: where to print.
 */
void debug_printMemory(FILE *out, size_t mem);


/** Print a spinning bar on a given output.
 * Successive calls will make the bar spin.
 * @param out: stream for output.
 */
void debug_spin(FILE *out);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DEBUG_UTILS_H */
