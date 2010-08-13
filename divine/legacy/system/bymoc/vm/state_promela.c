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

#include "tools.h"
#include "state.h"

#include "nipsvm.h"

#define STATE_OUT_DOTTY_LINE_END "\\l\\"


// mornfall: removed #include "state_inline.h" with STATE_INLINE set to extern,
// changed the state.h include to say static inline to avoid duplicated symbols
// (might be a clang bug, who knows)

// *** helper functions ***

// allocate memory of size sz in supplied buffer
// *pp_buf points to memory area of length *p_len to use for allocation
// NULL is returned in case of insufficient memory
static inline char * state_alloc( unsigned int sz, char **pp_buf, unsigned long *p_buf_len )
{
  if( *p_buf_len < sz ) return NULL;
  char *p_new = *pp_buf;
  *pp_buf += sz;
  *p_buf_len -= sz;
  memset( p_new, 0, sz ); // needed because of gaps in structures caused by alignment
  return p_new;
}


// *** functions visible from outside ***

// generate initial state
// *pp_buf points to memory area of length *p_len to use for new state
// NULL is returned in case of error
st_global_state_header * global_state_initial( char **pp_buf, unsigned long *p_buf_len ) // extern
{
  st_global_state_header *p_glob = (st_global_state_header *)state_alloc( global_state_initial_size, pp_buf, p_buf_len );
  if( p_glob == NULL ) return NULL;
  p_glob->gvar_sz = h2be_16( 0 ); // global header
  p_glob->proc_cnt = 1;
  p_glob->excl_pid = h2be_pid( 0 );
  p_glob->monitor_pid = h2be_pid( 0 );
  p_glob->chan_cnt = 0;
  st_process_header *p_proc = (st_process_header *)((char *)p_glob + sizeof( st_global_state_header )); // process header
  p_proc->pid = h2be_pid( 1 );
  p_proc->flags = 0;
  p_proc->lvar_sz = 0;
  p_proc->pc = h2be_pc( 0 );
  return p_glob;
}

nipsvm_state_t *
nipsvm_state_copy (size_t sz, nipsvm_state_t *p_glob, char **pp_buf,
				   unsigned long *p_buf_len)
{
	nipsvm_state_t *p_glob_new = (nipsvm_state_t *)state_alloc (sz, pp_buf, p_buf_len);
	if( p_glob_new == NULL ) return NULL;
	memcpy (p_glob_new, p_glob, sz); // simply copy
	return p_glob_new;
}

/* DEPRECATED */
st_global_state_header * global_state_copy( st_global_state_header *p_glob,
                                            char **pp_buf, unsigned long *p_buf_len ) // extern
{
  size_t sz = nipsvm_state_size( p_glob );
  return nipsvm_state_copy (sz, p_glob, pp_buf, p_buf_len);
}


// get copy of global state with resized global variables
// *pp_buf points to memory area of length *p_len to use for new state
st_global_state_header * global_state_copy_gvar_sz( st_global_state_header *p_glob,
                                                    uint16_t gvar_sz,
                                                    char **pp_buf, unsigned long *p_buf_len ) // extern
{
  unsigned int sz = nipsvm_state_size( p_glob ) - be2h_16( p_glob->gvar_sz ) + gvar_sz; // allocate
  st_global_state_header *p_glob_new = (st_global_state_header *)state_alloc( sz, pp_buf, p_buf_len );
  if( p_glob_new == NULL ) return NULL;
  char *src = (char *)p_glob;
  char *dest = (char *)p_glob_new;
  memcpy( dest, src, sizeof( st_global_state_header ) ); // header
  p_glob_new->gvar_sz = h2be_16( gvar_sz );
  dest += sizeof( st_global_state_header );
  src += sizeof( st_global_state_header );
  sz -= sizeof( st_global_state_header );
  memset( dest, 0, gvar_sz ); // variables
  memcpy( dest, src, min( gvar_sz, be2h_16( p_glob->gvar_sz ) ) );
  dest += gvar_sz;
  src += be2h_16( p_glob->gvar_sz );
  sz -= gvar_sz;
  memcpy( dest, src, sz ); // rest of state
  return p_glob_new;
}


