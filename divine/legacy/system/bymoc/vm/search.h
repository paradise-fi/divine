/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_search
#define INC_search


#include "nipsvm.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

// do a depth-first or breadth-first search in state space up to specified depth
// bfs: boolean flag if to do a breadth-first seach (depth_max is ignored in this case)
// buffer_len: size of state buffer
// hash_entries, hash_retries: parameters of hash table to finf duplicate states
// graph_out: if != NULL state graph will be output to this tream in graphviz dot format
// print_hex: if to print states in hex
// ini_state: state to start simulation at (or NULL to use default)
extern void search( nipsvm_bytecode_t *bytecode, int bfs, unsigned int depth_max, unsigned long buffer_len,
                    unsigned long hash_entries, unsigned long hash_retries, FILE *graph_out, int print_hex,
                    st_global_state_header *ini_state );


#ifdef __cplusplus
} // extern "C"
#endif

#endif // #ifndef INC_search
