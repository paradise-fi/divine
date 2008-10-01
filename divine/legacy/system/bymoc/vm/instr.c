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
#include "tools.h"
#include "instr.h"

#include "nipsvm.h"

// #define KEEP_TERMINATED // do not delete terminated processes from state
// #define UNSAFE // do not check memory addresses, stack overflow/underflow, ...
// #define TRUNC_ERR // generate IE_OV if truncation modifies value
// #define DEBUG_INSTR // print state before and after every instruction


// *** helper functions ***


// process runtime error
static void instr_err( st_instr_context *p_ctx, et_instr_err err )
{
  et_ic_status status;
  // inform caller of error
  if( p_ctx->mode != INSTR_CONTEXT_MODE_STOP )
    status = p_ctx->p_succ_ctx->err_cb( err,
                                        p_ctx->p_proc == NULL ? 0 : be2h_pid( p_ctx->p_proc->proc.pid ),
                                        p_ctx->p_proc == NULL ? 0 : be2h_pc( p_ctx->p_proc->proc.pc ),
                                        p_ctx->p_succ_ctx->priv_context );
  else // already stopped - this may happen if multiple checks are in one instruction
    status = IC_STOP;

  // cancel this execution path
  p_ctx->mode = INSTR_CONTEXT_MODE_INVALID; // mark current context as invalid
  // do not set pointers in context to NULL here,
  // because some operations (stack_pop, local_set) might be executed even after this error
  // the reason for this are calls like: local_set( stack_pop( ) )
  
  // set context mode to stop if callback requested stop
  if( status == IC_STOP )
    p_ctx->mode = INSTR_CONTEXT_MODE_STOP;
}


// conditional runtime error
#define instr_err_if( condition, p_ctx, err, ret_val ) { if( condition ) { instr_err( p_ctx, err ); return ret_val; } }
// conditional runtime error not checked in UNSAFE mode
//  - used for all errors that cannot happen when bytecode (and VM source code) is correct
#ifdef UNSAFE
  #define instr_err_if_u( condition, p_ctx, err, ret_val )
#else
  #define instr_err_if_u( condition, p_ctx, err, ret_val ) instr_err_if( condition, p_ctx, err, ret_val )
#endif


// fetch a byte from bytecode
static inline int8_t instr_bytecode_get1( st_instr_context *p_ctx )
{
  t_pc pc_h = be2h_pc( p_ctx->p_proc->proc.pc );
  int8_t value;
  // check that whole byte is within bytecode
  instr_err_if_u( pc_h + 1 > p_ctx->p_succ_ctx->bytecode->size, p_ctx, IE_BYTECODE, 0 );
  // return int from bytecode
  value = *(int8_t *)(p_ctx->p_succ_ctx->bytecode->ptr + pc_h);
  p_ctx->p_proc->proc.pc = h2be_pc( pc_h + 1 );
  return value;
}


// fetch a word from bytecode
static inline int16_t instr_bytecode_get2( st_instr_context *p_ctx )
{
  t_pc pc_h = be2h_pc( p_ctx->p_proc->proc.pc );
  int16_t value;
  // check that whole word is within bytecode
  instr_err_if_u( pc_h + 2 > p_ctx->p_succ_ctx->bytecode->size, p_ctx, IE_BYTECODE, 0 );
  // return int from bytecode
  value = ua_rd_16( p_ctx->p_succ_ctx->bytecode->ptr + pc_h );
  p_ctx->p_proc->proc.pc = h2be_pc( pc_h + 2 );
  return be2h_16( value );
}


// fetch a double-word from bytecode
static inline int32_t instr_bytecode_get4( st_instr_context *p_ctx )
{
  t_pc pc_h = be2h_pc( p_ctx->p_proc->proc.pc );
  int32_t value;
  // check that whole double-word is within bytecode
  instr_err_if_u( pc_h + 4 > p_ctx->p_succ_ctx->bytecode->size, p_ctx, IE_BYTECODE, 0 );
  // return int from bytecode
  value = ua_rd_32( p_ctx->p_succ_ctx->bytecode->ptr + pc_h );
  p_ctx->p_proc->proc.pc = h2be_pc( pc_h + 4 );
  return be2h_32( value );
}


// fetch a byte from local variables
static inline int8_t instr_local_get1( st_instr_context *p_ctx, t_val addr )
{
  // check that whole byte is within variables
  instr_err_if_u( addr < 0 || addr + 1 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, 0 );
  // return byte
  return *(int8_t *)(p_ctx->p_lvar + addr);
}


// fetch a word from local variables
static inline int16_t instr_local_get2( st_instr_context *p_ctx, t_val addr )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, 0 );
  // return word
  return be2h_16( ua_rd_16( p_ctx->p_lvar + addr ) );
}
// little endian version
static inline int16_t instr_local_get2_le( st_instr_context *p_ctx, t_val addr )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, 0 );
  // return word (convert from little endian)
  return le2h_16( ua_rd_16( p_ctx->p_lvar + addr ) );
}


// fetch a double-word from local variables
static inline int32_t instr_local_get4( st_instr_context *p_ctx, t_val addr )
{
  // check that whole double-word is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, 0 );
  // return double-word
  return be2h_32( ua_rd_32( p_ctx->p_lvar + addr ) );
}
// little endian version
static inline int32_t instr_local_get4_le( st_instr_context *p_ctx, t_val addr )
{
  // check that whole double-word is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, 0 );
  // return double-word (convert from little endian)
  return le2h_32( ua_rd_32( p_ctx->p_lvar + addr ) );
}


// fetch a byte from global variables
static inline int8_t instr_global_get1( st_instr_context *p_ctx, t_val addr )
{
  // check that whole byte is within variables
  instr_err_if_u( addr < 0 || addr + 1 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, 0 );
  // return byte
  return *(int8_t *)(p_ctx->p_gvar + addr);
}


// fetch a word from global variables
static inline int16_t instr_global_get2( st_instr_context *p_ctx, t_val addr )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, 0 );
  // return word
  return be2h_16( ua_rd_16( p_ctx->p_gvar + addr ) );
}
// little endian version
static inline int16_t instr_global_get2_le( st_instr_context *p_ctx, t_val addr )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, 0 );
  // return word (convert from little endian)
  return le2h_16( ua_rd_16( p_ctx->p_gvar + addr ) );
}


// fetch a double-word from global variables
static inline int32_t instr_global_get4( st_instr_context *p_ctx, t_val addr )
{
  // check that whole double-word is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, 0 );
  // return double-word
  return be2h_32( ua_rd_32( p_ctx->p_gvar + addr ) );
}
// little endian version
static inline int32_t instr_global_get4_le( st_instr_context *p_ctx, t_val addr )
{
  // check that whole double-word is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, 0 );
  // return double-word (convert from little endian)
  return le2h_32( ua_rd_32( p_ctx->p_gvar + addr ) );
}


// put a byte into local variables
static inline void instr_local_put1( st_instr_context *p_ctx, t_val addr, int8_t value )
{
  // check that whole byte is within variables
  instr_err_if_u( addr < 0 || addr + 1 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, );
  // store byte
  *(int8_t *)(p_ctx->p_lvar + addr) = value;
}


// put a word into local variables
static inline void instr_local_put2( st_instr_context *p_ctx, t_val addr, int16_t value )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, );
  // store word
  ua_wr_16( p_ctx->p_lvar + addr, h2be_16( value ) );
}
// little endian version
static inline void instr_local_put2_le( st_instr_context *p_ctx, t_val addr, int16_t value )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, );
  // store word (convert into little endian)
  ua_wr_16( p_ctx->p_lvar + addr, h2le_16( value ) );
}


// put a double-word into local variables
static inline void instr_local_put4( st_instr_context *p_ctx, t_val addr, int32_t value )
{
  // check that whole double-byte is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, );
  // store double-word
  ua_wr_32( p_ctx->p_lvar + addr, h2be_32( value ) );
}
// little endian version
static inline void instr_local_put4_le( st_instr_context *p_ctx, t_val addr, int32_t value )
{
  // check that whole double-byte is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->lvar_sz, p_ctx, IE_LOCAL, );
  // store double-word (convert into little endian)
  ua_wr_32( p_ctx->p_lvar + addr, h2le_32( value ) );
}


// put a byte into global variables
static inline void instr_global_put1( st_instr_context *p_ctx, t_val addr, int8_t value )
{
  // check that whole byte is within variables
  instr_err_if_u( addr < 0 || addr + 1 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, );
  // store byte
  *(int8_t *)(p_ctx->p_gvar + addr) = value;
}


// put a word into global variables
static inline void instr_global_put2( st_instr_context *p_ctx, t_val addr, int16_t value )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, );
  // store word
  ua_wr_16( p_ctx->p_gvar + addr, h2be_16( value ) );
}
// little endian version
static inline void instr_global_put2_le( st_instr_context *p_ctx, t_val addr, int16_t value )
{
  // check that whole word is within variables
  instr_err_if_u( addr < 0 || addr + 2 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, );
  // store word (convert into little endian)
  ua_wr_16( p_ctx->p_gvar + addr, h2le_16( value ) );
}


// put a double-word into global variables
static inline void instr_global_put4( st_instr_context *p_ctx, t_val addr, int32_t value )
{
  // check that whole double-byte is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, );
  // store double-word
  ua_wr_32( p_ctx->p_gvar + addr, h2be_32( value ) );
}
// little endian version
static inline void instr_global_put4_le( st_instr_context *p_ctx, t_val addr, int32_t value )
{
  // check that whole double-byte is within variables
  instr_err_if_u( addr < 0 || addr + 4 > p_ctx->gvar_sz, p_ctx, IE_GLOBAL, );
  // store double-word (convert into little endian)
  ua_wr_32( p_ctx->p_gvar + addr, h2le_32( value ) );
}


