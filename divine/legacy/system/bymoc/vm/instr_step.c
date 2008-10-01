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
#include "state.h"
#include "tools.h"


// #define DEBUG_INVIS // print invisible states


// default maximum values (i.e. buffer sizes) for successor state generation
#define INSTR_DEF_STACK_MAX 64 // number of entries in the stack during execution of a step
                               // type is uint8_t because stack pointer is an uint8_t
#ifdef SMALLSTACK // (reduce sizes for windows, because default stack size is only 1MB)
#  warning Building with reduced stack size demands.
#  define INSTR_DEF_STATE_MEM 65536 // size of memory for temporary states within a step
#  define INSTR_DEF_ENAB_STATE_MEM 8192 // size of memory for temporary states within a step in ENAB instruction
#  define INSTR_DEF_INVIS_MEM 8192 // size of memory to store invisible states for further execution
#else
#  define INSTR_DEF_STATE_MEM 262144 // size of memory for temporary states within a step
#  define INSTR_DEF_ENAB_STATE_MEM 32768 // size of memory for temporary states within a step in ENAB instruction
#  define INSTR_DEF_INVIS_MEM 32768 // size of memory to store invisible states for further execution
#endif
#define INSTR_DEF_PATH_MAX 256 // maximum number of states on stack during execution of a step
                               // maximum number of possible nondeterministic paths within a step
#define INSTR_DEF_INVIS_MAX 256 // maximum number of invisible states that can be stored
#define INSTR_DEF_SUCC_MAX 256 // maximum number of possible successor states during execution of a step
                               // maximum number of executable nondeterministic paths within a step


// type for numbers of successor states (of system and total)
typedef struct t_instr_succ_cnts
{
  unsigned int sys; // successor count of system
  unsigned int total; // total successor cnt
} st_instr_succ_cnts;


// type to save parameters of 1st part while executing 2nd part of sync. comm.
typedef struct t_instr_prm_save_sync
{
  uint8_t label; // label returned by 1st part
  t_flag_reg flag_reg; // flag register value returned by 1st part
  st_instr_output *p_output; // output done by 1st part
} st_instr_prm_save_sync;


// type to save parameters of system while executing monitor process
typedef struct t_instr_prm_save_monitor
{
  st_instr_prm_save_sync * p_prm_1st; // saved settings from 1st part of sync. comm.
  uint8_t label; // label returned by step of normal system
  t_flag_reg flag_reg; // flag register value returned by normal system
  st_instr_output *p_output; // output done by normal system
} st_instr_prm_save_monitor;


// type for internal successor callback function
typedef et_ic_status (*instr_int_succ_cb_t)( st_instr_tmp_succ * p_succ, // successor state (with label, flag_reg, ...)
                                             t_pid pid, // pid of process that executed
                                             t_flag_reg flag_reg, // initial value for flag register of process
                                             int timeout, // boolean flag if timeout
                                             unsigned int succ_cb_flags, // flags accumulated so far
                                             st_instr_succ_context *p_succ_ctx, // context for successor state generation
                                             st_instr_succ_cnts *p_succ_cnts, // successor state statistics
                                             void *p_int_ctx ); // private context