// get copy of global state with resized local variables
// *pp_buf points to memory area of length *p_len to use for new state
st_global_state_header * global_state_copy_lvar_sz( st_global_state_header *p_glob,
                                                    st_process_header *p_proc, uint8_t lvar_sz,
                                                    char **pp_buf, unsigned long *p_buf_len ) // extern
{
  size_t sz = nipsvm_state_size( p_glob ) - p_proc->lvar_sz + lvar_sz; // allocate
  st_global_state_header *p_glob_new = (st_global_state_header *)state_alloc( sz, pp_buf, p_buf_len );
  if( p_glob_new == NULL ) return NULL;
  char *src = (char *)p_glob;
  char *dest = (char *)p_glob_new;
  unsigned int len = (char *)p_proc - (char *)p_glob; // size before process
  st_process_header *p_proc_new = (st_process_header *)((char*)p_glob_new + len); // pointer to process in new state
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE ) // size of process header
    len += sizeof( st_process_active_header );
  else
    len += sizeof( st_process_header );
  memcpy( dest, src, len ); // part before process and process header
  p_proc_new->lvar_sz = lvar_sz;
  dest += len;
  src += len;
  sz -= len;
  memset( dest, 0, lvar_sz ); // variables
  memcpy( dest, src, min( lvar_sz, p_proc->lvar_sz ) );
  dest += lvar_sz;
  src += p_proc->lvar_sz;
  sz -= lvar_sz;
  memcpy( dest, src, sz ); // rest of state
  return p_glob_new;
}


// get copy of global state with selected process activated
// *pp_buf points to memory area of length *p_len to use for new state
// *pp_proc is filled with the pointer to the activated process
// NULL is returned in case of error
st_global_state_header * global_state_copy_activate( st_global_state_header *p_glob, st_process_header *p_proc,
                                                     uint8_t stack_max, t_flag_reg flag_reg,
                                                     char **pp_buf, unsigned long *p_buf_len,
                                                     st_process_active_header **pp_proc ) // extern
{
  size_t sz = nipsvm_state_size( p_glob ) // allocate
                  - sizeof( st_process_header )
                  + sizeof( st_process_active_header )
                  + stack_max * sizeof( t_val );
  st_global_state_header *p_glob_new = (st_global_state_header *)state_alloc( sz, pp_buf, p_buf_len );
  if( p_glob_new == NULL ) return NULL;
  char *src = (char *)p_glob;
  char *dest = (char *)p_glob_new;
  unsigned int len = (char *)p_proc - (char *)p_glob; // part before process
  memcpy( dest, src, len );
  dest += len;
  src += len;
  sz -= len;
  *pp_proc = (st_process_active_header *)dest; // return pointer to activated process
  memcpy( dest, src, sizeof( st_process_header ) ); // process header
  memset( ((st_process_active_header *)dest)->registers, 0, sizeof( ((st_process_active_header *)dest)->registers ) );
  ((st_process_active_header *)dest)->flag_reg = flag_reg;
  ((st_process_active_header *)dest)->proc.flags |= PROCESS_FLAGS_ACTIVE;
  ((st_process_active_header *)dest)->stack_cur = 0;
  ((st_process_active_header *)dest)->stack_max = stack_max;
  dest += sizeof( st_process_active_header );
  src += sizeof( st_process_header );
  sz -= sizeof( st_process_active_header );
  memcpy( dest, src, p_proc->lvar_sz ); // local variables
  dest += p_proc->lvar_sz;
  src += p_proc->lvar_sz;
  sz -= p_proc->lvar_sz;
  len = stack_max * sizeof( t_val ); // stack
  memset( dest, 0, len );
  dest += len;
  sz -= len;
  memcpy( dest, src, sz ); // rest of state
  return p_glob_new;
}


