/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "timeval.h"

#include "rt_err.h"
#include "hashtab.h"

#include "search.h"

// context for depth-first search
typedef struct t_search_context
{
  unsigned int depth_max; // maximum depth that may be reached (only for depth-first search)
  unsigned int depth_reached; // maximum depth that was reached (only for depth-first search)
  unsigned long state_cnt; // number of states visited
  unsigned long transition_cnt; // number of transitions
  unsigned long n_atomic_steps; // number of atomic steps
  unsigned int max_state_size; // maximum state size seen
  char *p_buf; // pointer to current position in state buffer
  unsigned long buf_len; // number of bytes left in buffer
  t_hashtab hashtab; // hash table to find duplicate states
  int error; // set in case of an error
  nipsvm_t nipsvm; // context for successor state generation
  FILE *graph_out; // stream to output state graph to
  nipsvm_state_t *graph_out_pred; // pointer to predecessor state (for graph output)
  int print_hex; // if to print states in hex
} st_search_context;


// print a state in hex
static void search_print_hex( nipsvm_state_t *state )
{
  int size = nipsvm_state_size( state );
  int i;
  for( i = 0; i < size; i++ )
    printf( "%02X", (unsigned char)((char *)state)[i] );
}


// successor callback function
nipsvm_status_t
search_succ_cb (size_t succ_sz, nipsvm_state_t *succ,
				nipsvm_transition_information_t *t_info,
				void *priv_context)
{
  st_search_context *ctx = (st_search_context *)priv_context;
  nipsvm_state_t **pos, *state;
  int result;

  ++ctx->transition_cnt;
  if (succ->excl_pid != 0) {
      ++ctx->n_atomic_steps;
  }
  
  // output edge of graph
  if( ctx->graph_out != NULL )
  {
    fprintf( ctx->graph_out, "\"" );
    global_state_print_ex( ctx->graph_out, ctx->graph_out_pred, 1 );
    fprintf( ctx->graph_out, "\" -> \"" );
    global_state_print_ex( ctx->graph_out, succ, 1 );
    fprintf( ctx->graph_out, "\";\n" );
  }
  
  //print state in hex
  if( ctx->print_hex )
  {
    printf( " " );
    search_print_hex( succ );
  }

  // get position for/of state in hash table
  result = hashtab_get_pos( ctx->hashtab, succ_sz, succ, &pos );
  
  if( result < 0 )
  {
    fprintf( stderr, "RUNTIME ERROR: unresolvable hash conflict (try \"-h\")\n" );
    ctx->error = 1;
    return IC_STOP;
  }

  // state already in hash table
  if( result == 0 )
    return IC_CONTINUE;

  // copy successor state into buffer
  state = nipsvm_state_copy( succ_sz, succ, &ctx->p_buf, &ctx->buf_len );
  if( state == NULL )
  {
    fprintf( stderr, "RUNTIME ERROR: out of state memory (try \"-b\")\n" );
    ctx->error = 1;
    return IC_STOP;
  }

  // put state into hash table
  *pos = state;

  // go on finding successor states
  return IC_CONTINUE;

  // keep compiler happy
  t_info = NULL;
}


// runtime error callback function
nipsvm_status_t
search_err_cb (nipsvm_errorcode_t err, nipsvm_pid_t pid, nipsvm_pc_t pc, void *priv_context) // extern
{
  st_search_context *ctx = (st_search_context *)priv_context;
  char str[256];

  // print runtime error to stderr
  nipsvm_errorstring( str, sizeof str, err, pid, pc );
  fprintf( stderr, "RUNTIME ERROR: %s\n", str );

  // remember error and stop
  ctx->error = 1;
  return IC_STOP;
}


// depth-first search in state space
static void
search_internal_dfs_to_depth (nipsvm_state_t *state,
							  unsigned int depth_cur,
							  st_search_context *ctx)
{
  char *start, *end;
  unsigned int state_size;

  // check maximum depth
  if( depth_cur > ctx->depth_max )
    return;

  // update reached depth
  if( depth_cur > ctx->depth_reached )
    ctx->depth_reached = depth_cur;

  // count states
  ctx->state_cnt++;

  // generate successor states
  start = ctx->p_buf; // remember pointer to start of successor states
  if( ctx->print_hex )
  {
    search_print_hex( state );
    printf( " ->" );
  }
  nipsvm_scheduler_iter (&ctx->nipsvm, state, ctx);
  if( ctx->print_hex )
    printf( "\n" );
  end = ctx->p_buf; // remember pointer behind end of successor states

  // continue with depth first search
  while( start < end && ! ctx->error ) // abort on error
  {
    ctx->graph_out_pred = (nipsvm_state_t *)start;
    search_internal_dfs_to_depth( (nipsvm_state_t *)start, depth_cur + 1, ctx );
    state_size = nipsvm_state_size( (nipsvm_state_t *)start );
    start += state_size;

    // remember maximum state size
    if( state_size > ctx->max_state_size )
      ctx->max_state_size = state_size;
  }
}


// breadth-first search in state space
static void
search_internal_bfs (nipsvm_state_t *state, st_search_context *ctx) 
{
  size_t state_size;

  // as long as there is a state
  while( (char *)state < ctx->p_buf && ! ctx->error ) // abort on error
  {
    // count states
    ctx->state_cnt++;

    // generate successor states
    ctx->graph_out_pred = state;
    if( ctx->print_hex )
    {
      search_print_hex( state );
      printf( " ->" );
    }
	nipsvm_scheduler_iter (&ctx->nipsvm, state, ctx);
    if( ctx->print_hex )
      printf( "\n" );

    // state is processed (jump over it)
    state_size = nipsvm_state_size (state);
    state = (nipsvm_state_t *)((char*)state + state_size);

    // remember maximum state size
    if( state_size > ctx->max_state_size )
      ctx->max_state_size = state_size;
  }
}


