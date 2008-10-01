/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */


#include "state.h"
#include "state_parts.h"


// get size of a global/process/channel part from pointer
unsigned int glob_part_size( char *p_glob_part ) // extern
{
  return global_state_size_noproc_nochan( (st_global_state_header *)p_glob_part );
}
unsigned int proc_part_size( char *p_proc_part ) // extern
{
  return process_size( (st_process_header *)p_proc_part );
}
unsigned int chan_part_size( char *p_chan_part ) // extern
{
  return channel_size( (st_channel_header *)p_chan_part );
}


// split up a global state into its parts
//  - callbacks are called with pointer to the different parts
//    - 1x glob_cb, Nx proc_cb, Mx chan_cb
//    - memory pointed to is read only and only valid within callback
void state_parts( st_global_state_header *p_glob, // the global state to split up
                  t_glob_part_cb glob_cb, t_proc_part_cb proc_cb, t_chan_part_cb chan_cb, // the callbacks to call
                  void *p_ctx ) // extern
{
  char *ptr = (char *)p_glob;
  unsigned int sz, i;

  // report global part to callback
  sz = global_state_size_noproc_nochan( (st_global_state_header *)ptr );
  glob_cb( ptr, sz, p_ctx );
  ptr += sz;

  // report process parts to callback
  for( i = 0; i < p_glob->proc_cnt; i++ )
  {
    sz = process_size( (st_process_header *)ptr );
    proc_cb( ptr, sz, p_ctx );
    ptr += sz;
  }

  // report channel parts to callback
  for( i = 0; i < p_glob->chan_cnt; i++ )
  {
    sz = channel_size( (st_channel_header *)ptr );
    chan_cb( ptr, sz, p_ctx );
    ptr += sz;
  }
}


// recreate a global state from its parts
//  - callbacks are called to retrieve parts of state
//    - 1x glob_cb, Nx proc_cb, Mx chan_cb
//  - state is restored in buffer pointed to by p_buf of length buf_len
//    - pointer to start of buffer is returned, or NULL if buffer too small
st_global_state_header * state_restore( char *p_buf, unsigned int buf_len,
                                        t_glob_restore_cb glob_cb, t_proc_restore_cb proc_cb, t_chan_restore_cb chan_cb, // the callbacks to call
                                        void *p_ctx ) // extern
{
  st_global_state_header *p_glob = (st_global_state_header *)p_buf;
  unsigned int sz, i;

  // get global part
  sz = glob_cb( p_buf, buf_len, p_ctx );
  if( sz <= 0 || sz >= buf_len )
    return NULL;
  p_buf += sz;
  buf_len -= sz;

  // get processes
  for( i = 0; i < p_glob->proc_cnt; i++ )
  {
    sz = proc_cb( p_buf, buf_len, p_ctx );
    if( sz <= 0 || sz >= buf_len )
      return NULL;
    p_buf += sz;
    buf_len -= sz;
  }

  // get channels
  for( i = 0; i < p_glob->chan_cnt; i++ )
  {
    sz = chan_cb( p_buf, buf_len, p_ctx );
    if( sz <= 0 || sz >= buf_len )
      return NULL;
    p_buf += sz;
    buf_len -= sz;
  }

  // return assembled state
  return p_glob;
}