// get copy of global state with an additional process
// *pp_buf points to memory area of length *p_len to use for new state
// *pp_proc is filled with the pointer to the new process
// NULL is returned in case of error
st_global_state_header * global_state_copy_new_process( st_global_state_header *p_glob,
                                                        t_pid new_pid, uint8_t lvar_sz,
                                                        char **pp_buf, unsigned long *p_buf_len,
                                                        st_process_header **pp_proc ) // extern
{
  size_t sz = nipsvm_state_size( p_glob ) // allocate
                  + sizeof( st_process_header )
                  + lvar_sz;
  st_global_state_header *p_glob_new = (st_global_state_header *)state_alloc( sz, pp_buf, p_buf_len );
  if( p_glob_new == NULL ) return NULL;
  char *src = (char *)p_glob;
  char *dest = (char *)p_glob_new;
  unsigned int len = (char *)global_state_get_channels( p_glob ) - (char *)p_glob; // part to end of last process
  memcpy( dest, src, len );
  p_glob_new->proc_cnt++;
  dest += len;
  src += len;
  sz -= len;
  *pp_proc = (st_process_header *)dest; // new process
  (*pp_proc)->pid = h2be_pid( new_pid );
  (*pp_proc)->flags = 0;
  (*pp_proc)->lvar_sz = lvar_sz;
  (*pp_proc)->pc = h2be_pc( 0 );
  dest += sizeof( st_process_header );
  sz -= sizeof( st_process_header );
  memset( dest, 0, lvar_sz );
  dest += lvar_sz;
  sz -= lvar_sz;
  memcpy( dest, src, sz ); // rest of state
  return p_glob_new;
}


// get copy of global state with an additional channel
// *pp_buf points to memory area of length *p_len to use for new state
// the new channel is inserted at the right place according to its channel id
// *pp_chan is filled with the pointer to the new process
// NULL is returned in case of error
st_global_state_header * global_state_copy_new_channel( st_global_state_header *p_glob,
                                                        t_chid new_chid, uint8_t max_len, uint8_t type_len, uint8_t msg_len,
                                                        char **pp_buf, unsigned long *p_buf_len,
                                                        st_channel_header **pp_chan ) // extern
{
  size_t len = nipsvm_state_size( p_glob );
  char * ptr = global_state_get_channels( p_glob ); // find place to insert new channel
  int i;
  for( i = 0; i < (int)p_glob->chan_cnt; i++ )
  {
    if( be2h_chid( ((st_channel_header *)ptr)->chid ) == new_chid ) // duplicate chid -> error
      return NULL;
    if( be2h_chid( ((st_channel_header *)ptr)->chid ) > new_chid ) // place found
      break;
    ptr += channel_size( (st_channel_header *)ptr );
  }
  unsigned int len_before = ptr - (char *)p_glob; // get size of part before new channel
  unsigned int len_chan = sizeof( st_channel_header ) // size of channel
                        + type_len
                        + max( 1, max_len ) * msg_len;
  unsigned int sz = len + len_chan;
  st_global_state_header *p_glob_new = (st_global_state_header *)state_alloc( sz, pp_buf, p_buf_len ); // allocate new state
  if( p_glob_new == NULL ) return NULL;
  char *src = (char *)p_glob;
  char *dest = (char *)p_glob_new;
  memcpy( dest, src, len_before ); // first part of old state
  p_glob_new->chan_cnt++;
  dest += len_before;
  src += len_before;
  sz -= len_before;
  *pp_chan = (st_channel_header *)dest; // new channel
  (*pp_chan)->chid = h2be_chid( new_chid );
  (*pp_chan)->max_len = max_len;
  (*pp_chan)->cur_len = 0;
  (*pp_chan)->msg_len = msg_len;
  (*pp_chan)->type_len = type_len;
  dest += sizeof( st_channel_header );
  sz -= sizeof( st_channel_header );
  len_chan -= sizeof( st_channel_header );
  memset( dest, 0, len_chan ); // types and content of new channel
  dest += len_chan;
  sz -= len_chan;
  memcpy( dest, src, sz ); // rest of state
  return p_glob_new;
}