// do a depth-first or breadth-first search in state space up to specified depth
// bfs: boolean flag if to do a breadth-first seach (depth_max is ignored in this case)
// buffer_len: size of state buffer
// hash_entries, hash_retries: parameters of hash table to finf duplicate states
// graph_out: if != NULL state graph will be output to this tream in graphviz dot format
// print_hex: if to print states in hex
// ini_state: state to start simulation at (or NULL to use default)
void
search (nipsvm_bytecode_t *bytecode, int bfs, unsigned int depth_max, unsigned long buffer_len,
		unsigned long hash_entries, unsigned long hash_retries, FILE *graph_out, int print_hex,
		nipsvm_state_t *ini_state) // extern
{
  char *p_buffer, *p_buf;
  t_hashtab hashtab;
  unsigned long buf_len;
  nipsvm_state_t *state;
  st_search_context ctx;
  struct timeval time_start, time_end, time_diff;
  double us_state;

  // allocate buffer for states
  p_buffer = (char *)malloc( buffer_len );
  if( p_buffer == NULL )
    rt_err( "out of memory (state buffer for search)" );
  p_buf = p_buffer;
  buf_len = buffer_len;
  // allocate hash table
  hashtab = hashtab_new( hash_entries, hash_retries );
  if( hashtab == NULL )
	  rt_err( "out of memory (hash table)" );

  // tell user to wait
  if( bfs )
    printf( "doing breadth-first search, please wait...\n" );
  else
    printf( "doing depth-first search up to depth %u, please wait...\n", depth_max );

  // remember start time
  gettimeofday( &time_start, NULL );

  // set up context for successor state generation
  nipsvm_init (&ctx.nipsvm, bytecode, &search_succ_cb, &search_err_cb);

  // get initial state
  if( ini_state == NULL ) {
	  ini_state = nipsvm_initial_state (&ctx.nipsvm);
  }

  // insert initial state into table
  {
	  size_t sz = nipsvm_state_size (ini_state);
	  state = nipsvm_state_copy (sz, ini_state, &p_buf, &buf_len);
	  hashtab_insert (hashtab, sz, ini_state);
  }
		  
  // set up rest of context for search
  ctx.depth_max = depth_max;
  ctx.depth_reached = 0;
  ctx.state_cnt = 0;
  ctx.transition_cnt = 0;
  ctx.n_atomic_steps = 0;
  ctx.max_state_size = 0;
  ctx.p_buf = p_buf;
  ctx.buf_len = buf_len;
  ctx.hashtab = hashtab;
  ctx.error = 0;
  ctx.graph_out = graph_out;
  ctx.graph_out_pred = state;
  ctx.print_hex = print_hex;

  // start search
  if( bfs )
    search_internal_bfs( state, &ctx );
  else
    search_internal_dfs_to_depth( state, 0, &ctx );
  
  nipsvm_finalize (&ctx.nipsvm);

  // update pointer and length of state buffer
  p_buf = ctx.p_buf;
  buf_len = ctx.buf_len;

  // remember end time
  gettimeofday( &time_end, NULL );
  // get time difference
  time_diff.tv_usec = time_end.tv_usec - time_start.tv_usec;
  time_diff.tv_sec = time_end.tv_sec - time_start.tv_sec;
  if( time_diff.tv_usec < 0 )
  {
    time_diff.tv_usec += 1000000;
    time_diff.tv_sec--;
  }
  // get time per state (in microseconds)
  us_state = ((double)time_diff.tv_sec * 1000000.0 + (double)time_diff.tv_usec)
           / (double)ctx.state_cnt;

  // print statistics
  table_statistics_t t_stats;
  table_statistics (hashtab, &t_stats);
  unsigned long used_buffer_space = buffer_len - buf_len;
  double table_fill_ratio = 100.0 * (double)t_stats.entries_used / (double)t_stats.entries_available;
  double state_memory_fill_ratio = 100.0 * (double)used_buffer_space / (double)buffer_len;

  printf(   "\n" );
  if( bfs )
    printf( "breadth-first search %s:\n", ctx.error ? "aborted" : "completed" );
  else
    printf( "depth-first search up to depth %u %s:\n", depth_max, ctx.error ? "aborted" : "completed" );
  printf(   "          states visited: %lu\n", ctx.state_cnt );
  printf(   "     transitions visited: %lu\n", ctx.transition_cnt );
  printf(   "            atomic steps: %lu\n", ctx.n_atomic_steps );
  printf(   "      maximum state size: %u\n", ctx.max_state_size );
  if( ! bfs )
    printf( "           reached depth: %u\n", ctx.depth_reached );
  printf(   "              total time: %lu.%06lu s\n"
            "          time per state: %f us\n", time_diff.tv_sec,
            (unsigned long)time_diff.tv_usec, us_state );
  printf(   "\n" );
  printf(   "state memory statistics:\n" );
  printf(   "                   total: %0.2fMB\n", buffer_len / 1048576.0 );
  printf(   "                    used: %0.2fMB (%0.1f%%)\n", used_buffer_space / 1048576.0,
			state_memory_fill_ratio );
  printf(   "\n" );
  printf(   "state table statistics:\n"
            "            memory usage: %0.2fMB\n", t_stats.memory_size / 1048576.0 );
  printf(   "  buckets used/available: %lu/%lu (%0.1f%%)\n", t_stats.entries_used,
			t_stats.entries_available, table_fill_ratio );
  printf(   "     max. no. of retries: %lu\n", t_stats.max_retries );
  printf(   "    conflicts (resolved): %lu\n", t_stats.conflicts );

  // free state buffer and hash table
  free( p_buffer );
  hashtab_free( hashtab );
}
