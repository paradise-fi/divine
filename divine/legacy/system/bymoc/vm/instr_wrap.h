/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_instr_wrap
#define INC_instr_wrap


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>

#include "bytecode.h"
#include "instr.h"
#include "state.h"


// successor state structure for instr_succ_buf_ex
typedef struct t_instr_succ_buf_ex_state
{
  st_global_state_header *p_state; // the successor state
  uint8_t label_1st; // label of STEP command of 1st part of sync. comm. (unused if ! sync)
  uint8_t label; // label of STEP command (of 2nd part of sync. comm. if sync)
  t_flag_reg flag_reg_1st; // flag register value returned by 1st part of sync. comm. (unused if ! sync)
  t_flag_reg flag_reg; // flag register value returned (by 2nd part of sync. comm. if sync)
  unsigned int succ_cb_flags; // boolean flags (if sync. comm. took place, if timeout occured, ...)
  char *p_output; // pointer to 0-terminated output string (if process_output in instr_succ_buf_ex call was != 0)
} st_instr_succ_buf_ex_state;


// initialize successor state generation context for instr_succ_buf
// must be called before instr_succ_buf (only 1 time, not every time)
extern void instr_succ_buf_prepare( st_instr_succ_context *succ_ctx,
                                    st_bytecode *bytecode );


// generate successor states in a buffer
// pointers to states are put into array pointed to by p_succ
// *pp_buf points to memory area of length *p_len to use for successor states
// returns number of successor states
extern unsigned long instr_succ_buf( st_instr_succ_context *succ_ctx, // must be initialized with instr_succ_buf_prepare
                                     st_global_state_header *p_glob, // state to start with
                                     st_global_state_header **p_succ, unsigned long succ_max,
                                     char **pp_buf, unsigned long *p_buf_len );


// initialize successor state generation context for instr_succ_buf_ex
// must be called before instr_succ_buf_ex (only 1 time, not every time)
extern void instr_succ_buf_ex_prepare( st_instr_succ_context *succ_ctx,
                                       st_bytecode *bytecode );


// generate successor states and extended information in a buffer
// state structure array succ is filled with results
// *pp_buf points to memory area of length *p_len to use for successor states and output strings
// returns number of successor states
extern unsigned long instr_succ_buf_ex( st_instr_succ_context *succ_ctx, // must be initialized with instr_succ_buf_ex_prepare
                                        st_global_state_header *p_glob, // state to start with
                                        t_flag_reg flag_reg, // value to put into flag register
                                        int process_output, // if to process output (from print instructions)
                                        st_instr_succ_buf_ex_state *succ, unsigned long succ_max,
                                        char **pp_buf, unsigned long *p_buf_len );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_instr_wrap