// deactivate selected process in global state
void global_state_deactivate( st_global_state_header *p_glob, st_process_active_header *p_proc ) // extern
{
  size_t sz = nipsvm_state_size( p_glob )
                  + sizeof( st_process_header )
                  - sizeof( st_process_active_header )
                  - p_proc->stack_max * sizeof( t_val );
  char *src = (char *)p_glob; // no new allocation must be done
  char *dest = src;           // because state becomes always smaller here
  unsigned int len = (char *)p_proc - (char *)p_glob; // part before process
  dest += len;
  src += len;
  sz -= len;
  ((st_process_header *)dest)->flags &= ~PROCESS_FLAGS_ACTIVE; // process header
  dest += sizeof( st_process_header );
  src += sizeof( st_process_active_header );
  sz -= sizeof( st_process_header );
  uint8_t lvar_sz = p_proc->proc.lvar_sz; // get size of local variables and stack
  uint8_t stack_max = p_proc->stack_max;
  memmove( dest, src, lvar_sz ); // local variables
  dest += lvar_sz;
  src += lvar_sz;
  sz -= lvar_sz;
  src += stack_max * sizeof( t_val ); // stack
  memmove( dest, src, sz ); // rest of state
}


// remove selected process in global state
void global_state_remove( st_global_state_header *p_glob, st_process_header *p_proc ) // extern
{
  unsigned int proc_sz = process_size( p_proc );
  size_t sz = nipsvm_state_size( p_glob ) - proc_sz;
  char *src = (char *)p_glob; // no new allocation must be done
  char *dest = src;           // because state becomes always smaller here
  p_glob->proc_cnt--; // one process less
  unsigned int len = (char *)p_proc - (char *)p_glob; // part before process
  dest += len;
  src += len;
  sz -= len;
  src += proc_sz; // remove process
  memmove( dest, src, sz ); // rest of state
}


// count enabled processes (i.e. processes that are not yet terminated)
// returns number of processes
unsigned int global_state_count_enabled_processes( st_global_state_header *p_glob ) // extern
{
  char *ptr = global_state_get_processes( p_glob );
  unsigned int i, cnt = 0;
  for( i = 0; i < p_glob->proc_cnt; i++ )
  {
    if( (((st_process_header *)ptr)->flags & PROCESS_FLAGS_MODE) != PROCESS_FLAGS_MODE_TERMINATED )
      cnt++;
    ptr += process_size( (st_process_header *)ptr );
  }
  return cnt;
}


// get enabled processes (i.e. processes that may be activated)
// returns number of processes or (unsigned int)-1 if there are too many enabled processes
unsigned int global_state_get_enabled_processes( st_global_state_header *p_glob, st_process_header **p_procs, unsigned int proc_max ) // extern
{
  char *ptr = global_state_get_processes( p_glob );
  unsigned int i, cnt = 0;
  for( i = 0; i < p_glob->proc_cnt; i++ )
  {
    if( (((st_process_header *)ptr)->flags & PROCESS_FLAGS_MODE) != PROCESS_FLAGS_MODE_TERMINATED )
    {
      if( cnt >= proc_max ) return (unsigned int)-1;
      p_procs[cnt++] = (st_process_header *)ptr;
    }
    ptr += process_size( (st_process_header *)ptr );
  }
  return cnt;
}

extern int32_t
nipsvm_state_monitor_scc_id (st_bytecode *bytecode, nipsvm_state_t *p_glob)
{
  // check that there is a monitor process (or at least a pid of some former monitor process)
  t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
  if( monitor_pid == 0 ) // no monitor
    return -1;
  // get monitor process
  st_process_header *p_monitor = global_state_get_process( p_glob, monitor_pid );
  if (p_monitor == NULL) 
      return -1;
  st_scc_map *map = bytecode_scc_map (bytecode, be2h_32(p_monitor->pc));
  return map == NULL ? -1 : map->id;
}

extern int
nipsvm_state_monitor_accepting (nipsvm_state_t *p_glob)
{
	st_process_header *p_monitor = global_state_get_process( p_glob, be2h_pid( p_glob->monitor_pid ) );
	if (p_monitor == NULL)
		return 0;
	return (p_monitor->flags & PROCESS_FLAGS_MONITOR_ACCEPT) != 0; // force result 0 or 1
}

