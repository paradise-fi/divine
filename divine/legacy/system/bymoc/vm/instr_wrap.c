/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "instr.h"
#include "instr_step.h"
#include "instr_tools.h"
#include "instr_wrap.h"
#include "state.h"

#include "nipsvm.h"

// *** helper functions ***

// private context for instr_succ_buf
typedef struct t_instr_succ_buf_ctx
{
  unsigned long succ_cnt;
  st_global_state_header **p_succ;
  unsigned long succ_max;
  char **pp_buf;
  unsigned long *p_buf_len;
} st_instr_succ_buf_ctx;


// successor callback function for instr_succ_buf
et_ic_status instr_succ_buf_succ_cb( st_global_state_header *succ,
                                     uint8_t label_1st, uint8_t label,
                                     t_flag_reg flag_reg_1st, t_flag_reg flag_reg,
                                     unsigned int succ_cb_flags,
                                     st_instr_succ_output *p_succ_out,
                                     void *priv_context )
{
  st_instr_succ_buf_ctx *p_ctx = (st_instr_succ_buf_ctx *)priv_context;
  // copy successor state into buffer and save pointer to it
  if( p_ctx->succ_cnt >= p_ctx->succ_max )
  {
    fprintf( stderr, "RUNTIME ERROR: too many successor states (try \"-s\")\n" );
    return IC_STOP;
  }
  p_ctx->p_succ[p_ctx->succ_cnt] = global_state_copy( succ, p_ctx->pp_buf, p_ctx->p_buf_len );
  if( p_ctx->p_succ[p_ctx->succ_cnt] == NULL )
  {
    fprintf( stderr, "RUNTIME ERROR: out of state memory (try \"-b\")\n" );
    return IC_STOP;
  }
  p_ctx->succ_cnt++;
  // go on finding successor states
  return IC_CONTINUE;
  // keep compiler happy
  label_1st = 0;
  label = 0;
  flag_reg_1st = 0;
  flag_reg = 0;
  succ_cb_flags = 0;
  p_succ_out = NULL;
}


// private context for instr_succ_buf_ex
typedef struct t_instr_succ_buf_ex_ctx
{
  unsigned long succ_cnt;
  st_instr_succ_buf_ex_state *succ;
  unsigned long succ_max;
  char **pp_buf;
  unsigned long *p_buf_len;
  int process_output;
  st_bytecode *bytecode;
} st_instr_succ_buf_ex_ctx;


// successor callback function for instr_succ_buf_ex
et_ic_status instr_succ_buf_ex_succ_cb( st_global_state_header *succ,
                                        uint8_t label_1st, uint8_t label,
                                        t_flag_reg flag_reg_1st, t_flag_reg flag_reg,
                                        unsigned int succ_cb_flags,
                                        st_instr_succ_output *p_succ_out,
                                        void *priv_context )
{
  st_instr_succ_buf_ex_ctx *p_ctx = (st_instr_succ_buf_ex_ctx *)priv_context;
  // copy successor state into buffer and save pointer to it
  if( p_ctx->succ_cnt >= p_ctx->succ_max )
  {
    fprintf( stderr, "RUNTIME ERROR: too many successor states (try \"-s\")\n" );
    return IC_STOP;
  }
  p_ctx->succ[p_ctx->succ_cnt].p_state = global_state_copy( succ, p_ctx->pp_buf, p_ctx->p_buf_len ); // save successor state
  if( p_ctx->succ[p_ctx->succ_cnt].p_state == NULL )
  {
    fprintf( stderr, "RUNTIME ERROR: out of state memory (try \"-b\")\n" );
    return IC_STOP;
  }
  p_ctx->succ[p_ctx->succ_cnt].label_1st = label_1st; // save additional information
  p_ctx->succ[p_ctx->succ_cnt].label = label;
  p_ctx->succ[p_ctx->succ_cnt].flag_reg_1st = flag_reg_1st;
  p_ctx->succ[p_ctx->succ_cnt].flag_reg = flag_reg;
  p_ctx->succ[p_ctx->succ_cnt].succ_cb_flags = succ_cb_flags;
  if( p_ctx->process_output ) // save output string
  {
    p_ctx->succ[p_ctx->succ_cnt].p_output = instr_succ_output_to_str( p_ctx->bytecode, p_succ_out,
                                                                      p_ctx->pp_buf, p_ctx->p_buf_len );
    if( p_ctx->succ[p_ctx->succ_cnt].p_output == NULL )
    {
      fprintf( stderr, "RUNTIME ERROR: out of state memory (try \"-b\")\n" );
      return IC_STOP;
    }
  }
  else // do not save output string
    p_ctx->succ[p_ctx->succ_cnt].p_output = NULL;
  p_ctx->succ_cnt++;
  // go on finding successor states
  return IC_CONTINUE;
}


