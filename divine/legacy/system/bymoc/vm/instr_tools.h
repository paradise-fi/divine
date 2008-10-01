/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_instr_tools
#define INC_instr_tools


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>

#include "bytecode.h"
#include "instr.h"
#include "state.h"

// convert output list to string
// *pp_buf: pointer into buffer, advanced by this function
// *p_buf_len: remaining length of buffer, decreased by this function
// returns pointer to 0-terminated string in buffer or NULL in case of error
extern char * instr_output_to_str( st_bytecode *bytecode, st_instr_output *p_output,
                                   char **p_buf, unsigned long *p_buf_len );


// convert output during successor state generation to string
// *pp_buf: pointer into buffer, advanced by this function
// *p_buf_len: remaining length of buffer, decreased by this function
// returns pointer to 0-terminated string in buffer or NULL in case of error
extern char * instr_succ_output_to_str( st_bytecode *bytecode, st_instr_succ_output *p_succ_out,
                                        char **p_buf, unsigned long *p_buf_len );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_instr_tools