extern int
nipsvm_state_monitor_terminated (nipsvm_state_t *p_glob)
{
	// check that there is a monitor process (or at least a pid of some former monitor process)
	t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
	if( monitor_pid == 0 ) // no monitor
		return 0;
	// get monitor process
	st_process_header *p_monitor = global_state_get_process( p_glob, monitor_pid );
	// return if monitor not available or terminated
	return p_monitor == NULL
		|| (p_monitor->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED;
}

extern int
nipsvm_state_monitor_acc_or_term (nipsvm_state_t *p_glob)
{
  // check that there is a monitor process (or at least a pid of some former monitor process)
  t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
  if( monitor_pid == 0 ) // no monitor
    return 0;
  // get monitor process
  st_process_header *p_monitor = global_state_get_process( p_glob, monitor_pid );
  return p_monitor == NULL || // not availabale -> terminated
         (p_monitor->flags & PROCESS_FLAGS_MODE) == PROCESS_FLAGS_MODE_TERMINATED || // terminated
         (p_monitor->flags & PROCESS_FLAGS_MONITOR_ACCEPT) != 0; // accepting state
}

/* DEPRECATED */
int global_state_monitor_accepting( st_global_state_header *p_glob )
{
	return nipsvm_state_monitor_accepting (p_glob);
}

/* DEPRECATED */
int global_state_monitor_terminated( st_global_state_header *p_glob )
{
	return nipsvm_state_monitor_terminated (p_glob);
}

/* DEPRECATED */
int global_state_monitor_acc_or_term( st_global_state_header *p_glob )
{
	return nipsvm_state_monitor_acc_or_term (p_glob);
}

// macro to shorten output to a string
// - p_buf: pointer to buffer to print string to
// - size: total size of buffer
// - pos: "unsigned int" variable  containing current position in buffer
#define to_str( p_buf, size, pos, ... ) \
        { int cnt = snprintf( (p_buf) + pos, pos > (size) ? 0 : (size) - pos, __VA_ARGS__ ); \
          pos += cnt < 0 ? 0 : cnt; }


// output a process to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
unsigned int process_to_str( st_process_header *p_proc, int dot_fmt, char *p_buf, unsigned int size ) // extern
{
  unsigned int pos = 0;
  int i;
  char *ptr;
  ua_t_val *val_ptr;
  // dotty format?
  char line_end[] = STATE_OUT_DOTTY_LINE_END;
  if( ! dot_fmt )
     line_end[0] = 0;
  // header
  to_str( p_buf, size, pos, "  %sprocess pid=%u", p_proc->flags & PROCESS_FLAGS_ACTIVE ? "active " : "", be2h_pid( p_proc->pid ) );
  switch( p_proc->flags & PROCESS_FLAGS_MODE )
  {
    case PROCESS_FLAGS_MODE_NORMAL: to_str( p_buf, size, pos, " mode=normal" ); break;
    case PROCESS_FLAGS_MODE_ATOMIC: to_str( p_buf, size, pos, " mode=normal" ); break;
    case PROCESS_FLAGS_MODE_INVISIBLE: to_str( p_buf, size, pos, " mode=invisible" ); break;
    case PROCESS_FLAGS_MODE_TERMINATED: to_str( p_buf, size, pos, " mode=terminated" ); break;
    default: to_str( p_buf, size, pos, " mode=?" );
  }
  to_str( p_buf, size, pos, " pc=0x%08X", be2h_pc( p_proc->pc ) );
  to_str( p_buf, size, pos, " (size=%u)%s\n", process_size( p_proc ), line_end );
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE ) // process is active
  {
    to_str( p_buf, size, pos, "    registers:" );
    for( i = 0; i < 8; i++ )
      to_str( p_buf, size, pos, " r%i=%d", i, ((st_process_active_header *)p_proc)->registers[i] );
    to_str( p_buf, size, pos, "%s\n", line_end );
    to_str( p_buf, size, pos, "    flag register:" );
    for( i = FLAG_REG_FLAG_CNT - 1; i >= 0; i-- )
      to_str( p_buf, size, pos, "%s%d", (i & 7) == 7 ? " " : "",
                      ((st_process_active_header *)p_proc)->flag_reg & 1 << i ? 1 : 0 );
    to_str( p_buf, size, pos, "%s\n", line_end );
    ptr = (char *)p_proc + sizeof( st_process_active_header );
  }
  else // process is not active
    ptr = (char *)p_proc + sizeof( st_process_header );
  // variables
  to_str( p_buf, size, pos, "    variables:" );
  for( i = 0; i < (int)p_proc->lvar_sz; i++ )
    to_str( p_buf, size, pos, " 0x%02X", (unsigned char)*(ptr++) );
  to_str( p_buf, size, pos, "%s\n", line_end );
  if( p_proc->flags & PROCESS_FLAGS_ACTIVE ) // process is active
  {
    // stack
    to_str( p_buf, size, pos, "    stack (max=%d):", ((st_process_active_header *)p_proc)->stack_max );
    val_ptr = (ua_t_val *)ptr;
    for( i = 0; i < (int)((st_process_active_header *)p_proc)->stack_cur; i++ )
      to_str( p_buf, size, pos, " %d", (val_ptr++)->val );
    to_str( p_buf, size, pos, "%s\n", line_end );
  }
  return pos;
}


