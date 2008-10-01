/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <string.h>

#include "split.h"
#include "state_parts.h"
#include "tools.h"

#include "nipsvm.h"

// out context for splitting up the state and reassembling ist
typedef struct t_split_ctx
{
  char *parts[512];
  unsigned int part_cnt;
  char *p_buf;
  unsigned int buf_len;
  unsigned int parts_size;
  unsigned int restore_cnt;
} st_split_ctx;


// callback to save a part
void part_cb( char *p_part, unsigned int part_size, void *vp_ctx )
{
  st_split_ctx *p_ctx = (st_split_ctx *)vp_ctx;
  if( p_ctx->part_cnt < count( p_ctx->parts ) && // free pointer storage
      part_size <= p_ctx->buf_len ) // free buffer space
  {
    memcpy( p_ctx->p_buf, p_part, part_size ); // copy part
    p_ctx->parts[p_ctx->part_cnt] = p_ctx->p_buf; // save pointer
    p_ctx->part_cnt++; // count pointers
    p_ctx->p_buf += part_size; // reduce buffer space
    p_ctx->buf_len -= part_size;
    p_ctx->parts_size += part_size; // count total size
  }
}


// callbacks to deliver parts for restoring state
unsigned int restore_cb( char *p_buf, unsigned int buf_len, void *vp_ctx, unsigned int (*part_size)( char * ) )
{
  st_split_ctx *p_ctx = (st_split_ctx *)vp_ctx;
  if( p_ctx->restore_cnt >= p_ctx->part_cnt ) // no more parts
    return 0;
  unsigned int sz = part_size( p_ctx->parts[p_ctx->restore_cnt] ); // get size
  if( buf_len < sz ) // no more buffer space
    return 0;
  memcpy( p_buf, p_ctx->parts[p_ctx->restore_cnt], sz ); // copy part
  p_ctx->restore_cnt++; // next part
  return sz;
}
unsigned int glob_cb( char *p_buf, unsigned int buf_len, void *vp_ctx )
{
  return restore_cb( p_buf, buf_len, vp_ctx, glob_part_size );
}
unsigned int proc_cb( char *p_buf, unsigned int buf_len, void *vp_ctx )
{
  return restore_cb( p_buf, buf_len, vp_ctx, proc_part_size );
}
unsigned int chan_cb( char *p_buf, unsigned int buf_len, void *vp_ctx )
{
  return restore_cb( p_buf, buf_len, vp_ctx, chan_part_size );
}

// split a state and recreate it from its parts - then compare it
//  - used to test state_parts
//  - outputs some info if stream != NULL
//  - returns boolean flag if success
int split_test( st_global_state_header * p_glob, FILE * stream ) // extern
{
  char buffer[65536]; // some memory to store the parts of the state
  st_split_ctx ctx;
  ctx.part_cnt = 0;
  ctx.p_buf = buffer;
  ctx.buf_len = sizeof( buffer );
  ctx.parts_size = 0;
  ctx.restore_cnt = 0;

  // split up state
  state_parts( p_glob, part_cb, part_cb, part_cb, &ctx );
  if( stream != NULL )
    fprintf( stream, "splitted state into %d parts with total size %d\n",
                     ctx.part_cnt, ctx.parts_size );

  // restore state
  char restore[65536];
  st_global_state_header *p_restored = state_restore( restore, sizeof( restore ), glob_cb, proc_cb, chan_cb, &ctx );
  if( p_restored == NULL )
    return 0;

  // compare
  int ok = nipsvm_state_compare( p_glob, p_restored, nipsvm_state_size (p_glob) ) == 0;
  if( stream != NULL )
    fprintf( stream, "comparing restored state with original one: %s\n",
                     ok ? "identical" : "DIFFERENT" );
  return ok;
}