// *** functions visible from outside ***


// initialize successor state generation context for instr_succ_buf
// must be called before instr_succ_buf (only 1 time, not every time)
void instr_succ_buf_prepare( st_instr_succ_context *succ_ctx,
                             st_bytecode *bytecode ) // extern
{
  // set default maximum values and bytecode
  instr_succ_context_default( succ_ctx, bytecode );

  // set callbacks
  succ_ctx->succ_cb = instr_succ_buf_succ_cb;
  succ_ctx->err_cb = nipsvm_default_error_cb;
}


// generate successor states in a buffer
// pointers to states are put into array pointed to by p_succ
// *pp_buf points to memory area of length *p_len to use for successor states
// returns number of successor states
unsigned long instr_succ_buf( st_instr_succ_context *succ_ctx, // must be initialized with instr_succ_buf_prepare
                              st_global_state_header *p_glob, // state to start with
                              st_global_state_header **p_succ, unsigned long succ_max,
                              char **pp_buf, unsigned long *p_buf_len ) // extern
{
  // set up private context
  st_instr_succ_buf_ctx ctx =
  {
    .succ_cnt = 0,
    .p_succ = p_succ,
    .succ_max = succ_max,
    .pp_buf = pp_buf,
    .p_buf_len = p_buf_len
  };
  succ_ctx->priv_context = &ctx; // put it into successor state generation context

  // generate successor states - using normal successor function
  instr_succ( p_glob, 0, succ_ctx );

  // return number of successor states
  return ctx.succ_cnt;
}


// initialize successor state generation context for instr_succ_buf_ex
// must be called before instr_succ_buf_ex (only 1 time, not every time)
void instr_succ_buf_ex_prepare( st_instr_succ_context *succ_ctx,
                                st_bytecode *bytecode ) // extern
{
  // set default maximum values and bytecode
  instr_succ_context_default( succ_ctx, bytecode );

  // set callbacks
  succ_ctx->succ_cb = instr_succ_buf_ex_succ_cb;
  succ_ctx->err_cb = nipsvm_default_error_cb;
}


// generate successor states and extended information in a buffer
// state structure array succ is filled with results
// *pp_buf points to memory area of length *p_len to use for successor states and output strings
// returns number of successor states
unsigned long instr_succ_buf_ex( st_instr_succ_context *succ_ctx, // must be initialized with instr_succ_buf_ex_prepare
                                 st_global_state_header *p_glob, // state to start with
                                 t_flag_reg flag_reg, // value to put into flag register
                                 int process_output, // if to process output (from print instructions)
                                 st_instr_succ_buf_ex_state *succ, unsigned long succ_max,
                                 char **pp_buf, unsigned long *p_buf_len ) // extern
{
  // set up private context
  st_instr_succ_buf_ex_ctx ctx =
  {
    .succ_cnt = 0,
    .succ = succ,
    .succ_max = succ_max,
    .pp_buf = pp_buf,
    .p_buf_len = p_buf_len,
    .process_output = process_output,
    .bytecode = succ_ctx->bytecode
  };
  succ_ctx->priv_context = &ctx; // put it into successor state generation context

  // generate successor states - using normal successor function
  instr_succ( p_glob, flag_reg, succ_ctx );

  // return number of successor states
  return ctx.succ_cnt;
}