// push a value onto the stack
static inline void instr_stack_push( st_instr_context *p_ctx, t_val value )
{
  // check that there is space for an entry on the stack
  instr_err_if_u( p_ctx->p_proc->stack_cur >= p_ctx->p_proc->stack_max, p_ctx, IE_STACKOV, );
  // place value onto stack
  p_ctx->p_stack[p_ctx->p_proc->stack_cur++].val = value;
}


// pop a value from the stack
static inline t_val instr_stack_pop( st_instr_context *p_ctx )
{
  // check that there is an entry on the stack
  instr_err_if_u( p_ctx->p_proc->stack_cur <= 0, p_ctx, IE_STACKUN, 0 );
  // fetch value from stack (and clear it)
  t_val value = p_ctx->p_stack[--p_ctx->p_proc->stack_cur].val;
  p_ctx->p_stack[p_ctx->p_proc->stack_cur].val = 0;
  return value;
}


// get topmost value from the stack without modifying stack
static inline t_val instr_stack_top( st_instr_context *p_ctx )
{
  // check that there is an entry on the stack
  instr_err_if_u( p_ctx->p_proc->stack_cur <= 0, p_ctx, IE_STACKUN, 0 );
  // return topmost value from stack
  return p_ctx->p_stack[p_ctx->p_proc->stack_cur - 1].val;
}


// add global state to stack of states to process
// normally: max_step_cnt = (unsigned int)-1
// state is not processed if state_cnt (at time whan state shall be processed) if greater than max_step_cnt
// - used in ELSE and UNLESS to ignore second path if another step has been completed
static inline void instr_push_state( st_instr_context *p_ctx,
                                     st_global_state_header *p_glob,
                                     st_instr_output *p_output,
                                     unsigned int max_step_cnt )
{
  // check for space
  instr_err_if( p_ctx->state_cnt >= p_ctx->state_cnt_max, p_ctx, IE_PATH_CNT, );
  // put state onto stack
  p_ctx->p_states[p_ctx->state_cnt].p_glob = p_glob;
  p_ctx->p_states[p_ctx->state_cnt].p_output = p_output;
  p_ctx->p_states[p_ctx->state_cnt].max_step_cnt = max_step_cnt;
  p_ctx->state_cnt++;
}


// *** instructions ***


// LDC c
static inline void instr_ldc( st_instr_context *p_ctx )
{
  // load constant from bytecode onto stack
  instr_stack_push( p_ctx, instr_bytecode_get4( p_ctx ) );
}


