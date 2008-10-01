/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//#include "bytecode.h"
//#include "instr.h"
//#include "instr_step.h"
#include "instr_wrap.h"
#include "rt_err.h"
#include "split.h"
//#include "state.h"
//#include "tools.h"

#include "interactive.h"


// do an interactive simulation stating from the initial state
// random: boolean flag if to replace interactivity by randomness
// rnd_quiet: if to suppress state output during random simulation
// print_hex: boolean flag if to print current states in hexadecimal
// split: boolean flag if to split up and reassemble states
// buffer_len: size of state buffer
// states_max: number of state pointer buffer entries
// ini_state: state to start simulation at (or NULL to use default)
void interactive_simulate( nipsvm_bytecode_t *bytecode,
						   int random, int rnd_quiet, int print_hex, int split,
                           unsigned long buffer_len, unsigned int states_max,
                           nipsvm_state_t *ini_state ) // extern
{
  char *p_buffer, *p_buf;
  nipsvm_state_t *p_cur;
  t_flag_reg flag_reg;
  st_instr_succ_buf_ex_state *states;
  unsigned long buf_len, cnt, i, j, sel, len;
  st_instr_succ_context succ_ctx;

  // does monitor process exist?
  if( bytecode_monitor_present( bytecode ) )
    printf( "monitor process exists\n\n" );

  // allocate buffer for states
  p_buffer = (char *)malloc( buffer_len );
  if( p_buffer == NULL )
    rt_err( "out of memory (state buffer for interactive/random simulation)" );
  // allocate buffer for successor state pointers
  states = (st_instr_succ_buf_ex_state *)malloc( states_max * sizeof( st_instr_succ_buf_ex_state ) );
  if( states == NULL )
    rt_err( "out of memory (successor state pointer buffer for interactive/random simulation)" );

  // get initial state
  p_buf = p_buffer;
  buf_len = buffer_len;
  if( ini_state != NULL )
    p_cur = global_state_copy( ini_state, &p_buf, &buf_len );
  else
    p_cur = global_state_initial( &p_buf, &buf_len );
  if( p_cur == NULL )
    rt_err( "state buffer too small for initial state" );
  flag_reg = 0;

  // initialize context for instr_succ_buf_ex
  instr_succ_buf_ex_prepare( &succ_ctx, bytecode );

  // scheduler loop
  for( ; ; )
  {

    // show current state
    if( ! random || ! rnd_quiet )
    {
      printf( "=== current state ===\n" );
      global_state_print( p_cur );
      printf( "ext info: flags=0x%X\n", flag_reg );
      printf( "\n" );

      // print current state in hexadecimal
      if( print_hex )
      {
        len = nipsvm_state_size( p_cur );
        for( i = 0, j = 0; i < len; )
        {
          printf( "%04lX:", i );
          for( j = 0; i < len && j < 0x10; i++, j++ )
            printf( " %02X", ((unsigned char *)p_cur)[i] );
          printf( "\n" );
        }
        printf( "\n" );
      }
    }

    // split up and reassemble
    if( split )
    {
      if( ! split_test( p_cur, stdout ) )
      {
        fprintf( stderr, "split test failed\n" );
        break;
      }
      printf( "\n" );
    }

    // generate successor states
    cnt = instr_succ_buf_ex( &succ_ctx, p_cur, flag_reg, 1 /* process output */,
                             states, states_max, &p_buf, &buf_len );

    if( random )
    {

      // select a random state
      if( cnt == 0 )
        sel = 1;
      else
      {
        sel = rand( ) % cnt;
        if( states[sel].p_output[0] != 0 ) // show output
        {
          if( rnd_quiet )
            printf( "%s", states[sel].p_output );
          else
            printf( "output: \"%s\"\n\n", states[sel].p_output );
        }
      }

    }
    else
    {
      
      // show successor states
      printf( "\n=== %ld successor state%s ===\n", cnt, cnt == 1 ? "" : "s" );
      for( i = 0; i < cnt; i++ )
      {
        printf( "--- successor state %ld ---\n", i + 1 );
        global_state_print( states[i].p_state );
        printf( "ext info: " ); // show extended information
        if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_SYNC )
          printf( "SYNC, label=%u flags=0x%X, label=%u flags=0x%X",
                  states[i].label_1st, states[i].flag_reg_1st,
                  states[i].label, states[i].flag_reg );
        else
          printf( "label=%u flags=0x%X",
                  states[i].label, states[i].flag_reg );
        if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_TIMEOUT )
          printf( ", TIMEOUT" );
        if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_SYS_BLOCK )
          printf( ", SYS_BLOCK" );
        printf( "\n" );
        if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_MONITOR_EXIST )
        {
          printf( "monitor: exists" );
          if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_MONITOR_EXEC )
            printf( ", executed" );
          if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_MONITOR_ACCEPT )
            printf( ", ACCEPT" );
          if( states[i].succ_cb_flags & INSTR_SUCC_CB_FLAG_MONITOR_TERM )
            printf( ", TERM" );
          printf( "\n" );
        }
        if( states[i].p_output[0] != 0 ) // show output
          printf( "output: \"%s\"\n", states[i].p_output );
      }
      printf( "\n" );

      // let user select a state
      if( cnt == 0 )
        printf( "enter something to quit: " );
      else if( cnt == 1 )
        printf( "enter state number (1) or anything else to quit: " );
      else
        printf( "enter state number (1..%ld) or anything else to quit: ", cnt );
      fflush( stdout );
      scanf( "%lu", &sel );
      printf( "\n" );
      sel--; // convert to 0 based index

    }

    // end requested
    if( sel >= cnt )
      break;
    // system blocked in random mode -> end
    if( random && states[sel].succ_cb_flags & INSTR_SUCC_CB_FLAG_SYS_BLOCK )
      break;

    // keep flag register of selected state (just for testing)
    flag_reg = states[sel].flag_reg;
    
    // move selected state to beginning of buffer and use it as current state
    len = nipsvm_state_size( states[sel].p_state );
    memmove( p_buffer, states[sel].p_state, len );
    p_buf = p_buffer + len;
    buf_len = buffer_len - len;
    p_cur = (nipsvm_state_t *)p_buffer;

  } // for ( ; ; )

  // free malloc-ed memory buffers
  free( p_buffer );
  free( states );
}