// output a channel to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
unsigned int channel_to_str( st_channel_header *p_chan, int dot_fmt, char *p_buf, unsigned int size ) // extern
{
  unsigned int pos = 0;
  int i, j;
  // dotty format?
  char line_end[] = STATE_OUT_DOTTY_LINE_END;
  if( ! dot_fmt )
     line_end[0] = 0;
  // header
  t_chid chid = be2h_chid( p_chan->chid );
  to_str( p_buf, size, pos, "  channel chid=0x%04X=%u-%u", chid, chid >> 8, chid & 0xFF );
  to_str( p_buf, size, pos, " max_len=%u", p_chan->max_len );
  to_str( p_buf, size, pos, " (size=%u)%s\n", channel_size( p_chan ), line_end );
  // type
  to_str( p_buf, size, pos, "    type:" );
  int8_t *p_type = (int8_t *)((char *)p_chan + sizeof( st_channel_header ));
  for( i = 0; i < (int)p_chan->type_len; i++ )
    to_str( p_buf, size, pos, " %d", p_type[i] );
  to_str( p_buf, size, pos, "%s\n", line_end );
  // messages
  to_str( p_buf, size, pos, "    messages:""%s\n", line_end );
  char *ptr = (char *)p_type + p_chan->type_len;
  for( i = 0; i < (int)p_chan->cur_len; i++ )
  {
    to_str( p_buf, size, pos, "      %u) ", i + 1 );
    for( j = 0; j < (int)p_chan->type_len; j++ )
    {
      if( p_type[j] < 0 ) // signed
      {
        if( p_type[j] >= -7 )
          { to_str( p_buf, size, pos, " %d", *(int8_t *)ptr ); ptr++; }
        else if( p_type[j] >= -15 )
          { to_str( p_buf, size, pos, " %d", be2h_16( (int16_t)ua_rd_16( ptr ) ) ); ptr += 2; }
        else
          { to_str( p_buf, size, pos, " %d", be2h_32( (int32_t)ua_rd_32( ptr ) ) ); ptr += 4; }
      }
      else // unsigned
      {
        if( p_type[j] <= 8 )
          { to_str( p_buf, size, pos, " %u", *(uint8_t *)ptr ); ptr++; }
        else if( p_type[j] <= 16 )
          { to_str( p_buf, size, pos, " %u", be2h_16( (uint16_t)ua_rd_16( ptr ) ) ); ptr += 2; }
        else
          { to_str( p_buf, size, pos, " %u", be2h_32( (uint32_t)ua_rd_32( ptr ) ) ); ptr += 4; }
      }
    }
    to_str( p_buf, size, pos, "%s\n", line_end );
  }
  return pos;
}


