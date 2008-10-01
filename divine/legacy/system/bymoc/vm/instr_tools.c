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
#include "instr_tools.h"
#include "state.h"

#include "nipsvm.h"

// convert runtime error to string
int
nipsvm_errorstring( char *str, size_t size, nipsvm_errorcode_t err,
					nipsvm_pid_t pid, nipsvm_pc_t pc ) // extern
{
  switch( err )
  {
    case IE_BYTECODE: return snprintf( str, size, "bytecode segmentation fault (pid=%d, pc=0x%08X)", pid, pc );
    case IE_INVOP: return snprintf( str, size, "invalid opcode (pid=%d, pc=0x%08X)", pid, pc );
    case IE_GLOBAL: return snprintf( str, size, "invalid global memory access (pid=%d, pc=0x%08X)", pid, pc );
    case IE_LOCAL: return snprintf( str, size, "invalid local memory access (pid=%d, pc=0x%08X)", pid, pc );
    case IE_STACKOV: return snprintf( str, size, "stack overflow (pid=%d, pc=0x%08X)", pid, pc );
    case IE_STACKUN: return snprintf( str, size, "stack underflow (pid=%d, pc=0x%08X)", pid, pc );
    case IE_DIV0: return snprintf( str, size, "division by zero (pid=%d, pc=0x%08X)", pid, pc );
    case IE_OV: return snprintf( str, size, "overflow error (pid=%d, pc=0x%08X)", pid, pc );
    case IE_INDEX: return snprintf( str, size, "invalid array index (pid=%d, pc=0x%08X)", pid, pc );
    case IE_ASSERT: return snprintf( str, size, "assertion violated (pid=%d, pc=0x%08X)", pid, pc );
    case IE_INV_PROC_ID: return snprintf( str, size, "invalid process id (pid=%d, pc=0x%08X)", pid, pc );
    case IE_NO_PROC: return snprintf( str, size, "no process with specified process id (pid=%d, pc=0x%08X)", pid, pc );
    case IE_INV_CHAN_ID: return snprintf( str, size, "invalid channel id (pid=%d, pc=0x%08X)", pid, pc );
    case IE_NO_CHAN: return snprintf( str, size, "no channel with specified channel id (pid=%d, pc=0x%08X)", pid, pc );
    case IE_INV_CHAN_TYPE: return snprintf( str, size, "invalid channel type (pid=%d, pc=0x%08X)", pid, pc );
    case IE_CHAN_OV: return snprintf( str, size, "channel overflow (pid=%d, pc=0x%08X)", pid, pc );
    case IE_CHAN_UN: return snprintf( str, size, "channel underflow (pid=%d, pc=0x%08X)", pid, pc );
    case IE_CHAN_EMPTY: return snprintf( str, size, "channel is empty, but message is needed here (pid=%d, pc=0x%08X)", pid, pc );
    case IE_PROC_CNT: return snprintf( str, size, "too many processes (pid=%d, pc=0x%08X)", pid, pc );
    case IE_CHAN_CNT: return snprintf( str, size, "too many channels (pid=%d, pc=0x%08X)", pid, pc );
    case IE_PATH_CNT: return snprintf( str, size, "too many parallel execution paths (pid=%d, pc=0x%08X)", pid, pc );
    case IE_SUCC_CNT: return snprintf( str, size, "too many possible successor states (pid=%d, pc=0x%08X)", pid, pc );
    case IE_STATE_MEM: return snprintf( str, size, "out of temporary state memory (pid=%d, pc=0x%08X)", pid, pc );
    case IE_NO_ACT_PROC: return snprintf( str, size, "INTERNAL ERROR: no active process" ); // no pid or pc here
    case IE_EN_PROC_CNT: return snprintf( str, size, "too many enabled processes" ); // no pid or pc here
    case IE_INVIS_MEM: return snprintf( str, size, "out of memory for invisible states (pid=%d, pc=0x%08X)", pid, pc );
    default: return snprintf( str, size, "unknown error (pid=%d, pc=0x%08X)", pid, pc );
  }
}

