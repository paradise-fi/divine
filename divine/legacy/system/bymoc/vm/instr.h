/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_instr
#define INC_instr


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>

#include "bytecode.h"
#include "state.h"
#include "tools.h"


// return value of instruction callbacks
typedef enum t_ic_status
{ 
  IC_CONTINUE, // continue (e.g. generate further successor states)
  IC_STOP, // stop processing immediately and return
} et_ic_status;


// run time errors
typedef enum t_instr_err
{
  IE_BYTECODE, // bytecode segmentation fault
  IE_INVOP, // invalid opcode
  IE_LOCAL, // invalid local memory access
  IE_GLOBAL, // invalid global memory access
  IE_STACKOV, // stack overflow
  IE_STACKUN, // stack underflow
  IE_DIV0, // division by zero
  IE_OV, // overflow error (TRUNC instruction, must be enabled in instr.c)
  IE_INDEX, // invalid array index
  IE_ASSERT, // assertion violated
  IE_INV_PROC_ID, // invalid process id
  IE_NO_PROC, // no process with specified process id
  IE_INV_CHAN_ID, // invalid channel id
  IE_NO_CHAN, // no channel with specified channel id
  IE_INV_CHAN_TYPE, // invalid channel type
  IE_CHAN_OV, // channel overflow
  IE_CHAN_UN, // channel underflow
  IE_CHAN_EMPTY, // channel is empty, but message is needed here
  IE_PROC_CNT, // too many processes
  IE_CHAN_CNT, // too many channels
  IE_PATH_CNT, // too many parallel execution paths
  IE_INVIS_CNT, // too many invisible states
  IE_SUCC_CNT, // too many possible successor states
  IE_STATE_MEM, // out of temporary state memory
  IE_NO_ACT_PROC, // no active process (this is most likely an internal error) (no pid or pc is passed to callback)
  IE_EN_PROC_CNT, // too many enabled processes (no pid or pc is passed to callback)
  IE_INVIS_MEM, // out of memory for invisible states
} et_instr_err;


// output of print instructions is stored in state buffer during instruction
// execution as linked list (pointing towards first output)

// an entry of the linked list of print instruction output
//  - must be packed because of improper alignment in state buffer
typedef struct t_instr_output
{
  struct t_instr_output *p_prev; // pointer to previous output or NULL
  int8_t is_str; // if this is a string output or not (a value output)
  union
  {
    struct // string output
    {
      uint16_t str; // number of string to print
    } str;
    struct // value output
    {
      uint8_t fmt; // format to print value in
      t_val value; // value to print
    } value;
  } data;
} PACKED st_instr_output;

// output during successor state generation
typedef struct t_instr_succ_output
{
  st_instr_output *p_out_sys1st; // output of system during 1st part of sync. comm.
  st_instr_output *p_out_sys; // output of system
  st_instr_output *p_out_monitor; // output of monitor
} st_instr_succ_output;

 
// types of callback functions

// report single single successor state to caller
// - memory pointed to by succ must not be modified or accessed outside of callback
#define INSTR_SUCC_CB_FLAG_SYNC 0x00000001 // sync. comm. took place
#define INSTR_SUCC_CB_FLAG_TIMEOUT 0x00000002 // timeout occured
#define INSTR_SUCC_CB_FLAG_SYS_BLOCK 0x00000004 // system is blocked
#define INSTR_SUCC_CB_FLAG_MONITOR_EXIST 0x00000010 // monitor process exists
#define INSTR_SUCC_CB_FLAG_MONITOR_EXEC 0x00000020 // monitor process executed
#define INSTR_SUCC_CB_FLAG_MONITOR_ACCEPT 0x00000040 // monitor process is in accepting state
#define INSTR_SUCC_CB_FLAG_MONITOR_TERM 0x00000080 // monitor process is terminated
typedef et_ic_status (*instr_succ_callback_t)( st_global_state_header *succ, // the successor state
                                               uint8_t label1st, // label of STEP command of 1st part of sync. comm. (unused if _SYNC flag is not set)
                                               uint8_t label, // label of STEP command (of 2nd part of sync. comm. if _SYNC flag is set)
                                               t_flag_reg flag_reg_1st, // flag register value returned by 1st part of sync. comm. (unused if _SYNC flag is not set)
                                               t_flag_reg flag_reg, // flag register value returned (by 2nd part of sync. comm. if _SYNC flag is set)
                                               unsigned int succ_cb_flags, // flags with boolean information
                                               st_instr_succ_output *p_succ_out, // output during successor state generation
                                               void *priv_context );

// report run time error to caller
typedef et_ic_status (*instr_err_callback_t)( et_instr_err err, // error code
                                              t_pid pid, // pid of process causing error
                                              t_pc pc, // program counter when error occured (might point into middle of instruction)
                                              void *priv_context );


