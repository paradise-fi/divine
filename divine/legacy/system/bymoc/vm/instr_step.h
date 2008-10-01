/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_instr_step
#define INC_instr_step


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>

#include "bytecode.h"
#include "instr.h"
#include "state.h"


// initialize context with default maximum values
// only fills in maximum values and bytecode
// does not touch pointers to callback functions
extern void instr_succ_context_default( st_instr_succ_context *succ_ctx, st_bytecode *bytecode );


// generate successor states
// callbacks in context are called for every event (e.g. a successor state)
// returns number of successor states that were reported to successor callback
extern unsigned int instr_succ( st_global_state_header *p_glob, // state to start with
                                t_flag_reg flag_reg, // value to put into flag register
                                st_instr_succ_context *succ_ctx );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_instr_step