// execute a step in a process
// - callbacks in context are called for every event (e.g. errors)
// - returns instruction callback status
static et_ic_status instr_proc_step( st_global_state_header *p_glob, // state to start with
                                     st_process_header *p_proc, // process to execute
                                     t_flag_reg flag_reg, // initial value for flag register of process
                                     int timeout, // boolean flag if timeout
                                     t_pid monitor_last, // pid of process that did last step (0 if not executing monitor)
                                     unsigned int succ_cb_flags, // flags already accumulated for successor callback
                                     instr_int_succ_cb_t int_succ_cb, // internal successor callback function
                                     void *p_int_ctx, // private context for int_succ_cb
                                     char **pp_invis_buf, unsigned long *p_invis_buf_len, // buffer for invisible states
                                     st_instr_tmp_succ **pp_invis_tmp_succ, unsigned int *p_invis_tmp_succ_cnt, // array for invisible states
                                     st_instr_succ_context *p_succ_ctx, st_instr_succ_cnts *p_succ_cnts,
                                     unsigned int *p_state_cnt ) // number of resulting states
{
  // buffer for temporary states
  char tmp_buf[p_succ_ctx->state_mem];
  char *p_tmp_buf = tmp_buf;
  unsigned long tmp_buf_len = sizeof( tmp_buf );

  // activate process
  st_process_active_header *p_proc_act;
  st_global_state_header *p_glob_act = global_state_copy_activate( p_glob, p_proc,
                                                                   p_succ_ctx->stack_max, flag_reg,
                                                                   &p_tmp_buf, &tmp_buf_len,
                                                                   &p_proc_act );
  if( p_glob_act == NULL )
    return p_succ_ctx->err_cb( IE_STATE_MEM, be2h_pid( p_proc->pid ), be2h_pc( p_proc->pc ),
                               p_succ_ctx->priv_context );

  // reset monitor accept flag in process header
  p_proc_act->proc.flags &= ~PROCESS_FLAGS_MONITOR_ACCEPT;

  // set up context to execute instructions
  st_instr_context ctx;
  st_instr_context_state states[p_succ_ctx->path_max]; // create stack of states to process
  states[0].p_glob = p_glob_act;
  states[0].p_output = NULL;
  states[0].max_step_cnt = (unsigned int)-1; // process this state in any case
  ctx.p_states = states;
  ctx.state_cnt_max = count( states );
  ctx.state_cnt = 1;
  ctx.step_cnt = 0; // no step completed yet
  ctx.pp_buf = &p_tmp_buf; // fill in pointers for buffer with memory for new temporary states
  ctx.p_buf_len = &tmp_buf_len;
  ctx.init_flag_reg = flag_reg;
  ctx.timeout = timeout;
  ctx.last_pid = monitor_last;
  ctx.p_succ_ctx = p_succ_ctx;

  // execute instructions until all paths are done
  st_instr_tmp_succ tmp_succs[p_succ_ctx->succ_max];
  unsigned int tmp_succ_cnt = 0;
  if( instr_exec_paths( &ctx, 0, // do not stop on first state found
                        tmp_succs, count( tmp_succs ), &tmp_succ_cnt ) == IC_STOP )
    return IC_STOP;

  // process all completed states
  unsigned int i;
  for( i = 0; i < tmp_succ_cnt; i++ )
  {
    (*p_state_cnt)++;
    // invisible state
    if( tmp_succs[i].invisible && // state marked as invisible
        ! tmp_succs[i].sync_comm && // no synchronous communication in progress (sync. comm. forces visibility)
        tmp_succs[i].p_output == NULL ) // no output done (outout forces visibility)
    {
      // put invisible state into buffer for invisible states
      st_global_state_header * p_glb = global_state_copy( tmp_succs[i].p_glob, // copy state
                                                          pp_invis_buf, p_invis_buf_len );
      if( p_glb == NULL )
        return p_succ_ctx->err_cb( IE_INVIS_MEM, be2h_pid( p_proc->pid ), be2h_pc( p_proc->pc ),
                                   p_succ_ctx->priv_context );
      if( *p_invis_tmp_succ_cnt == 0 ) // put copy of state into array
        return p_succ_ctx->err_cb( IE_INVIS_CNT, be2h_pid( p_proc->pid ), be2h_pc( p_proc->pc ),
                                   p_succ_ctx->priv_context );
      **pp_invis_tmp_succ = tmp_succs[i];
      (**pp_invis_tmp_succ).p_glob = p_glb;
      (*pp_invis_tmp_succ)++;
      (*p_invis_tmp_succ_cnt)--;
    }
    // visible state
    else
    {
      // report state to caller
      if( int_succ_cb( &tmp_succs[i], // successor state
                       be2h_pid( p_proc->pid ), // pid of process that executed
                       flag_reg, // initial value for flag register of process
                       timeout, // boolean flag if timeout
                       succ_cb_flags, // flags accumulated so far
                       p_succ_ctx, p_succ_cnts,
                       p_int_ctx ) == IC_STOP )
        return IC_STOP;
    }
  } // for( i ...

  return IC_CONTINUE;
}