// output a global state to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
unsigned int global_state_to_str( st_global_state_header *p_glob, int dot_fmt, char *p_buf, unsigned int size ) // extern
{
  unsigned int pos = 0;
  int i;
  // dotty format?
  char line_end[] = STATE_OUT_DOTTY_LINE_END;
  if( ! dot_fmt )
     line_end[0] = 0;
  // header
  t_pid monitor_pid = be2h_pid( p_glob->monitor_pid );
  to_str( p_buf, size, pos, "global state excl_pid=%u monitor_pid=%u",
          be2h_pid( p_glob->excl_pid ),
          monitor_pid );
  to_str( p_buf, size, pos, " (size=%lu)%s\n", (unsigned long)nipsvm_state_size( p_glob ), line_end );
  // variables
  to_str( p_buf, size, pos, "  variables:" );
  char *ptr = (char *)p_glob + sizeof( st_global_state_header );
  for( i = 0; i < (int)be2h_16( p_glob->gvar_sz ); i++ )
    to_str( p_buf, size, pos, " 0x%02X", (unsigned char)*(ptr++) );
  to_str( p_buf, size, pos, "%s\n", line_end );
  // processes
  for( i = 0; i< (int)p_glob->proc_cnt; i++ )
  {
    int cnt = process_to_str( (st_process_header *)ptr, dot_fmt,
                              p_buf + pos, pos > size ? 0 : size - pos );
    pos += cnt < 0 ? 0 : cnt;
    ptr += process_size( (st_process_header *)ptr );
  }
  // channels
  for( i = 0; i< (int)p_glob->chan_cnt; i++ )
  {
    int cnt = channel_to_str( (st_channel_header *)ptr, dot_fmt,
                              p_buf + pos, pos > size ? 0 : size - pos );
    pos += cnt < 0 ? 0 : cnt;
    ptr += channel_size( (st_channel_header *)ptr );
  }
  // monitor flags
  if (monitor_pid != 0) {
      to_str (p_buf, size, pos, "Monitor:");
      if (nipsvm_state_monitor_acc_or_term (p_glob)) {
          to_str (p_buf, size, pos, " ACCEPTING");
      }
      if (nipsvm_state_monitor_terminated (p_glob)) {
          to_str (p_buf, size, pos, " TERMINATED");
      }
#ifdef NIPS_XXX_GLOBAL_BYTECODE
      extern st_bytecode *global_bytecode;
      int32_t scc_id = nipsvm_state_monitor_scc_id (global_bytecode, p_glob);
      t_scc_type type = bytecode_scc_type (global_bytecode, scc_id);
      to_str (p_buf, size, pos, " SCC %d (type %d)", scc_id, type);
#endif      
      to_str( p_buf, size, pos, "%s\n", line_end );
  }
  
  return pos;
}


// print a process to a stream
// - possibly to output in graphviz dot format
void process_print_ex( FILE *out, st_process_header *p_proc, int dot_fmt ) // extern
{
  unsigned int size = process_to_str( p_proc, dot_fmt, NULL, 0 ) + 1; // get needed buffer size
  {
    char buf[size]; // allocate buffer
    process_to_str( p_proc, dot_fmt, buf, size ); // convert to string
    fprintf( out, "%s", buf ); // output
  }
}


// print a channel to a stream
// - possibly to output in graphviz dot format
void channel_print_ex( FILE *out, st_channel_header *p_chan, int dot_fmt ) // extern
{
  unsigned int size = channel_to_str( p_chan, dot_fmt, NULL, 0 ) + 1; // get needed buffer size
  {
    char buf[size]; // allocate buffer
    channel_to_str( p_chan, dot_fmt, buf, size ); // convert to string
    fprintf( out, "%s", buf ); // output
  }
}


// print a global state to a stream
// - possibly to output in graphviz dot format
void global_state_print_ex( FILE *out, st_global_state_header *p_glob, int dot_fmt ) // extern
{
  unsigned int size = global_state_to_str( p_glob, dot_fmt, NULL, 0 ) + 1; // get needed buffer size
  {
    char buf[size]; // allocate buffer
    global_state_to_str( p_glob, dot_fmt, buf, size ); // convert to string
    fprintf( out, "%s", buf ); // output
  }
}

  
// print a process to stdout
void process_print( st_process_header *p_proc ) // extern
{
  process_print_ex( stdout, p_proc, 0 );  
}


// print a channel to stdout
void channel_print( st_channel_header *p_chan ) // extern
{
  channel_print_ex( stdout, p_chan, 0 );  
}


// print a global state to stdout
void global_state_print( st_global_state_header *p_glob ) // extern
{
  global_state_print_ex( stdout, p_glob, 0 );  
}