// LDV L
static inline void instr_ldv_l1u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint8_t)instr_local_get1( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l1s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_local_get1( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l2u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_local_get2( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l2s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_local_get2( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l4( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_local_get4( p_ctx, instr_stack_pop( p_ctx ) ) );
}


// LDV G
static inline void instr_ldv_g1u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint8_t)instr_global_get1( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g1s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_global_get1( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g2u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_global_get2( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g2s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_u_val)instr_global_get2( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g4( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_global_get4( p_ctx, instr_stack_pop( p_ctx ) ) );
}


// STV L
static inline void instr_stv_l1u( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put1( p_ctx, addr, (int8_t)(uint8_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l1s( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put1( p_ctx, addr, (int8_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l2u( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put2( p_ctx, addr, (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l2s( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put2( p_ctx, addr, (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l4( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put4( p_ctx, addr, instr_stack_pop( p_ctx ) );
}


// STV G
static inline void instr_stv_g1u( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put1( p_ctx, addr, (int8_t)(uint8_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g1s( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put1( p_ctx, addr, (int8_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g2u( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put2( p_ctx, addr, (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g2s( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put2( p_ctx, addr, (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g4( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put4( p_ctx, addr, instr_stack_pop( p_ctx ) );
}


// TRUNC b
static inline void instr_trunc( st_instr_context *p_ctx )
{
  int8_t bits = instr_bytecode_get1( p_ctx );
  t_val value = instr_stack_pop( p_ctx );
#ifdef TRUNC_ERR
  t_val value_orig = value;
#endif
  if( bits >= 0 )
    value &= (1 << bits) - 1;
  else if( value >= 0 )
    value &= (1 << -bits) - 1;
  else
    value = -(-value & ((1 << -bits) - 1));
#ifdef TRUNC_ERR
  if( value_orig != value )
    instr_err( p_ctx, IE_OV );
#endif
  instr_stack_push( p_ctx, value );
}


// LDS timeout
static inline void instr_lds_timeout( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, p_ctx->timeout ? 1 : 0 );
}


// LDS pid
static inline void instr_lds_pid( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, be2h_pid( p_ctx->p_proc->proc.pid ) );
}

// LDS nrpr
static inline void instr_lds_nrpr( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, global_state_count_enabled_processes( p_ctx->p_glob ) );
}


// LDS last
static inline void instr_lds_last( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, p_ctx->last_pid );
}


// LDS np
static inline void instr_lds_np( st_instr_context *p_ctx )
{
  // search for a process at a progress state
  char *ptr = global_state_get_processes( p_ctx->p_glob );
  unsigned int i;
  for( i = 0; i < p_ctx->p_glob->proc_cnt; i++ )
  {
    if( bytecode_flags( p_ctx->p_succ_ctx->bytecode, be2h_pc( ((st_process_header *)ptr)->pc ) ) & BC_FLAG_PROGRESS )
      break;
    ptr += process_size( (st_process_header *)ptr );
  }
  // "non progress" if loop was completed
  instr_stack_push( p_ctx, i < p_ctx->p_glob->proc_cnt ? 0 : 1 );
}


// ADD
static inline void instr_add( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a + b );
}


// SUB
static inline void instr_sub( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a - b );
}


// MUL
static inline void instr_mul( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a * b );
}


// DIV
static inline void instr_div( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_err_if( b == 0, p_ctx, IE_DIV0, );
  instr_stack_push( p_ctx, a / b );
}


// MOD
static inline void instr_mod( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_err_if( b == 0, p_ctx, IE_DIV0, );
  instr_stack_push( p_ctx, a % b );
}


// NEG
static inline void instr_neg( st_instr_context *p_ctx )
{
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, -a );
}


// NOT
static inline void instr_not( st_instr_context *p_ctx )
{
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, ~a );
}


// AND
static inline void instr_and( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a & b );
}


// OR
static inline void instr_or( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a | b );
}


// XOR
static inline void instr_xor( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a ^ b );
}


// SHL
static inline void instr_shl( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a << b );
}


// SHR
static inline void instr_shr( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a >> b );
}


// EQ
static inline void instr_eq( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a == b );
}


// NEQ
static inline void instr_neq( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a != b );
}


// LT
static inline void instr_lt( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a < b );
}


// LTE
static inline void instr_lte( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a <= b );
}


// GT
static inline void instr_gt( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a > b );
}


// GTE
static inline void instr_gte( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a >= b );
}


// BNOT
static inline void instr_bnot( st_instr_context *p_ctx )
{
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, !a );
}


// BAND
static inline void instr_band( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a && b );
}


// BOR
static inline void instr_bor( st_instr_context *p_ctx )
{
  t_val b = instr_stack_pop( p_ctx );
  t_val a = instr_stack_pop( p_ctx );
  instr_stack_push( p_ctx, a || b );
}


// ICHK n
static inline void instr_ichk( st_instr_context *p_ctx )
{
  uint8_t n = (uint8_t)instr_bytecode_get1( p_ctx );
  t_val i = instr_stack_top( p_ctx );
  instr_err_if( i < 0 || i >= (t_val)n, p_ctx, IE_INDEX, );
}


// BCHK
static inline void instr_bchk( st_instr_context *p_ctx )
{
  instr_err_if( ! instr_stack_pop( p_ctx ), p_ctx, IE_ASSERT, );
}


// JMP a
static inline void instr_jmp( st_instr_context *p_ctx )
{
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  p_ctx->p_proc->proc.pc = h2be_pc(   be2h_pc( p_ctx->p_proc->proc.pc )
                                    + rel_addr );
}


// JMPZ a
static inline void instr_jmpz( st_instr_context *p_ctx )
{
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  if( instr_stack_pop( p_ctx ) == 0 )
    p_ctx->p_proc->proc.pc = h2be_pc(   be2h_pc( p_ctx->p_proc->proc.pc )
                                      + rel_addr );
}


// JMPNZ a
static inline void instr_jmpnz( st_instr_context *p_ctx )
{
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  if( instr_stack_pop( p_ctx ) != 0 )
    p_ctx->p_proc->proc.pc = h2be_pc(   be2h_pc( p_ctx->p_proc->proc.pc )
                                      + rel_addr );
}


// LJMP a
static inline void instr_ljmp( st_instr_context *p_ctx )
{
  t_val addr = instr_bytecode_get4( p_ctx );
  p_ctx->p_proc->proc.pc = h2be_pc( addr );
}


// TOP r_i
static inline void instr_top( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  p_ctx->p_proc->registers[reg] = instr_stack_top( p_ctx );
}


// POP r_i
static inline void instr_pop( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  p_ctx->p_proc->registers[reg] = instr_stack_pop( p_ctx );
}


// PUSH r_i
static inline void instr_push( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  instr_stack_push( p_ctx, p_ctx->p_proc->registers[reg] );
}


// POPX
static inline void instr_popx( st_instr_context *p_ctx )
{
  instr_stack_pop( p_ctx );
}


// INC r_i
static inline void instr_inc( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  p_ctx->p_proc->registers[reg]++;
}


// DEC r_i
static inline void instr_dec( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  p_ctx->p_proc->registers[reg]--;
}


// LOOP r_i, a
static inline void instr_loop( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  p_ctx->p_proc->registers[reg]--;
  if( p_ctx->p_proc->registers[reg] > 0 )
    p_ctx->p_proc->proc.pc = h2be_pc(   be2h_pc( p_ctx->p_proc->proc.pc )
                                      + rel_addr );
}


// CALL a
static inline void instr_call( st_instr_context *p_ctx )
{
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  t_pc pc_h = be2h_pc( p_ctx->p_proc->proc.pc );
  instr_stack_push( p_ctx, pc_h );
  p_ctx->p_proc->proc.pc = h2be_pc( pc_h + rel_addr );
}


// RET
static inline void instr_ret( st_instr_context *p_ctx )
{
  t_val pc_h = instr_stack_pop( p_ctx );
  instr_err_if_u( (t_pc)pc_h > PC_MAX, p_ctx, IE_BYTECODE, );
  p_ctx->p_proc->proc.pc = h2be_pc( (t_pc)pc_h );
}


// LCALL a
static inline void instr_lcall( st_instr_context *p_ctx )
{
  t_val addr = instr_bytecode_get4( p_ctx );
  instr_stack_push( p_ctx, be2h_pc( p_ctx->p_proc->proc.pc ) );
  p_ctx->p_proc->proc.pc = h2be_pc( addr );
}


// CHNEW l, n
static inline void instr_chnew( st_instr_context *p_ctx )
{
  uint8_t max_len = instr_bytecode_get1( p_ctx );
  uint8_t type_len = instr_bytecode_get1( p_ctx );
  t_val bits[type_len]; // fetch bit counts from stack
  uint8_t msg_len = 0; // and get message size from bits
  int i;
  instr_err_if( p_ctx->p_glob->chan_cnt >= CHAN_CNT_MAX, p_ctx, IE_CHAN_CNT, );
  instr_err_if_u( type_len <= 0, p_ctx, IE_INV_CHAN_TYPE, );
  for( i = type_len - 1; i >= 0; i-- )
  {
    bits[i] = instr_stack_pop( p_ctx );
    if( bits[i] >= -7 && bits[i] <= 8 )
      msg_len += 1;
    else if( bits[i] >= -15 && bits[i] <= 16 )
      msg_len += 2;
    else if( bits[i] >= -31 && bits[i] <= 32 )
      msg_len += 4;
#ifndef UNSAFE // avoid warning about empty body in else statement in UNSAFE mode
    else
      instr_err_if_u( 1, p_ctx, IE_INV_CHAN_TYPE, );
#endif
  }
  t_chid new_chid = global_state_get_new_chid( p_ctx->p_glob, // generate id for new channel
                                               be2h_pid( p_ctx->p_proc->proc.pid ) );
  instr_err_if( new_chid == 0, p_ctx, IE_CHAN_CNT, );
  st_channel_header * p_new_chan; // create channel
  st_global_state_header * p_glob_new = global_state_copy_new_channel( p_ctx->p_glob,
                                                                       new_chid, max_len, type_len, msg_len,
                                                                       p_ctx->pp_buf, p_ctx->p_buf_len,
                                                                       &p_new_chan );
  instr_err_if( p_glob_new == NULL, p_ctx, IE_STATE_MEM, );
  p_ctx->p_glob = p_glob_new; // update current context
  p_ctx->p_gvar = global_state_get_variables( p_ctx->p_glob );
  p_ctx->gvar_sz = (t_val)be2h_16( p_ctx->p_glob->gvar_sz );
  p_ctx->p_proc = global_state_get_active_process( p_ctx->p_glob );
  instr_err_if_u( p_ctx->p_proc == NULL, p_ctx, IE_NO_ACT_PROC, );
  p_ctx->p_lvar = process_get_variables( &p_ctx->p_proc->proc );
  p_ctx->lvar_sz = (t_val)p_ctx->p_proc->proc.lvar_sz;
  p_ctx->p_stack = process_active_get_stack( p_ctx->p_proc );
  int8_t *p_type = channel_get_type( p_new_chan ); // store type in channel
  for( i = 0; i < (int)type_len; i++ )
    p_type[i] = bits[i];
  instr_stack_push( p_ctx, be2h_chid( p_new_chan->chid ) ); // put id of new channel onto stack
}


// CHMAX
static inline void instr_chmax( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_stack_push( p_ctx, max( 1, p_chan->max_len ) );
}


// CHLEN
static inline void instr_chlen( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_stack_push( p_ctx, p_chan->cur_len );
}


// CHFREE
static inline void instr_chfree( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_stack_push( p_ctx, max( 1, p_chan->max_len ) - p_chan->cur_len );
}


// CHADD
static inline void instr_chadd( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len >= max( 1, p_chan->max_len ), p_ctx, IE_CHAN_OV, );
  char *p_msg = channel_get_msg( p_chan, p_chan->cur_len ); // add message
  p_chan->cur_len++;
  memset( p_msg, 0, p_chan->msg_len ); // fill message with 0
}


// CHSET - internal version
static inline void instr_chset_internal( st_instr_context *p_ctx, t_val value, t_u_val ofs )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len < 1, p_ctx, IE_CHAN_EMPTY, );
  if( ofs >= (t_u_val)p_chan->type_len ) // offset behind message -> ignore
    return;
  int8_t *p_type = channel_get_type( p_chan );
  unsigned int i; // get pointer to value in message from type
  char *ptr = channel_get_msg( p_chan, p_chan->cur_len - 1 ); // last message
  for( i = 0; i < ofs; i++ )
    ptr += p_type[i] < 0 ? (-p_type[i] + 1 + 7) >> 3 : (p_type[i] + 7) >> 3;
  if( p_type[ofs] < 0 ) // write value (signed)
  {
    if( p_type[ofs] >= -7 )
      *(int8_t *)ptr = (int8_t)value;
    else if( p_type[ofs] >= -15 )
      ua_wr_16( ptr, h2be_16( (int16_t)value ) );
    else
      ua_wr_32( ptr, h2be_32( (int32_t)value ) );
  }
  else // write value (unsigned)
  {
    if( p_type[ofs] <= 8 )
      *(uint8_t *)ptr = (uint8_t)value;
    else if( p_type[ofs] <= 16 )
      ua_wr_16( ptr, h2be_16( (uint16_t)value ) );
    else
      ua_wr_32( ptr, h2be_32( (uint32_t)value ) );
  }
}


// CHSET
static inline void instr_chset( st_instr_context *p_ctx )
{
  t_val value = instr_stack_pop( p_ctx );
  t_u_val ofs = instr_stack_pop( p_ctx );
  instr_chset_internal( p_ctx, value, ofs );
}


// CHGET - internal version
static inline void instr_chget_internal( st_instr_context *p_ctx, t_u_val ofs )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len < 1, p_ctx, IE_CHAN_EMPTY, );
  if( ofs >= (t_u_val)p_chan->type_len ) // offset behind message -> return 0
  {
    instr_stack_push( p_ctx, 0 );
    return;
  }
  int8_t *p_type = channel_get_type( p_chan );
  unsigned int i; // get pointer to value in message from type
  char *ptr = channel_get_msg( p_chan, 0 ); //first message
  for( i = 0; i < ofs; i++ )
    ptr += p_type[i] < 0 ? (-p_type[i] + 1 + 7) >> 3 : (p_type[i] + 7) >> 3;
  t_val value;
  if( p_type[ofs] < 0 ) // read value (signed)
  {
    if( p_type[ofs] >= -7 )
      value = *(int8_t *)ptr;
    else if( p_type[ofs] >= -15 )
      value = be2h_16( ua_rd_16( ptr ) );
    else
      value = be2h_32( ua_rd_32( ptr ) );
  }
  else // read value (unsigned)
  {
    if( p_type[ofs] <= 8 )
      value = *(uint8_t *)ptr;
    else if( p_type[ofs] <= 16 )
      value = be2h_16( ua_rd_16( ptr ) );
    else
      value = be2h_32( ua_rd_32( ptr ) );
  }
  instr_stack_push( p_ctx, value );
}


// CHGET
static inline void instr_chget( st_instr_context *p_ctx )
{
  t_u_val ofs = instr_stack_pop( p_ctx );
  instr_chget_internal( p_ctx, ofs );
}


// CHDEL
static inline void instr_chdel( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len < 1, p_ctx, IE_CHAN_UN, );
  char *p_msg = channel_get_msg( p_chan, 0 ); // remove message by shifting messages one place left
  p_chan->cur_len--;
  memmove( p_msg, p_msg + p_chan->msg_len, p_chan->cur_len * p_chan->msg_len );
  p_msg = channel_get_msg( p_chan, p_chan->cur_len ); // fill message now unused with 0
  memset( p_msg, 0, p_chan->msg_len );
}


// CHSORT
static inline void instr_chsort( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len < 1, p_ctx, IE_CHAN_EMPTY, );
  char *p_msg = channel_get_msg( p_chan, 0 ); // get start of messages
  char *p_new = p_msg + (p_chan->cur_len - 1) * p_chan->msg_len; // last message is newest one (the one to be sorted into channel)
  int i; // search first message greater than new one
  char *p_cur;
  for( i = 0, p_cur = p_msg; i < p_chan->cur_len - 1; i++, p_cur += p_chan->msg_len )
    // simply compare byte-wise (possible because values are stored in big-endian format)
    if( memcmp( p_cur, p_new, p_chan->msg_len ) > 0 )
      break;
  { // move new message to current position
    char buf[p_chan->msg_len];
    memcpy( buf, p_new, p_chan->msg_len );
    memmove( p_cur + p_chan->msg_len, p_cur, (p_chan->cur_len - 1 - i) * p_chan->msg_len );
    memcpy( p_cur, buf, p_chan->msg_len ); 
  }
}


// CHROT
static inline void instr_chrot( st_instr_context *p_ctx )
{
  t_chid chid = instr_stack_pop( p_ctx );
  instr_err_if( ! CHID_OK( chid ), p_ctx, IE_INV_CHAN_ID, );
  st_channel_header *p_chan = global_state_get_channel( p_ctx->p_glob, chid );
  instr_err_if( p_chan == NULL, p_ctx, IE_NO_CHAN, );
  instr_err_if_u( p_chan->cur_len < 1, p_ctx, IE_CHAN_EMPTY, );
  char *p_msg = channel_get_msg( p_chan, 0 ); // get start of messages
  { // move first message to end of channel
    char buf[p_chan->msg_len];
    memcpy( buf, p_msg, p_chan->msg_len );
    memmove( p_msg, p_msg + p_chan->msg_len, (p_chan->cur_len - 1) * p_chan->msg_len );
    memcpy( p_msg + (p_chan->cur_len - 1) * p_chan->msg_len, buf, p_chan->msg_len ); 
  }
}