// execute steps in a process
// - callbacks in context are called for every event (e.g. errors)
// - continues execution for invisible states are reached
// - returns instruction callback status
static et_ic_status instr_proc_steps( st_global_state_header *p_glob, // state to start with
                                      st_process_header *p_proc, // process to execute
                                      t_flag_reg flag_reg, // initial value for flag register of process
                                      int timeout, // boolean flag if timeout
                                      t_pid monitor_last, // pid of process that did last step (0 if not executing monitor)
                                      unsigned int succ_cb_flags, // flags already accumulated for successor callback
                                      instr_int_succ_cb_t int_succ_cb, // internal successor callback function
                                      void *p_int_ctx, // private context for int_succ_cb
                                      st_instr_succ_context *p_succ_ctx, st_instr_succ_cnts *p_succ_cnts )
{
  // buffers for invisible states
  char invis_bufs[2][p_succ_ctx->invis_mem];
  st_instr_tmp_succ invis_tmp_succs[2][p_succ_ctx->invis_max];
  int invis_buf_idx = 0; // active buffer

  // save pid of process to execute
  t_pid exec_pid = be2h_pid( p_proc->pid );

  // initialize buffer for invisible states
  char *p_invis_buf = invis_bufs[invis_buf_idx];
  unsigned long invis_buf_len = sizeof( invis_bufs[invis_buf_idx] );
  st_instr_tmp_succ *p_invis_tmp_succs = invis_tmp_succs[invis_buf_idx];
  unsigned int invis_tmp_succ_cnt = count( invis_tmp_succs[invis_buf_idx] );

  // process initial state
  unsigned int state_cnt = 0;
  if( instr_proc_step( p_glob, p_proc,
                       flag_reg, timeout, monitor_last,
                       succ_cb_flags, int_succ_cb, p_int_ctx,
                       &p_invis_buf, &invis_buf_len,
                       &p_invis_tmp_succs, &invis_tmp_succ_cnt,
                       p_succ_ctx, p_succ_cnts, &state_cnt ) == IC_STOP )
    return IC_STOP;

  // as long as there are states in buffer
  for( ; ; )
  {
    // get pointer to and number of states in buffer
    st_instr_tmp_succ *p_tmp_succs = invis_tmp_succs[invis_buf_idx];
    unsigned int tmp_succ_cnt = count( invis_tmp_succs[invis_buf_idx] ) - invis_tmp_succ_cnt;
    // no more states ---> exit loop
    if( tmp_succ_cnt == 0 )
      break;
  
    // initialize buffer for new invisible states to use other buffer
    p_invis_buf = invis_bufs[1 - invis_buf_idx];
    invis_buf_len = sizeof( invis_bufs[1 - invis_buf_idx] );
    p_invis_tmp_succs = invis_tmp_succs[1 - invis_buf_idx];
    invis_tmp_succ_cnt = count( invis_tmp_succs[1 - invis_buf_idx] );

    // process all states in buffer
    for( ; tmp_succ_cnt > 0 ; tmp_succ_cnt--, p_tmp_succs++ )
    {
#ifdef DEBUG_INVIS
      printf( "DEBUG (invisible state): " );
      global_state_print( p_tmp_succs->p_glob );
#endif
      // get process to execute
      st_process_header *p_prc = global_state_get_process( p_tmp_succs->p_glob, exec_pid );
      if( p_prc != NULL )
      {
        unsigned int state_cnt = 0;
        // if this process became the monitor or ceased to be the monitor
        // while executing an invisible step
        // then the next step is not executable according to the formal model
        // so force this behaviour in this implementation
       if( (p_glob->monitor_pid == p_proc->pid) == (p_tmp_succs->p_glob->monitor_pid == p_prc->pid) )
       {
          // process state
          if( instr_proc_step( p_tmp_succs->p_glob, p_prc,
                               flag_reg, timeout, monitor_last,
                               succ_cb_flags, int_succ_cb, p_int_ctx,
                               &p_invis_buf, &invis_buf_len,
                               &p_invis_tmp_succs, &invis_tmp_succ_cnt,
                               p_succ_ctx, p_succ_cnts, &state_cnt ) == IC_STOP )
            return IC_STOP;
        }
        // no resulting states -> report original state to caller
        if( state_cnt == 0 )
        {
          if( int_succ_cb( p_tmp_succs, // successor state
                           exec_pid, // pid of process that executed
                           flag_reg, // initial value for flag register of process
                           timeout, // boolean flag if timeout
                           succ_cb_flags, // flags accumulated so far
                           p_succ_ctx, p_succ_cnts,
                           p_int_ctx ) == IC_STOP )
            return IC_STOP;
        }
      }
    } // for( ; tmp_succ_cnt > 0; ...

    // swap buffers
    invis_buf_idx = 1 - invis_buf_idx;

  } // for( ; ; )

  return IC_CONTINUE;
}


