/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_interactive
#define INC_interactive

#include "nipsvm.h"

#ifdef __cplusplus
extern "C"
{
#endif

// do an interactive simulation stating from the initial state
// random: boolean flag if to replace interactivity by randomness
// rnd_quiet: if to suppress state output during random simulation
// print_hex: boolean flag if to print current states in hexadecimal
// split: boolean flag if to split up and reassemble states
// buffer_len: size of state buffer
// states_max: number of state pointer buffer entries
// ini_state: state to start simulation at (or NULL to use default)
extern void interactive_simulate( st_bytecode *bytecode, int random, int rnd_quiet, int print_hex, int split,
                                  unsigned long buffer_len, unsigned int states_max,
                                  st_global_state_header *ini_state );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_interactive