// CHSETO o
static inline void instr_chseto( st_instr_context *p_ctx )
{
  t_val value = instr_stack_pop( p_ctx );
  uint8_t ofs = instr_bytecode_get1( p_ctx );
  instr_chset_internal( p_ctx, value, ofs );
}


// CHGETO o
static inline void instr_chgeto( st_instr_context *p_ctx )
{
  uint8_t ofs = instr_bytecode_get1( p_ctx );
  instr_chget_internal( p_ctx, ofs );
}


// NDET a - internal version
// swap_paths: if to jump in first path
// if_no_step: if to use second path only if no state in first path
static inline void instr_ndet_internal( st_instr_context *p_ctx, int swap_paths, int if_no_step )
{
  int16_t rel_addr = instr_bytecode_get2( p_ctx );

  // remember offset of program counter
  int ofs_pc = (char *)&p_ctx->p_proc->proc.pc - (char *)p_ctx->p_glob;

  // create copy of state
  st_global_state_header *p_glob_dup = global_state_copy( p_ctx->p_glob, p_ctx->pp_buf, p_ctx->p_buf_len );
  instr_err_if( p_glob_dup == NULL, p_ctx, IE_STATE_MEM, );

  // change program counter in one copy
  st_global_state_header *p_glob_change;
  if( swap_paths )
    p_glob_change = p_ctx->p_glob; // change program counter in original state
  else
    p_glob_change = p_glob_dup; // change program counter in copy of state
  t_pc *p_pc = (t_pc *)((char *)p_glob_change + ofs_pc); // get pointer to program counter  
  *p_pc = h2be_pc( be2h_pc( *p_pc ) + rel_addr ); // change program counter

  // put copy of state onto stack with states to process
  instr_push_state( p_ctx, p_glob_dup, p_ctx->p_output,
                    if_no_step ? p_ctx->step_cnt // process state only if no step is completed since now
                               : (unsigned int)-1 ); // process state in any case
}


// NDET a
static inline void instr_ndet( st_instr_context *p_ctx )
{
  instr_ndet_internal( p_ctx, 0, 0 ); // normal
}


// ELSE a
static inline void instr_else( st_instr_context *p_ctx )
{
  instr_ndet_internal( p_ctx, 0, 1 ); // if no step in 1st path
}


// UNLESS a
static inline void instr_unless( st_instr_context *p_ctx )
{
  instr_ndet_internal( p_ctx, 1, 1 ); // swap paths, if no step in 1 st path
}


// NEX
static inline void instr_nex( st_instr_context *p_ctx )
{
  p_ctx->mode = INSTR_CONTEXT_MODE_INVALID; // mark current context as invalid
  p_ctx->p_glob = NULL;
  p_ctx->p_gvar = NULL;
  p_ctx->gvar_sz = 0;
  p_ctx->p_proc = NULL;
  p_ctx->p_lvar = NULL;
  p_ctx->lvar_sz = 0;
  p_ctx->p_stack = NULL;
}


// NEXZ
static inline void instr_nexz( st_instr_context *p_ctx )
{
  if( instr_stack_pop( p_ctx ) == 0 )
    instr_nex( p_ctx );
}


// NEXNZ
static inline void instr_nexnz( st_instr_context *p_ctx )
{
  if( instr_stack_pop( p_ctx ) != 0 )
    instr_nex( p_ctx );
}


// STEP ?, l
static inline void instr_step_intern( st_instr_context *p_ctx, uint8_t new_mode, t_pid excl_pid_be )
{
  uint8_t label = instr_bytecode_get1( p_ctx );
  p_ctx->p_proc->proc.flags &= ~PROCESS_FLAGS_MODE; // set new mode
  p_ctx->p_proc->proc.flags |= new_mode;
  p_ctx->p_glob->excl_pid = excl_pid_be;
  p_ctx->mode = INSTR_CONTEXT_MODE_COMPLETED; // mark current context as step completed
  p_ctx->invisible = new_mode == PROCESS_FLAGS_MODE_INVISIBLE; // if this state is invisible
  p_ctx->label = label; // save label
  p_ctx->flag_reg = p_ctx->p_proc->flag_reg; // save flag register value
  global_state_deactivate( p_ctx->p_glob, p_ctx->p_proc ); // deactivate process
  p_ctx->p_gvar = NULL;
  p_ctx->gvar_sz = 0;
  p_ctx->p_proc = NULL;
  p_ctx->p_lvar = NULL;
  p_ctx->lvar_sz = 0;
  p_ctx->p_stack = NULL;
  p_ctx->step_cnt++; // count completed steps
}
// STEP N, l
static inline void instr_step_n( st_instr_context *p_ctx )
{
  instr_step_intern( p_ctx, PROCESS_FLAGS_MODE_NORMAL, h2be_pid( 0 ) );
}
// STEP A, l
static inline void instr_step_a( st_instr_context *p_ctx )
{
  instr_step_intern( p_ctx, PROCESS_FLAGS_MODE_ATOMIC, p_ctx->p_proc->proc.pid );
}
// STEP I, l
static inline void instr_step_d( st_instr_context *p_ctx )
{
  instr_step_intern( p_ctx, PROCESS_FLAGS_MODE_INVISIBLE, p_ctx->p_proc->proc.pid );
}
// STEP T, l
static inline void instr_step_t( st_instr_context *p_ctx )
{
  t_pid pid = be2h_pid( p_ctx->p_proc->proc.pid );
  instr_step_intern( p_ctx, PROCESS_FLAGS_MODE_TERMINATED, h2be_pid( 0 ) );
#ifndef KEEP_TERMINATED
  st_process_header * p_proc = global_state_get_process( p_ctx->p_glob, pid ); // remove process
  global_state_remove( p_ctx->p_glob, p_proc );
#endif
}


// [L]RUN l, k, a
static inline void instr_run_intern(  st_instr_context *p_ctx, uint8_t lvar_sz, uint8_t param_cnt, t_val addr )
{
  instr_err_if( p_ctx->p_glob->proc_cnt >= PROC_CNT_MAX, p_ctx, IE_PROC_CNT, );
  t_pid max_pid = global_state_get_max_pid( p_ctx->p_glob ); // generate id for new process
  instr_err_if( max_pid >= PID_MAX, p_ctx, IE_PROC_CNT, );
  st_process_header * p_new_proc;
  st_global_state_header * p_glob_new = global_state_copy_new_process( p_ctx->p_glob, // create process
                                                                       max_pid + 1, lvar_sz,
                                                                       p_ctx->pp_buf, p_ctx->p_buf_len,
                                                                       &p_new_proc );
  instr_err_if( p_glob_new == NULL, p_ctx, IE_STATE_MEM, );
  p_ctx->p_glob = p_glob_new; // update current context
  p_ctx->p_gvar = global_state_get_variables( p_ctx->p_glob );
  p_ctx->gvar_sz = (t_val)be2h_16( p_ctx->p_glob->gvar_sz );
  p_ctx->p_proc = global_state_get_active_process( p_ctx->p_glob );
  instr_err_if_u( p_ctx->p_proc == NULL, p_ctx, IE_NO_ACT_PROC, );
  p_ctx->p_lvar = process_get_variables( &p_ctx->p_proc->proc );
  p_ctx->lvar_sz = (t_val)p_ctx->p_proc->proc.lvar_sz;
  p_ctx->p_stack = process_active_get_stack( p_ctx->p_proc );
  p_new_proc->pc = h2be_pc( addr ); // set program counter of new process
  int i; // fetch parameters and their bit counts from stack
  t_val bits[param_cnt], vals[param_cnt];
  for( i = param_cnt - 1; i >= 0; i-- )
  {
    vals[i] = instr_stack_pop( p_ctx );
    bits[i] = instr_stack_pop( p_ctx );
  }
  char *ptr = process_get_variables( p_new_proc ); // put parameters into local variables of new process
  char *ptr_end = ptr + p_new_proc->lvar_sz;
  for( i = 0; i < param_cnt; i++ )
  {
    if( bits[i] < 0 ) // write value (signed)
    {
      if( bits[i] >= -7 ) {
        if( ptr + 1 > ptr_end ) break;
        *(int8_t *)ptr = (int8_t)vals[i];
        ptr++; }
      else if( bits[i] >= -15 ) {
        if( ptr + 2 > ptr_end ) break;
        ua_wr_16( ptr, h2be_16( (int16_t)vals[i] ) );
        ptr += 2; }
      else {
        if( ptr + 4 > ptr_end ) break;
        ua_wr_32( ptr, h2be_32( (int32_t)vals[i] ) );
        ptr += 4; }
    }
    else // write value (unsigned)
    {
      if( bits[i] <= 8 ) {
        if( ptr + 1 > ptr_end ) break;
        *(uint8_t *)ptr = (uint8_t)vals[i];
        ptr++; }
      else if( bits[i] <= 16 ) {
        if( ptr + 2 > ptr_end ) break;
        ua_wr_16( ptr, h2be_16( (uint16_t)vals[i] ) );
        ptr += 2; }
      else {
        if( ptr + 4 > ptr_end ) break;
        ua_wr_32( ptr, h2be_32( (uint32_t)vals[i] ) );
        ptr += 4; }
    }
  }  
  instr_stack_push( p_ctx, be2h_pid( p_new_proc->pid ) ); // put id of new process onto stack
  instr_err_if_u( i < param_cnt, p_ctx, IE_LOCAL, ) // not all parameters fit into local variables
}