// execute a system step (and monitor step afterwards)
// - callbacks in context are called for every event (e.g. a successor state)
// - returns instruction callback status
static et_ic_status instr_sys_step( st_global_state_header *p_glob, // state to start with
                                    t_flag_reg flag_reg, // initial value for flag register
                                    unsigned int succ_cb_flags, // flags already accumulated for successor callback
                                    instr_int_succ_cb_t int_succ_cb, // internal successor callback function
                                    void *p_int_ctx, // private context for int_succ_cb
                                    st_instr_succ_context *p_succ_ctx, st_instr_succ_cnts *p_succ_cnts )
{
  // get enabled processes
  st_process_header *procs[PID_MAX]; // there cannot be more than PID_MAX processes
  unsigned int proc_cnt = global_state_get_enabled_processes( p_glob, procs, count( procs ) );
  if( proc_cnt == (unsigned int)-1 ) // too many enabled processes
    return p_succ_ctx->err_cb( IE_EN_PROC_CNT, 0, 0, p_succ_ctx->priv_context );

  // execute a step in every enabled process / in the process running exclusively
  unsigned int sys_succ_cnt_start = p_succ_cnts->sys;
  int timeout;
  for( timeout = 0; timeout <= 1; timeout++ ) // first try timeout = 0, then try timeout = 1
  {
    unsigned int i;

    // a process is running exclusively (and it is not the monitor)
    if( p_glob->excl_pid != h2be_pid( 0 ) && p_glob->excl_pid != p_glob->monitor_pid )
    {
      for( i = 0; i < proc_cnt; i++ ) // search this process
        if( procs[i]->pid == p_glob->excl_pid )
          break;
      if( i < proc_cnt ) // found this process
      {
        if( instr_proc_steps( p_glob, procs[i],
                              flag_reg, timeout,
                              0, // executing normal system (not monitor)
                              succ_cb_flags, int_succ_cb, p_int_ctx,
                              p_succ_ctx, p_succ_cnts ) == IC_STOP ) // execute this process
          return IC_STOP; // some callback requested stop
        if( p_succ_cnts->sys > sys_succ_cnt_start ) // process can do a step
          return IC_CONTINUE; // ignore other processes
      }
    }

    // execute every enabled process (except monitor)
    for( i = 0; i < proc_cnt; i++ )
      if( procs[i]->pid != p_glob->monitor_pid )
        if( instr_proc_steps( p_glob, procs[i],
                              flag_reg, timeout,
                              0, // executing normal system (not monitor)
                              succ_cb_flags, int_succ_cb, p_int_ctx,
                              p_succ_ctx, p_succ_cnts ) == IC_STOP )
          return IC_STOP; // some callback requested stop
    if( p_succ_cnts->sys > sys_succ_cnt_start ) // at least one successor state
      return IC_CONTINUE; // do not try again with timeout set

    // set timeout flag
    succ_cb_flags |= INSTR_SUCC_CB_FLAG_TIMEOUT;
  }
  return IC_CONTINUE;
}