// convert output entries (recursively) to string (without terminating 0)
// *pp_buf: pointer into buffer, advanced by this function
// *p_buf_len: remaining length of buffer, decreased by this function
// returns -1 on error, 0 on success
static int instr_output_entry_to_str( st_bytecode *bytecode, st_instr_output *p_output,
                                      char **p_buf, unsigned long *p_buf_len )
{
  char *str, buf[32];

  // nothing to print
  if( p_output == NULL )
    return 0;

  // process previous entries
  if( instr_output_entry_to_str( bytecode, p_output->p_prev, p_buf, p_buf_len ) < 0 )
    return -1;

  // string
  if( p_output->is_str )
  {
    if( p_output->data.str.str < bytecode->string_cnt )
      str = bytecode->strings[p_output->data.str.str];
    else
      sprintf( str = buf, "STRING(%d)", p_output->data.str.str );
  }

  // value
  else
  {
    switch( p_output->data.value.fmt ) // print formatted value
    {
      case 'c': case 'C': sprintf( str = buf, "%c", p_output->data.value.value ); break;
      case 'd': case 'D': sprintf( str = buf, "%d", p_output->data.value.value ); break;
      case 'e': case 'E': if( p_output->data.value.value < bytecode->string_cnt )
                            str = bytecode->strings[p_output->data.value.value];
                          else
                            sprintf( str = buf, "STRING(%d)", p_output->data.value.value );
                          break;
      case 'o': case 'O': sprintf( str = buf, "%o", p_output->data.value.value ); break;
      case 'u': case 'U': sprintf( str = buf, "%u", p_output->data.value.value ); break;
      case 'x': sprintf( str = buf, "%x", p_output->data.value.value ); break;
      case 'X': sprintf( str = buf, "%X", p_output->data.value.value ); break;
      default: sprintf( str = buf, "%d", p_output->data.value.value ); // default is decimal
    }
  }

  // copy string into buffer
  unsigned int len = strlen( str );
  if( *p_buf_len < strlen( str ) )
    return -1;
  memcpy( *p_buf, str, len );
  *p_buf += len;
  *p_buf_len -= len;

  // success
  return 0;
}


// convert output list to string
// *pp_buf: pointer into buffer, advanced by this function
// *p_buf_len: remaining length of buffer, decreased by this function
// returns pointer to 0-terminated string in buffer or NULL in case of error
char * instr_output_to_str( st_bytecode *bytecode, st_instr_output *p_output,
                            char **p_buf, unsigned long *p_buf_len ) // extern
{
  // remember start of string
  char *start = *p_buf;

  // convert output list
  if( instr_output_entry_to_str( bytecode, p_output, p_buf, p_buf_len ) < 0 )
    return NULL;

  // add terminating 0
  if( *p_buf_len < 1 )
    return NULL;
  **p_buf = 0;
  (*p_buf)++;
  (*p_buf_len)--;
  return start;
}


// convert output during successor state generation to string
// *pp_buf: pointer into buffer, advanced by this function
// *p_buf_len: remaining length of buffer, decreased by this function
// returns pointer to 0-terminated string in buffer or NULL in case of error
char * instr_succ_output_to_str( st_bytecode *bytecode, st_instr_succ_output *p_succ_out,
                                 char **p_buf, unsigned long *p_buf_len ) // extern
{
  // remember start of string
  char *start = *p_buf;

  // convert output of system during 1st part of sync. comm.
  if( instr_output_entry_to_str( bytecode, p_succ_out->p_out_sys1st, p_buf, p_buf_len ) < 0 )
    return NULL;
  // convert output of system
  if( instr_output_entry_to_str( bytecode, p_succ_out->p_out_sys, p_buf, p_buf_len ) < 0 )
    return NULL;
  // convert output of monitor
  if( instr_output_entry_to_str( bytecode, p_succ_out->p_out_monitor, p_buf, p_buf_len ) < 0 )
    return NULL;

  // add terminating 0
  if( *p_buf_len < 1 )
    return NULL;
  **p_buf = 0;
  (*p_buf)++;
  (*p_buf_len)--;
  return start;
}