// RUN l, k, a
static inline void instr_run( st_instr_context *p_ctx )
{
  uint8_t lvar_sz = instr_bytecode_get1( p_ctx );
  uint8_t param_cnt = instr_bytecode_get1( p_ctx );
  int16_t rel_addr = instr_bytecode_get2( p_ctx );
  t_val addr = be2h_pc( p_ctx->p_proc->proc.pc ) + rel_addr;
  instr_run_intern( p_ctx, lvar_sz, param_cnt, addr );
}

// LRUN l, k, a
static inline void instr_lrun( st_instr_context *p_ctx )
{
  uint8_t lvar_sz = instr_bytecode_get1( p_ctx );
  uint8_t param_cnt = instr_bytecode_get1( p_ctx );
  t_val addr = instr_bytecode_get4( p_ctx );
  instr_run_intern( p_ctx, lvar_sz, param_cnt, addr );
}


// GLOBSZ[X] g - internal vaerion
static inline void instr_globsz_intern( st_instr_context *p_ctx, uint16_t gvar_sz )
{
  st_global_state_header * p_glob_new = global_state_copy_gvar_sz( p_ctx->p_glob, // resize global variables
                                                                   gvar_sz,
                                                                   p_ctx->pp_buf, p_ctx->p_buf_len );
  instr_err_if( p_glob_new == NULL, p_ctx, IE_STATE_MEM, );
  p_ctx->p_glob = p_glob_new; // update current context
  p_ctx->p_gvar = global_state_get_variables( p_ctx->p_glob );
  p_ctx->gvar_sz = (t_val)be2h_16( p_ctx->p_glob->gvar_sz );
  p_ctx->p_proc = global_state_get_active_process( p_ctx->p_glob );
  instr_err_if_u( p_ctx->p_proc == NULL, p_ctx, IE_NO_ACT_PROC, );
  p_ctx->p_lvar = process_get_variables( &p_ctx->p_proc->proc );
  p_ctx->lvar_sz = (t_val)p_ctx->p_proc->proc.lvar_sz;
  p_ctx->p_stack = process_active_get_stack( p_ctx->p_proc );
}


// GLOBSZ g
static inline void instr_globsz( st_instr_context *p_ctx )
{
  instr_globsz_intern( p_ctx, (uint16_t)(uint8_t)instr_bytecode_get1( p_ctx ) );
}


// LOCSZ l
static inline void instr_locsz( st_instr_context *p_ctx )
{
  uint8_t lvar_sz = instr_bytecode_get1( p_ctx );
  st_global_state_header * p_glob_new = global_state_copy_lvar_sz( p_ctx->p_glob, // resize local variables
                                                                   (st_process_header *)p_ctx->p_proc, lvar_sz,
                                                                   p_ctx->pp_buf, p_ctx->p_buf_len );
  instr_err_if( p_glob_new == NULL, p_ctx, IE_STATE_MEM, );
  p_ctx->p_glob = p_glob_new; // update current context
  p_ctx->p_gvar = global_state_get_variables( p_ctx->p_glob );
  p_ctx->gvar_sz = (t_val)be2h_16( p_ctx->p_glob->gvar_sz );
  p_ctx->p_proc = global_state_get_active_process( p_ctx->p_glob );
  instr_err_if_u( p_ctx->p_proc == NULL, p_ctx, IE_NO_ACT_PROC, );
  p_ctx->p_lvar = process_get_variables( &p_ctx->p_proc->proc );
  p_ctx->lvar_sz = (t_val)p_ctx->p_proc->proc.lvar_sz;
  p_ctx->p_stack = process_active_get_stack( p_ctx->p_proc );
}


// GLOBSZX g
static inline void instr_globszx( st_instr_context *p_ctx )
{
  instr_globsz_intern( p_ctx, (uint16_t)instr_bytecode_get2( p_ctx ) );
}


// FCLR
static inline void instr_fclr( st_instr_context *p_ctx )
{
  p_ctx->p_proc->flag_reg = 0;
}


// FGET f
static inline void instr_fget( st_instr_context *p_ctx )
{
  t_flag_reg mask = 1 << instr_bytecode_get1( p_ctx );
  instr_stack_push( p_ctx, p_ctx->p_proc->flag_reg & mask ? 1 : 0 );
}


// FSET f
static inline void instr_fset( st_instr_context *p_ctx )
{
  t_flag_reg mask = 1 << instr_bytecode_get1( p_ctx );
  if( instr_stack_pop( p_ctx ) )
    p_ctx->p_proc->flag_reg |= mask;
  else
    p_ctx->p_proc->flag_reg &= ~mask;
}


// BGET r_i, b
static inline void instr_bget( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  t_val mask = 1 << instr_bytecode_get1( p_ctx );
  instr_stack_push( p_ctx, p_ctx->p_proc->registers[reg] & mask ? 1 : 0 );
}


// BSET r_i, b
static inline void instr_bset( st_instr_context *p_ctx )
{
  int8_t reg = instr_bytecode_get1( p_ctx ) & 7;
  t_val mask = 1 << instr_bytecode_get1( p_ctx );
  if( instr_stack_pop( p_ctx ) )
    p_ctx->p_proc->registers[reg] |= mask;
  else
    p_ctx->p_proc->registers[reg] &= ~mask;
}


// PRINTS str
static inline void instr_prints( st_instr_context *p_ctx )
{
  uint16_t str = instr_bytecode_get2( p_ctx );
  // create new output list entry in buffer
  instr_err_if( *(p_ctx->p_buf_len) < sizeof( st_instr_output ), p_ctx, IE_STATE_MEM, );
  st_instr_output *p_output = (st_instr_output *)*(p_ctx->pp_buf);
  *(p_ctx->pp_buf) += sizeof( st_instr_output );
  *(p_ctx->p_buf_len) -= sizeof( st_instr_output );
  // save string to print
  p_output->is_str = 1;
  p_output->data.str.str = str;
  // insert output list entry into output list
  p_output->p_prev = p_ctx->p_output;
  p_ctx->p_output = p_output;
}


// PRINTV fmt
static inline void instr_printv( st_instr_context *p_ctx )
{
  uint8_t fmt = instr_bytecode_get1( p_ctx );
  t_val value = instr_stack_pop( p_ctx );
  // create new output list entry in buffer
  instr_err_if( *(p_ctx->p_buf_len) < sizeof( st_instr_output ), p_ctx, IE_STATE_MEM, );
  st_instr_output *p_output = (st_instr_output *)*(p_ctx->pp_buf);
  *(p_ctx->pp_buf) += sizeof( st_instr_output );
  *(p_ctx->p_buf_len) -= sizeof( st_instr_output );
  // save format and value to print
  p_output->is_str = 0;
  p_output->data.value.fmt = fmt;
  p_output->data.value.value = value;
  // insert output list entry into output list
  p_output->p_prev = p_ctx->p_output;
  p_ctx->p_output = p_output;
}


// LDVA L
static inline void instr_ldva_l1u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint8_t)instr_local_get1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_l1s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_local_get1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_l2u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_local_get2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_l2s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_local_get2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_l4( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_local_get4( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}


// LDVA G
static inline void instr_ldva_g1u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint8_t)instr_global_get1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_g1s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_global_get1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_g2u( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_global_get2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_g2s( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_global_get2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}
static inline void instr_ldva_g4( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_global_get4( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ) ) );
}