// execute a monitor step
// - callbacks in context are called for every event (e.g. a successor state)
// - returns instruction callback status
static et_ic_status instr_monitor_step( st_global_state_header *p_glob, // state to start with
                                        uint8_t last_pid, // pid of process that executed last step
                                        unsigned int succ_cb_flags, // flags already accumulated for successor callback
                                        instr_int_succ_cb_t int_succ_cb, // internal successor callback function
                                        void *p_int_ctx, // private context for int_succ_cb
                                        st_instr_succ_context *p_succ_ctx, st_instr_succ_cnts *p_succ_cnts )
{
  // get monitor process if available
  t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
  st_process_header *p_monitor = NULL;
  if( monitor_pid != 0 )
    p_monitor = global_state_get_process( p_glob, monitor_pid );

  // monitor process is not available or terminated
  if( p_monitor == NULL ||
      (p_monitor->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED )
    return IC_CONTINUE;

  // monitor process exists
  succ_cb_flags |= INSTR_SUCC_CB_FLAG_MONITOR_EXIST;

  // execute a step in the monitor process
  unsigned int succ_cnt_start = p_succ_cnts->total;
  int timeout;
  for( timeout = 0; timeout <= 1; timeout++ ) // first try timeout = 0, then try timeout = 1
  {
    // execute monitor process
    if( instr_proc_steps( p_glob, p_monitor,
                          0, timeout, // monitor starts always with flag_reg = 0
                          last_pid, // executing monitor
                          succ_cb_flags, int_succ_cb, p_int_ctx,
                          p_succ_ctx, p_succ_cnts ) == IC_STOP )
      return IC_STOP; // some callback requested stop
    if( p_succ_cnts->total > succ_cnt_start ) // at least one successor state
      return IC_CONTINUE; // do not try again with timeout set
  }

  // when we get here, there was no successor state with timeout = 0 and none with timeout = 1 
  // ---> monitor process is blocked
  // ---> no successor state
  return IC_CONTINUE;
}


// internal successor callback function for monitor successor states
et_ic_status instr_monitor_succ( st_instr_tmp_succ * p_succ, // successor state (with label, flag_reg, ...)
                                 t_pid pid, // pid of process that executed
                                 t_flag_reg flag_reg, // initial value for flag register of process
                                 int timeout, // boolean flag if timeout
                                 unsigned int succ_cb_flags, // flags accumulated so far
                                 st_instr_succ_context *p_succ_ctx, // context for successor state generation
                                 st_instr_succ_cnts *p_succ_cnts, // successor state statistics
                                 void *vp_prm_sys ) // private context: saved settings of system
{
  st_instr_prm_save_monitor *p_prm_sys = (st_instr_prm_save_monitor *)vp_prm_sys;

  // synchronous communication is still going on
  if( p_succ->sync_comm )
    // synchronous communication in monitor is not allowed -> invalid state
    return IC_CONTINUE;

  // monitor executed a step
  succ_cb_flags |= INSTR_SUCC_CB_FLAG_MONITOR_EXEC;

  // set flags if monitor is in accepting state
  st_process_header *p_monitor = global_state_get_process( p_succ->p_glob, pid );
  if( p_monitor != NULL &&
      bytecode_flags( p_succ_ctx->bytecode, be2h_pc( p_monitor->pc ) ) & BC_FLAG_ACCEPT )
  {
    p_monitor->flags |= PROCESS_FLAGS_MONITOR_ACCEPT; // set flag in process header of monitor process
    succ_cb_flags |= INSTR_SUCC_CB_FLAG_MONITOR_ACCEPT; // set flag for successor callback
  }
  // set flag if monitor is terminated
  if( p_monitor == NULL ||
      (p_monitor->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED )
    succ_cb_flags |= INSTR_SUCC_CB_FLAG_MONITOR_TERM; // set flag for successor callback
  
  // count state and report it to user
  p_succ_cnts->total++;
  st_instr_succ_output succ_out = // assemble structure with output
  {
    .p_out_sys1st = p_prm_sys->p_prm_1st->p_output, // output of system during 1st part of sync. comm.
    .p_out_sys = p_prm_sys->p_output, // output of system
    .p_out_monitor = p_succ->p_output // output of monitor
  };
  return p_succ_ctx->succ_cb( p_succ->p_glob, // report resulting state
                              p_prm_sys->p_prm_1st->label, p_prm_sys->label, // pass on saved settings of system
                              p_prm_sys->p_prm_1st->flag_reg, p_prm_sys->flag_reg,
                              succ_cb_flags, // pass on calulated flags
                              &succ_out,
                              p_succ_ctx->priv_context );

  // keep compiler happy
  flag_reg = 0;
  timeout = 0;
}


// execute monitor on system successor state
static et_ic_status instr_exec_monitor( st_instr_tmp_succ *p_sys_succ, // successor state of system (with label, flag_reg, ...)
                                        st_instr_prm_save_sync *p_prm_1st, // saved parameters of 1st part of sync. comm. of system
                                        uint8_t last_pid, // pid of process that executed last step
                                        unsigned int succ_cb_flags, // flags already accumulated for successor callback
                                        st_instr_succ_context *p_succ_ctx, st_instr_succ_cnts *p_succ_cnts )
{
  // count system's successor state
  //  - monitor is executed on every successor state of system
  //  - so system's successor states can be counted here
  p_succ_cnts->sys++;

  // save parameters returned by normal system
  st_instr_prm_save_monitor prm_sys =
  {
    .p_prm_1st = p_prm_1st, // saved settings from 1st part of sync. comm.
    .label = p_sys_succ->label, // label returned by normal system
    .flag_reg = p_sys_succ->flag_reg, // flag register value returned by normal system
    .p_output = p_sys_succ->p_output, // output done by normal system
  };
  
  // there is a monitor process (or at least a pid of some former monitor process)
  if( p_sys_succ->p_glob->monitor_pid != h2be_pid( 0 ) )
  {
    // execute a monitor step
    if( instr_monitor_step( p_sys_succ->p_glob, last_pid, succ_cb_flags,
                            instr_monitor_succ, (void *)&prm_sys, // monitor callback with saved settings of system as context
                            p_succ_ctx, p_succ_cnts ) == IC_STOP )
      return IC_STOP; // some callback requested stop
    return IC_CONTINUE;
  }

  // when we get here, there is no monitor process

  // count original system successor state and report it to user
  p_succ_cnts->total++;
  st_instr_succ_output succ_out = // assemble structure with output
  {
    .p_out_sys1st = p_prm_1st->p_output, // output of system during 1st part of sync. comm.
    .p_out_sys = p_sys_succ->p_output, // output of system
    .p_out_monitor = NULL // no output of monitor
  };
  return p_succ_ctx->succ_cb( p_sys_succ->p_glob, // report system state
                              p_prm_1st->label, p_sys_succ->label, // pass on settings of system
                              p_prm_1st->flag_reg, p_sys_succ->flag_reg,
                              succ_cb_flags, // pass on flags of system
                              &succ_out,
                              p_succ_ctx->priv_context );
}


// internal successor callback function for 2nd part of synchronous communication
et_ic_status instr_sync_succ( st_instr_tmp_succ * p_succ, // successor state (with label, flag_reg, ...)
                              t_pid pid, // pid of process that executed
                              t_flag_reg flag_reg, // initial value for flag register of process
                              int timeout, // boolean flag if timeout
                              unsigned int succ_cb_flags, // flags accumulated so far
                              st_instr_succ_context *p_succ_ctx, // context for successor state generation
                              st_instr_succ_cnts *p_succ_cnts, // successor state statistics
                              void *vp_prm_1st ) // private context: saved settings of 1st part of sync. comm.
{
  st_instr_prm_save_sync *p_prm_1st = (st_instr_prm_save_sync *)vp_prm_1st;

  // synchronous communication is still going on
  if( p_succ->sync_comm )
    // this is not a valid state
    return IC_CONTINUE;

  // execute monitor
  return instr_exec_monitor( p_succ, // start from system successor state
                             p_prm_1st, // with saved setting from 1st part of sync. comm.
                             pid, // tell monitor process which process did the last step
                             succ_cb_flags, // pass on flags
                             p_succ_ctx, p_succ_cnts );

  // keep compiler happy
  flag_reg = 0;
  timeout = 0;
}


// internal successor callback function for system successor states
et_ic_status instr_sys_succ( st_instr_tmp_succ * p_succ, // successor state (with label, flag_reg, ...)
                             t_pid pid, // pid of process that executed
                             t_flag_reg flag_reg, // initial value for flag register of process
                             int timeout, // boolean flag if timeout
                             unsigned int succ_cb_flags, // flags accumulated so far
                             st_instr_succ_context *p_succ_ctx, // context for successor state generation
                             st_instr_succ_cnts *p_succ_cnts, // successor state statistics
                             void *p_unused ) // private context
{
  // no synchronous communication is going on
  if( ! p_succ->sync_comm )
  {
    // create some dummy parameters to pass on
    st_instr_prm_save_sync dummy_1st = { .label = 0, .flag_reg = 0, .p_output = NULL };
    // execute monitor
    return instr_exec_monitor( p_succ, // start from system successor state
                               &dummy_1st, // with saved setting from 1st part of sync. comm.
                               pid, // tell monitor process which process did the last step
                               succ_cb_flags, // pass on flags
                               p_succ_ctx, p_succ_cnts );
  }

  // save parameters of 1st part for 2nd part
  st_instr_prm_save_sync prm_1st =
  {
    .label = p_succ->label, // label returned by 1st part
    .flag_reg = p_succ->flag_reg, // flag register value returned by 1st part
    .p_output = p_succ->p_output, // output done by 1st part
  };

  // get enabled processes
  st_process_header *procs[PID_MAX]; // there cannot be more than PID_MAX processes
  unsigned int proc_cnt = global_state_get_enabled_processes( p_succ->p_glob, procs, count( procs ) );
  if( proc_cnt == (unsigned int)-1 ) // too many enabled processes
    return p_succ_ctx->err_cb( IE_EN_PROC_CNT, 0, 0, p_succ_ctx->priv_context );

  // let possible receivers receive message synchronously
  unsigned int i;
  for( i = 0; i < proc_cnt; i++ ) // execute every enabled process
    if( procs[i]->pid != p_succ->p_glob->monitor_pid && // except monitor and sender
        procs[i]->pid != h2be_pid( pid ) )
      if( instr_proc_steps( p_succ->p_glob, procs[i],
                            flag_reg, timeout, // start with same flag_reg value and timeout setting as 1st part
                            0, // executing normal system (not monitor)
                            succ_cb_flags | INSTR_SUCC_CB_FLAG_SYNC, // add flag for synchronous communication
                            instr_sync_succ, (void *)&prm_1st, // sync. comm. callback with saved settings of 1st part as context
                            p_succ_ctx, p_succ_cnts ) == IC_STOP )
      return IC_STOP; // some callback requested stop

  return IC_CONTINUE;

  // keep compiler happy
  p_unused = NULL;
}


// *** functions visible from outside ***


// initialize context with default maximum values
// only fills in maximum values and bytecode
// does not touch pointers to callback functions
void instr_succ_context_default( st_instr_succ_context *succ_ctx, st_bytecode *bytecode ) // extern
{
  succ_ctx->stack_max = INSTR_DEF_STACK_MAX;
  succ_ctx->state_mem = INSTR_DEF_STATE_MEM;
  succ_ctx->enab_state_mem = INSTR_DEF_ENAB_STATE_MEM;
  succ_ctx->invis_mem = INSTR_DEF_INVIS_MEM;
  succ_ctx->path_max = INSTR_DEF_PATH_MAX;
  succ_ctx->invis_max = INSTR_DEF_INVIS_MAX;
  succ_ctx->succ_max = INSTR_DEF_SUCC_MAX;
  succ_ctx->bytecode = bytecode;
}


// generate successor states
// callbacks in context are called for every event (e.g. a successor state)
// returns number of successor states that were reported to successor callback
unsigned int instr_succ( st_global_state_header *p_glob, // state to start with
                         t_flag_reg flag_reg, // value to put into flag register
                         st_instr_succ_context *succ_ctx ) // extern
{
  // get monitor process in original state
  t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
  st_process_header *p_monitor = NULL;

  // there is a monitor process (or at least a pid of some former monitor process)
  if( monitor_pid != 0 )
  {
    // get monitor process
    p_monitor = global_state_get_process( p_glob, monitor_pid );
    // monitor not available or terminated
    if( p_monitor == NULL ||
        (p_monitor->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED )
    {
      // report original state as successor state to user
      st_instr_succ_output succ_out = // structure with output: none
      {
        .p_out_sys1st = NULL, // output of system during 1st part of sync. comm.
        .p_out_sys = NULL, // output of system
        .p_out_monitor = NULL // output of monitor
      };
         succ_ctx->succ_cb( p_glob, // report original state
                            0, 0, 0, 0,
                            INSTR_SUCC_CB_FLAG_MONITOR_EXIST | INSTR_SUCC_CB_FLAG_MONITOR_TERM,
                            &succ_out,
                            succ_ctx->priv_context );
         return 1;
    }
  }

  // execute a step in system (and monitor step afterwards)
  st_instr_succ_cnts succ_cnts = { .sys = 0, .total = 0 };
  instr_sys_step( p_glob, flag_reg,
                  0, // no successor callback flags yet
                  instr_sys_succ, NULL, // system successor callback and context
                  succ_ctx, &succ_cnts );
  // done if at least one successor state of system
  if( succ_cnts.sys > 0 )
    return succ_cnts.total;

  // when we get here, the system is blocked

  // extension of finite paths only if monitor is present.
  if (monitor_pid == 0)
      return 0;
  
  // report original state as successor state to user
  st_instr_succ_output succ_out = // structure with output: none
  {
    .p_out_sys1st = NULL, // output of system during 1st part of sync. comm.
    .p_out_sys = NULL, // output of system
    .p_out_monitor = NULL // output of monitor
  };
  succ_ctx->succ_cb( p_glob, // report original state
                     0, 0, 0, 0,
                     INSTR_SUCC_CB_FLAG_SYS_BLOCK |
                     (p_monitor != NULL ? INSTR_SUCC_CB_FLAG_MONITOR_EXIST : 0) |
                     (p_monitor != NULL && (p_monitor->flags & PROCESS_FLAGS_MONITOR_ACCEPT) ? INSTR_SUCC_CB_FLAG_MONITOR_ACCEPT : 0),
                     &succ_out,
                     succ_ctx->priv_context );
  return 1;
}