// context for successor state generation
typedef struct t_instr_succ_context
{
  // maximum values (i.e. buffer sizes) used during successor state generation
  uint8_t stack_max; // number of entries in the stack during execution of a step
                     // type is uint8_t because stack pointer is an uint8_t
                     // error IE_STACK_OV will occur if too small
  unsigned int state_mem; // size of memory for temporary states within a step
                          // error IE_STATE_MEM will occur if too small
  unsigned int enab_state_mem; // size of memory for temporary states within a step in ENAB instruction
                               // this can be smaller as above because execution stops after first state
                               // error IE_STATE_MEM will occur if too small
  unsigned int invis_mem; // size of memory for invisible states stored for further processing
                          // error IE_INVIS_MEM will occur if too small
  unsigned int path_max; // maximum number of states on stack during execution of a step
                         // maximum number of possible nondeterministic paths within a step
                         // e.g. maximum number of options in Promela's "do" statement
                         // error IE_PATH_CNT will happen if too small
  unsigned int invis_max; // maximum number of invisible states that can be stored
                          // error IE_INVIS_CNT will happen if too small
  unsigned int succ_max; // maximum number of possible successor states during execution of a step
                         // maximum number of executable nondeterministic paths within a step
                         // e.g. maximum number of executable options in Promela's "do" statement
                         // error IE_SUCC_CNT will happen if too small
  // bytecode to execute
  st_bytecode *bytecode;
  // pointers to callback functions
  instr_succ_callback_t succ_cb;
  instr_err_callback_t err_cb;
  // private context of caller
  //  - will be passed to every callback function being called
  void *priv_context;
} st_instr_succ_context;


// context for execution of an instruction
#define INSTR_CONTEXT_MODE_INVALID 0 // context not initialized
#define INSTR_CONTEXT_MODE_ACTIVE 1 // a process is active, p_glob, p_gvar, gvar_sz, p_proc, p_lvar, lvar_sz, p_stack are valid
#define INSTR_CONTEXT_MODE_COMPLETED 2 // a step has been completed, p_glob is valid, invisible, label and flag_reg are set
#define INSTR_CONTEXT_MODE_STOP 3 // error callback requested stop - context not initialized
typedef struct t_instr_context_state
{
  st_global_state_header *p_glob; // pointer to global state
  st_instr_output *p_output; // pointer to last output list entry or NULL if none
  unsigned int max_step_cnt; // maximum step_cnt in context
                             // - if step_cnt in context is greater than this, ignore this state
                             // - used in ELSE and UNLESS to ignore second path if some step has already been completed
} st_instr_context_state;
typedef struct t_instr_context
{
  // current state
  int mode; // current mode of context
  int8_t invisible; // if the completed step is invisible
  uint8_t label; // label of completed step
  t_flag_reg flag_reg; // the flag register value before the step was completed
  st_global_state_header *p_glob; // the global state the instruction shall be executed on
  char *p_gvar; // the global variables within *p_glob
  t_val gvar_sz; // the size of the global variables *p_gvar already converted to t_val
  st_process_active_header *p_proc; // the active process within *p_glob
  char *p_lvar; // the local variables within *p_proc
  t_val lvar_sz; // the size of the local variables *p_lvar already converted to t_val
  ua_t_val *p_stack; // the stack of the active process *p_proc
  st_instr_output *p_output; // pointer to last output list entry or NULL if none
  // additional states to process (must always be valid)
  st_instr_context_state *p_states; // pointer to array with additonal states (stack)
  unsigned int state_cnt_max; // maximum number of entries that fit into array
  unsigned int state_cnt; // number of entries currently in array
  // buffer with memory for new temporary states (must always be valid)
  char **pp_buf;
  unsigned long *p_buf_len;
  // counter of completed steps
  unsigned int step_cnt;
  // other stuff
  t_flag_reg init_flag_reg; // initial value of flag register (needed for ENAB)
  int timeout; // boolean flag  - Promela's timeout variable
  t_pid last_pid; // pid of last process that executed a step (or 0 if not available)
  st_instr_succ_context *p_succ_ctx; // context for successor state generation
} st_instr_context;


// type for a temporary successor
typedef struct t_instr_tmp_succ
{
  st_global_state_header *p_glob; // the pointer to the state
  int8_t invisible; // if the completed step is invisible
  int8_t sync_comm; // if synchronous communication is taking place in the completed step
  uint8_t label; // the label the step was completed with
  t_flag_reg flag_reg; // value of flag register at end of step
  st_instr_output *p_output; // output list at end of step
} st_instr_tmp_succ;


// execute instructions in executions paths
// - must be called with an active state
// - stops when all paths have reached inactive state
extern et_ic_status instr_exec_paths( st_instr_context *p_ctx,
                                      int stop_on_first, // boolean flag if to stop when first temp. state is found
                                      st_instr_tmp_succ *p_tmp_succs, // temp. states are returned here
                                      unsigned int tmp_succ_cnt_max, // max. number of temp. states
                                      unsigned int *p_tmp_succ_cnt // number of temp. states is returned here
                                    );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_instr