// STVA L
static inline void instr_stva_l1u( st_instr_context *p_ctx )
{
  instr_local_put1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int8_t)(uint8_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_l1s( st_instr_context *p_ctx )
{
  instr_local_put1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int8_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_l2u( st_instr_context *p_ctx )
{
  instr_local_put2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_l2s( st_instr_context *p_ctx )
{
  instr_local_put2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_l4( st_instr_context *p_ctx )
{
  instr_local_put4( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), instr_stack_pop( p_ctx ) );
}


// STVA G
static inline void instr_stva_g1u( st_instr_context *p_ctx )
{
  instr_global_put1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int8_t)(uint8_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_g1s( st_instr_context *p_ctx )
{
  instr_global_put1( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int8_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_g2u( st_instr_context *p_ctx )
{
  instr_global_put2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_g2s( st_instr_context *p_ctx )
{
  instr_global_put2( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stva_g4( st_instr_context *p_ctx )
{
  instr_global_put4( p_ctx, (t_val)(t_u_val)(uint8_t)instr_bytecode_get1( p_ctx ), instr_stack_pop( p_ctx ) );
}


// LDA a
static inline void instr_load_address( st_instr_context *p_ctx )
{
  // load address from bytecode onto stack
  instr_stack_push( p_ctx, instr_bytecode_get4( p_ctx ) );
}


// PCVAL
static inline void instr_pcval( st_instr_context *p_ctx )
{
  t_val pid = instr_stack_pop( p_ctx );
  // find process
  instr_err_if( ! PID_OK( pid ), p_ctx, IE_INV_PROC_ID, );
  st_process_header *p_proc = global_state_get_process( p_ctx->p_glob, (t_pid)pid );
  instr_err_if( p_proc == NULL, p_ctx, IE_NO_PROC, );
  // return program counter
  instr_stack_push( p_ctx, be2h_pc( p_proc->pc ) );
}


// LVAR
static inline char * instr_lvar_intern( st_instr_context *p_ctx, int len )
{
  t_val addr = instr_stack_pop( p_ctx );
  t_val pid = instr_stack_pop( p_ctx );
  // find process
  instr_err_if( ! PID_OK( pid ), p_ctx, IE_INV_PROC_ID, NULL );
  st_process_header *p_proc = global_state_get_process( p_ctx->p_glob, (t_pid)pid );
  instr_err_if( p_proc == NULL, p_ctx, IE_NO_PROC, NULL );
  // get address of local variable
  instr_err_if_u( addr < 0 || addr + len > (t_val)p_proc->lvar_sz, p_ctx, IE_LOCAL, NULL );
  char *p_lvar = process_get_variables( p_proc );
  return p_lvar + addr;
}
static inline void instr_lvar_1u( st_instr_context *p_ctx )
{
  char * ptr = instr_lvar_intern( p_ctx, 1 );
  if( ptr != NULL )
    instr_stack_push( p_ctx, *(uint8_t *)ptr );
}
static inline void instr_lvar_1s( st_instr_context *p_ctx )
{
  char * ptr = instr_lvar_intern( p_ctx, 1 );
  if( ptr != NULL )
    instr_stack_push( p_ctx, *(int8_t *)ptr );
}
static inline void instr_lvar_2u( st_instr_context *p_ctx )
{
  char * ptr = instr_lvar_intern( p_ctx, 2 );
  if( ptr != NULL )
    instr_stack_push( p_ctx, (uint16_t)be2h_16( ua_rd_16( ptr ) ) );
}
static inline void instr_lvar_2s( st_instr_context *p_ctx )
{
  char * ptr = instr_lvar_intern( p_ctx, 2 );
  if( ptr != NULL )
    instr_stack_push( p_ctx, (int16_t)be2h_16( ua_rd_16( ptr ) ) );
}
static inline void instr_lvar_4( st_instr_context *p_ctx )
{
  char * ptr = instr_lvar_intern( p_ctx, 4 );
  if( ptr != NULL )
    instr_stack_push( p_ctx, (int32_t)be2h_32( ua_rd_32( ptr ) ) );
}


// ENAB
static inline void instr_enab( st_instr_context *p_ctx )
{
  t_val pid = instr_stack_pop( p_ctx );

  // get copy of global state without current process
  //  - allocate this state locally on the stack cause it is only needed in this function
  // copy global state
  size_t sz = nipsvm_state_size( p_ctx->p_glob );
  char glob[sz];
  memcpy( glob, p_ctx->p_glob, sz );
  st_global_state_header * p_glob = (st_global_state_header *)glob;
  // get pointer to current process in copied state
  st_process_header *p_proc_cur = (st_process_header *)(glob + ((char *)p_ctx->p_proc - (char *)p_ctx->p_glob));
  // remove current process from copied state
  global_state_remove( p_glob, p_proc_cur );
  // set exclusive pid to 0 in copied state
  p_glob->excl_pid = h2be_pid( 0 );

  // find process to check for enabledness
  instr_err_if( ! PID_OK( pid ), p_ctx, IE_INV_PROC_ID, );
  st_process_header *p_proc = global_state_get_process( p_glob, (t_pid)pid );
  instr_err_if( p_proc == NULL, p_ctx, IE_NO_PROC, );

  // return false if process is terminated
  if( (p_proc->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED )
  {
    instr_stack_push( p_ctx, 0 );
    return;
  }

  // execute process to check if there is at least a single successor state
  unsigned int tmp_succ_cnt = 0;
  {
    // create buffer with memory for temporary states
    char tmp_buf[p_ctx->p_succ_ctx->enab_state_mem];
    char *p_tmp_buf = tmp_buf;
    unsigned long tmp_buf_len = sizeof( tmp_buf );
   
    // activate process
    st_process_active_header *p_proc_act;
    st_global_state_header *p_glob_act = global_state_copy_activate( p_glob, p_proc,
                                                                     p_ctx->p_succ_ctx->stack_max,
                                                                     p_ctx->init_flag_reg,
                                                                     &p_tmp_buf, &tmp_buf_len,
                                                                     &p_proc_act );
    if( p_glob_act == NULL )
    {
      p_ctx->p_succ_ctx->err_cb( IE_STATE_MEM, be2h_pid( p_proc->pid ), be2h_pc( p_proc->pc ),
                                 p_ctx->p_succ_ctx->priv_context );
      return;
    }

    // set up context to execute instructions
    st_instr_context ctx;
    st_instr_context_state states[p_ctx->p_succ_ctx->path_max]; // create stack of states to process
    states[0].p_glob = p_glob_act;
    states[0].p_output = NULL;
    states[0].max_step_cnt = (unsigned int)-1; // process this state in any case
    ctx.p_states = states;
    ctx.state_cnt_max = count( states );
    ctx.state_cnt = 1;
    ctx.step_cnt = 0; // no step completed yet
    ctx.pp_buf = &p_tmp_buf; // fill in pointers for buffer with memory for new temporary states
    ctx.p_buf_len = &tmp_buf_len;
    ctx.init_flag_reg = p_ctx->init_flag_reg;
    ctx.timeout = p_ctx->timeout;
    ctx.last_pid = p_ctx->last_pid;
    ctx.p_succ_ctx = p_ctx->p_succ_ctx;

    // execute instructions until all paths are done
    st_instr_tmp_succ tmp_succ;
    instr_exec_paths( &ctx, 1, // stop on first state found
                      &tmp_succ, 1, &tmp_succ_cnt );
  }

  // put result onto stack
  instr_stack_push( p_ctx, tmp_succ_cnt > 0 ? 1 : 0 );
}


// MONITOR
static inline void instr_monitor( st_instr_context *p_ctx )
{
  t_val pid = instr_stack_pop( p_ctx );

  // unset monitor process
  if( pid == 0 )
  {
    p_ctx->p_glob->monitor_pid = h2be_pid( 0 );
    return;
  }

  // find process
  instr_err_if( ! PID_OK( pid ), p_ctx, IE_INV_PROC_ID, );
  st_process_header *p_proc = global_state_get_process( p_ctx->p_glob, (t_pid)pid );
  instr_err_if( p_proc == NULL, p_ctx, IE_NO_PROC, );

  // set monitor process
  p_ctx->p_glob->monitor_pid = h2be_pid( pid );
}


// KILL
static inline void instr_kill( st_instr_context *p_ctx )
{
  t_val pid = instr_stack_pop( p_ctx );

  // find process
  instr_err_if( ! PID_OK( pid ), p_ctx, IE_INV_PROC_ID, );
  st_process_header *p_proc = global_state_get_process( p_ctx->p_glob, (t_pid)pid );
  instr_err_if( p_proc == NULL, p_ctx, IE_NO_PROC, );

  // self-kill -> STEP T
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE ) // there is _always_ only one active process, so this is our process if it is active
  {
    if( (st_process_header *)p_ctx->p_proc != p_proc ) // double check assumption from two lines before
      return;
    p_ctx->p_proc->proc.flags &= ~PROCESS_FLAGS_MODE; // terminate
    p_ctx->p_proc->proc.flags |= PROCESS_FLAGS_MODE_TERMINATED;
    p_ctx->p_glob->excl_pid = 0;
    p_ctx->mode = INSTR_CONTEXT_MODE_COMPLETED; // mark current context as step completed
    p_ctx->invisible = 0;
    p_ctx->label = 0;
    p_ctx->flag_reg = p_ctx->p_proc->flag_reg; // save flag register value
    global_state_deactivate( p_ctx->p_glob, p_ctx->p_proc ); // deactivate process
    p_ctx->p_gvar = NULL;
    p_ctx->gvar_sz = 0;
    p_ctx->p_proc = NULL;
    p_ctx->p_lvar = NULL;
    p_ctx->lvar_sz = 0;
    p_ctx->p_stack = NULL;
    p_ctx->step_cnt++; // count completed steps
    return;
  }
  
  // kill other
  else
  {
    // terminate process
    p_proc->flags &= ~PROCESS_FLAGS_MODE;
    p_proc->flags |= PROCESS_FLAGS_MODE_TERMINATED;
  }

#ifndef KEEP_TERMINATED
  p_proc = global_state_get_process( p_ctx->p_glob, (t_pid)pid ); // remove process
  global_state_remove( p_ctx->p_glob, p_proc );
#endif
}


// LDB L
static inline void instr_ldb_l( st_instr_context *p_ctx )
{
  uint8_t bit = (uint8_t)(t_u_val)instr_stack_pop( p_ctx ) & 0x07;
  t_val addr = instr_stack_pop( p_ctx );
  uint8_t byte = instr_local_get1( p_ctx, addr );
  instr_stack_push( p_ctx, byte & 1 << bit ? 1 : 0 );
}


// LDB G
static inline void instr_ldb_g( st_instr_context *p_ctx )
{
  uint8_t bit = (uint8_t)(t_u_val)instr_stack_pop( p_ctx ) & 0x07;
  t_val addr = instr_stack_pop( p_ctx );
  uint8_t byte = instr_global_get1( p_ctx, addr );
  instr_stack_push( p_ctx, byte & 0x01 << bit ? 1 : 0 );
}


// STB L
static inline void instr_stb_l( st_instr_context *p_ctx )
{
  uint8_t bit = (uint8_t)(t_u_val)instr_stack_pop( p_ctx ) & 0x07;
  t_val addr = instr_stack_pop( p_ctx );
  t_val val = instr_stack_pop( p_ctx );
  uint8_t byte = instr_local_get1( p_ctx, addr );
  if( val )
    byte |= 0x01 << bit;
  else
    byte &= ~(0x01 << bit);
  instr_local_put1( p_ctx, addr, byte );
}


// STB G
static inline void instr_stb_g( st_instr_context *p_ctx )
{
  uint8_t bit = (uint8_t)(t_u_val)instr_stack_pop( p_ctx ) & 0x07;
  t_val addr = instr_stack_pop( p_ctx );
  t_val val = instr_stack_pop( p_ctx );
  uint8_t byte = instr_global_get1( p_ctx, addr );
  if( val )
    byte |= 0x01 << bit;
  else
    byte &= ~(0x01 << bit);
  instr_global_put1( p_ctx, addr, byte );
}


// LDV L LE
static inline void instr_ldv_l2u_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_local_get2_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l2s_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)instr_local_get2_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_l4_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_local_get4_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}


// LDV G LE
static inline void instr_ldv_g2u_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_val)(t_u_val)(uint16_t)instr_global_get2_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g2s_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, (t_u_val)instr_global_get2_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}
static inline void instr_ldv_g4_le( st_instr_context *p_ctx )
{
  instr_stack_push( p_ctx, instr_global_get4_le( p_ctx, instr_stack_pop( p_ctx ) ) );
}


// STV L LE
static inline void instr_stv_l2u_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put2_le( p_ctx, addr, (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l2s_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put2_le( p_ctx, addr, (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_l4_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_local_put4_le( p_ctx, addr, instr_stack_pop( p_ctx ) );
}


// STV G LE
static inline void instr_stv_g2u_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put2_le( p_ctx, addr, (int16_t)(uint16_t)(t_u_val)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g2s_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put2_le( p_ctx, addr, (int16_t)instr_stack_pop( p_ctx ) );
}
static inline void instr_stv_g4_le( st_instr_context *p_ctx )
{
  t_val addr = instr_stack_pop( p_ctx );
  instr_global_put4_le( p_ctx, addr, instr_stack_pop( p_ctx ) );
}


// execute an instruction
static inline void instr_exec( st_instr_context *p_ctx ) // extern
{
  // check that program counter is within bytecode
  t_pc pc_h = be2h_pc( p_ctx->p_proc->proc.pc );
  instr_err_if_u( pc_h >= p_ctx->p_succ_ctx->bytecode->size, p_ctx, IE_BYTECODE, );

  // select instruction (and advance program counter)
#ifdef DEBUG_INSTR
  printf( "DEBUG (before instr): " ); global_state_print( p_ctx->p_glob );
#endif
  p_ctx->p_proc->proc.pc = h2be_pc( pc_h + 1 );
  switch( p_ctx->p_succ_ctx->bytecode->ptr[pc_h] )
  {
    case 0x00: /* NOP */ break;
    case 0x01: /* LDC c */ instr_ldc( p_ctx ); break;
    case 0x02: /* LDV L (unsigned byte) */ instr_ldv_l1u( p_ctx ); break;
    case 0x03: /* LDV L (signed byte) */ instr_ldv_l1s( p_ctx ); break;
    case 0x04: /* LDV L (unsigned word) */ instr_ldv_l2u( p_ctx ); break;
    case 0x05: /* LDV L (signed word) */ instr_ldv_l2s( p_ctx ); break;
    case 0x06: /* LDV L (double-word) */ instr_ldv_l4( p_ctx ); break;
    case 0x07: /* LDV G (unsigned byte) */ instr_ldv_g1u( p_ctx ); break;
    case 0x08: /* LDV G (signed byte) */ instr_ldv_g1s( p_ctx ); break;
    case 0x09: /* LDV G (unsigned word) */ instr_ldv_g2u( p_ctx ); break;
    case 0x0A: /* LDV G (signed word) */ instr_ldv_g2s( p_ctx ); break;
    case 0x0B: /* LDV G (double-word) */ instr_ldv_g4( p_ctx ); break;
    case 0x0C: /* STV L (unsigned byte) */ instr_stv_l1u( p_ctx ); break;
    case 0x0D: /* STV L (signed byte) */ instr_stv_l1s( p_ctx ); break;
    case 0x0E: /* STV L (unsigned word) */ instr_stv_l2u( p_ctx ); break;
    case 0x0F: /* STV L (signed word) */ instr_stv_l2s( p_ctx ); break;
    case 0x10: /* STV L (double-word) */ instr_stv_l4( p_ctx ); break;
    case 0x11: /* STV G (unsigned byte) */ instr_stv_g1u( p_ctx ); break;
    case 0x12: /* STV G (signed byte) */ instr_stv_g1s( p_ctx ); break;
    case 0x13: /* STV G (unsigned word) */ instr_stv_g2u( p_ctx ); break;
    case 0x14: /* STV G (signed word) */ instr_stv_g2s( p_ctx ); break;
    case 0x15: /* STV G (double-word) */ instr_stv_g4( p_ctx ); break;
    case 0x16: /* TRUNC b */ instr_trunc( p_ctx ); break;
    // 0x17 reserved
    case 0x18: /* LDS timeout */ instr_lds_timeout( p_ctx ); break;
    case 0x19: /* LDS pid */ instr_lds_pid( p_ctx ); break;
    case 0x1A: /* LDS nrpr */ instr_lds_nrpr( p_ctx ); break;
    case 0x1B: /* LDS last */ instr_lds_last( p_ctx ); break;
    case 0x1C: /* LDS np */ instr_lds_np( p_ctx ); break;
    // 0x1D .. 0x1F reserved for future LDS commands
    case 0x20: /* ADD */ instr_add( p_ctx ); break;
    case 0x21: /* SUB */ instr_sub( p_ctx ); break;
    case 0x22: /* MUL */ instr_mul( p_ctx ); break;
    case 0x23: /* DIV */ instr_div( p_ctx ); break;
    case 0x24: /* MOD */ instr_mod( p_ctx ); break;
    case 0x25: /* NEG */ instr_neg( p_ctx ); break;
    case 0x26: /* NOT */ instr_not( p_ctx ); break;
    case 0x27: /* AND */ instr_and( p_ctx ); break;
    case 0x28: /* OR */ instr_or( p_ctx ); break;
    case 0x29: /* XOR */ instr_xor( p_ctx ); break;
    case 0x2A: /* SHL */ instr_shl( p_ctx ); break;
    case 0x2B: /* SHR */ instr_shr( p_ctx ); break;
    case 0x2C: /* EQ */ instr_eq( p_ctx ); break;
    case 0x2D: /* NEQ */ instr_neq( p_ctx ); break;
    case 0x2E: /* LT */ instr_lt( p_ctx ); break;
    case 0x2F: /* LTE */ instr_lte( p_ctx ); break;
    case 0x30: /* GT */ instr_gt( p_ctx ); break;
    case 0x31: /* GTE */ instr_gte( p_ctx ); break;
    case 0x32: /* BNOT */ instr_bnot( p_ctx ); break;
    case 0x33: /* BAND */ instr_band( p_ctx ); break;
    case 0x34: /* BOR */ instr_bor( p_ctx ); break;
    // 0x35 .. 0x3F reserved
    case 0x40: /* ICHK n */ instr_ichk( p_ctx ); break;
    case 0x41: /* BCHK */ instr_bchk( p_ctx ); break;
    // 0x42 .. 0x47 reserved
    case 0x48: /* JMP a */ instr_jmp( p_ctx ); break;
    case 0x49: /* JMPZ a */ instr_jmpz( p_ctx ); break;
    case 0x4A: /* JMPNZ a */ instr_jmpnz( p_ctx ); break;
    case 0x4B: /* LJMP a */ instr_ljmp( p_ctx ); break;
    // 0x4C .. 0x4F reserved
    case 0x50: /* TOP r_i */ instr_top( p_ctx ); break;
    case 0x51: /* POP r_i */ instr_pop( p_ctx ); break;
    case 0x52: /* PUSH r_i */ instr_push( p_ctx ); break;
    case 0x53: /* POPX */ instr_popx( p_ctx ); break;
    case 0x54: /* INC r_i */ instr_inc( p_ctx ); break;
    case 0x55: /* DEC r_i */ instr_dec( p_ctx ); break;
    case 0x56: /* LOOP r_i, a */ instr_loop( p_ctx ); break;
    // 0x57 reserved
    case 0x58: /* CALL a */ instr_call( p_ctx ); break;
    case 0x59: /* RET */ instr_ret( p_ctx ); break;
    case 0x5A: /* LCALL a */ instr_lcall( p_ctx ); break;
    // 0x5B .. 0x5F reserved
    case 0x60: /* CHNEW l, n */ instr_chnew( p_ctx ); break;
    case 0x61: /* CHMAX */ instr_chmax( p_ctx ); break;
    case 0x62: /* CHLEN */ instr_chlen( p_ctx ); break;
    case 0x63: /* CHFREE */ instr_chfree( p_ctx ); break;
    case 0x64: /* CHADD */ instr_chadd( p_ctx ); break;
    case 0x65: /* CHSET */ instr_chset( p_ctx ); break;
    case 0x66: /* CHGET */ instr_chget( p_ctx ); break;
    case 0x67: /* CHDEL */ instr_chdel( p_ctx ); break;
    case 0x68: /* CHSORT */ instr_chsort( p_ctx ); break;
    // 0x69 .. 0x6A reserved
    case 0x6B: /* CHROT */ instr_chrot( p_ctx ); break;
    case 0x6C: /* CHSETO o */ instr_chseto( p_ctx ); break;
    case 0x6D: /* CHGETO o */ instr_chgeto( p_ctx ); break;
    // 0x6E .. 0x6F reserved
    case 0x70: /* NDET a */ instr_ndet( p_ctx ); break;
    // 0x71 reserved
    case 0x72: /* ELSE a */ instr_else( p_ctx ); break;
    case 0x73: /* UNLESS a */ instr_unless( p_ctx ); break;
    case 0x74: /* NEX */ instr_nex( p_ctx ); break;
    case 0x75: /* NEXZ */ instr_nexz( p_ctx ); break;
    case 0x76: /* NEXNZ */ instr_nexnz( p_ctx ); break;
    // 0x77 reserved
    case 0x78: /* STEP N, l */ instr_step_n( p_ctx ); break;
    case 0x79: /* STEP A, l */ instr_step_a( p_ctx ); break;
    case 0x7A: /* STEP I, l */ instr_step_d( p_ctx ); break;
    case 0x7B: /* STEP T, l */ instr_step_t( p_ctx ); break;
    // 0x7C .. 0x7F reserved
    case 0x80: /* RUN l, k, a */ instr_run( p_ctx ); break;
    case 0x81: /* LRUN l, k, a */ instr_lrun( p_ctx ); break;
    // 0x82 .. 0x83 reserved
    case 0x84: /* GLOBSZ g */ instr_globsz( p_ctx ); break;
    case 0x85: /* LOCSZ l */ instr_locsz( p_ctx ); break;
    case 0x86: /* GLOBSZX g */ instr_globszx( p_ctx ); break;
    // 0x87 reserved
    case 0x88: /* FCLR */ instr_fclr( p_ctx ); break;
    case 0x89: /* FGET f */ instr_fget( p_ctx ); break;
    case 0x8A: /* FSET f */ instr_fset( p_ctx ); break;
    // 0x8B reserved
    case 0x8C: /* BGET r_i, b */ instr_bget( p_ctx ); break;
    case 0x8D: /* BSET r_i, b */ instr_bset( p_ctx ); break;
    // 0x8E .. 0x8F reserved
    case 0x90: /* PRINTS str */ instr_prints( p_ctx ); break;
    case 0x91: /* PRINTV fmt */ instr_printv( p_ctx ); break;
    case 0x92: /* LDVA L (unsigned byte) */ instr_ldva_l1u( p_ctx ); break;
    case 0x93: /* LDVA L (signed byte) */ instr_ldva_l1s( p_ctx ); break;
    case 0x94: /* LDVA L (unsigned word) */ instr_ldva_l2u( p_ctx ); break;
    case 0x95: /* LDVA L (signed word) */ instr_ldva_l2s( p_ctx ); break;
    case 0x96: /* LDVA L (double-word) */ instr_ldva_l4( p_ctx ); break;
    case 0x97: /* LDVA G (unsigned byte) */ instr_ldva_g1u( p_ctx ); break;
    case 0x98: /* LDVA G (signed byte) */ instr_ldva_g1s( p_ctx ); break;
    case 0x99: /* LDVA G (unsigned word) */ instr_ldva_g2u( p_ctx ); break;
    case 0x9A: /* LDVA G (signed word) */ instr_ldva_g2s( p_ctx ); break;
    case 0x9B: /* LDVA G (double-word) */ instr_ldva_g4( p_ctx ); break;
    case 0x9C: /* STVA L (unsigned byte) */ instr_stva_l1u( p_ctx ); break;
    case 0x9D: /* STVA L (signed byte) */ instr_stva_l1s( p_ctx ); break;
    case 0x9E: /* STVA L (unsigned word) */ instr_stva_l2u( p_ctx ); break;
    case 0x9F: /* STVA L (signed word) */ instr_stva_l2s( p_ctx ); break;
    case 0xA0: /* STVA L (double-word) */ instr_stva_l4( p_ctx ); break;
    case 0xA1: /* STVA G (unsigned byte) */ instr_stva_g1u( p_ctx ); break;
    case 0xA2: /* STVA G (signed byte) */ instr_stva_g1s( p_ctx ); break;
    case 0xA3: /* STVA G (unsigned word) */ instr_stva_g2u( p_ctx ); break;
    case 0xA4: /* STVA G (signed word) */ instr_stva_g2s( p_ctx ); break;
    case 0xA5: /* STVA G (double-word) */ instr_stva_g4( p_ctx ); break;
    // 0xA6 .. 0xAF reserved
    case 0xB0: /* LDA a */ instr_load_address( p_ctx ); break;
    // 0xB1 .. 0xB3 reserved
    case 0xB4: /* PCVAL */ instr_pcval( p_ctx ); break;
    // 0xB5 .. 0xB7 reserved
    case 0xB8: /* LVAR (unsigned byte) */ instr_lvar_1u( p_ctx ); break;
    case 0xB9: /* LVAR (signed byte) */ instr_lvar_1s( p_ctx ); break;
    case 0xBA: /* LVAR (unsigned word) */ instr_lvar_2u( p_ctx ); break;
    case 0xBB: /* LVAR (signed word) */ instr_lvar_2s( p_ctx ); break;
    case 0xBC: /* LVAR (double-word) */ instr_lvar_4( p_ctx ); break;
    // 0xBD reserved
    case 0xBE: /* ENAB */ instr_enab( p_ctx ); break;
    // 0xBF reserved
    case 0xC0: /* MONITOR */ instr_monitor( p_ctx ); break;
    // 0xC1 .. 0xC3 reserved
    case 0xC4: /* KILL */ instr_kill( p_ctx ); break;
    // 0xC5 .. 0xCF reserved
    case 0xD0: /* LDB L */ instr_ldb_l( p_ctx ); break;
    case 0xD1: /* LDB G */ instr_ldb_g( p_ctx ); break;
    case 0xD2: /* STB L */ instr_stb_l( p_ctx ); break;
    case 0xD3: /* STB G */ instr_stb_g( p_ctx ); break;
    case 0xD4: /* LDV L (unsigned word, little endian) */ instr_ldv_l2u_le( p_ctx ); break;
    case 0xD5: /* LDV L (signed word, little endian) */ instr_ldv_l2s_le( p_ctx ); break;
    case 0xD6: /* LDV L (double-word, little endian) */ instr_ldv_l4_le( p_ctx ); break;
    case 0xD7: /* LDV G (unsigned word, little endian) */ instr_ldv_g2u_le( p_ctx ); break;
    case 0xD8: /* LDV G (signed word, little endian) */ instr_ldv_g2s_le( p_ctx ); break;
    case 0xD9: /* LDV G (double-word, little endian) */ instr_ldv_g4_le( p_ctx ); break;
    case 0xDA: /* STV L (unsigned word, little endian) */ instr_stv_l2u_le( p_ctx ); break;
    case 0xDB: /* STV L (signed word, little endian) */ instr_stv_l2s_le( p_ctx ); break;
    case 0xDC: /* STV L (double-word, little endian) */ instr_stv_l4_le( p_ctx ); break;
    case 0xDD: /* STV G (unsigned word, little endian) */ instr_stv_g2u_le( p_ctx ); break;
    case 0xDE: /* STV G (signed word, little endian) */ instr_stv_g2s_le( p_ctx ); break;
    case 0xDF: /* STV G (double-word, little endian) */ instr_stv_g4_le( p_ctx ); break;
    // 0xE0 .. 0xFF reserved
    default: instr_err_if_u( 1, p_ctx, IE_INVOP, );
  } // switch( p_ctx->p_succ_ctx->bytecode->ptr[pc_h] )
#ifdef DEBUG_INSTR
  if( p_ctx->p_glob != NULL ) { printf( "DEBUG (after instr): " ); global_state_print( p_ctx->p_glob ); }
#endif
}


// execute instructions in executions paths
// - must be called with an active state
// - stops when all paths have reached inactive state
et_ic_status instr_exec_paths( st_instr_context *p_ctx,
                               int stop_on_first, // boolean flag if to stop when first temp. state is found
                               st_instr_tmp_succ *p_tmp_succs, // temp. states are returned here
                               unsigned int tmp_succ_cnt_max, // max. number of temp. states
                               unsigned int *p_tmp_succ_cnt // number of temp. states is returned here
                             ) // extern
{
  // process states on stack
  while( p_ctx->state_cnt > 0 )
  {

    // process next stored state
    p_ctx->state_cnt--;
    if( p_ctx->step_cnt > p_ctx->p_states[p_ctx->state_cnt].max_step_cnt ) // another step has been completed sind this state was pushed
      continue; // do not process this state (used in ELSE and UNLESS)
    p_ctx->p_glob = p_ctx->p_states[p_ctx->state_cnt].p_glob;
    p_ctx->mode = INSTR_CONTEXT_MODE_ACTIVE;
    p_ctx->invisible = 0;
    p_ctx->label = 0;
    p_ctx->flag_reg = 0;
    p_ctx->p_gvar = global_state_get_variables( p_ctx->p_glob );
    p_ctx->gvar_sz = (t_val)be2h_16( p_ctx->p_glob->gvar_sz );
    p_ctx->p_proc = global_state_get_active_process( p_ctx->p_glob );
    if( p_ctx->p_proc == NULL ) // no active process found (there should be one in every state on stack)
    {
      instr_err( p_ctx, IE_NO_ACT_PROC );
      if( p_ctx->mode == INSTR_CONTEXT_MODE_STOP )
        return IC_STOP;
      break;
    }
    p_ctx->p_lvar = process_get_variables( &p_ctx->p_proc->proc );
    p_ctx->lvar_sz = (t_val)p_ctx->p_proc->proc.lvar_sz;
    p_ctx->p_stack = process_active_get_stack( p_ctx->p_proc );
    p_ctx->p_output = p_ctx->p_states[p_ctx->state_cnt].p_output;

    // as long as active: execute instructions
    while( p_ctx->mode == INSTR_CONTEXT_MODE_ACTIVE )
      instr_exec( p_ctx );

    // save processed state if it is a completed one
    if( p_ctx->mode == INSTR_CONTEXT_MODE_COMPLETED )
    {
      if( *p_tmp_succ_cnt < tmp_succ_cnt_max )
      {
        st_instr_tmp_succ *p_tmp_succ = &p_tmp_succs[(*p_tmp_succ_cnt)++]; // get pointer to buffer entry (and advance and count)
        p_tmp_succ->p_glob = p_ctx->p_glob; // put data into buffer entry
        p_tmp_succ->invisible = p_ctx->invisible;
        p_tmp_succ->label = p_ctx->label;
        p_tmp_succ->flag_reg = p_ctx->flag_reg;
        p_tmp_succ->p_output = p_ctx->p_output;
        p_tmp_succ->sync_comm = global_state_sync_comm( p_ctx->p_glob ); // check and remember if synchronous communication is taking place
      }
      else // no more space in buffer for pointers to possible successor states
        instr_err( p_ctx, IE_SUCC_CNT );

      if( stop_on_first ) // stop on first state if requested
        break;
    }

    // error callback requested stop
    if( p_ctx->mode == INSTR_CONTEXT_MODE_STOP )
      return IC_STOP;

  } // while( p_ctx->state_cnt > 0 )

  return IC_CONTINUE;
}

