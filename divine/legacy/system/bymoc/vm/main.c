/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005,2006: Stefan Schuermans <stefan@schuermans.info>
 *                          Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                          Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "bytecode.h"
#include "interactive.h"
#include "search.h"
#include "state.h"


// minimal, default, maximal size of state buffer (in MB)
#define MINIMAL_BUFFER_LEN_M 1 // 1MB
#define DEFAULT_BUFFER_LEN_M 256 // 256MB
#define MAXIMAL_BUFFER_LEN_M 8192 // 8GB
// minimal, default, maximal hash table parameters (in k entries, in retries)
#define MINIMAL_HASH_ENTRIES_K 64 // 64k entries (may not be smaller than 64k because of hashtab internals)
#define DEFAULT_HASH_ENTRIES_K 4096 // 4M entries
#define MAXIMAL_HASH_ENTRIES_K 131072 // 128M entries
#define MINIMAL_HASH_RETRIES 2 // 2 retries
#define DEFAULT_HASH_RETRIES 500 // 500 retries
#define MAXIMAL_HASH_RETRIES 1000 // 1000 retries (insane)
// minimal, default, maximal size of state pointer buffer entries (in k entries)
#define MINIMAL_STATES_MAX_K 1 // 1k entries
#define DEFAULT_STATES_MAX_K 8 // 8k entries
#define MAXIMAL_STATES_MAX_K 1024 // 1M entries
// maximum values are here to ensure that user does not enter utopical values
//  - like 1e9 MB for state buffer size
//  - would result in overflow during multiplication (while converting from MB to bytes)
// the maximum values can be increased on 64 bit systems
//              and slightly increased on 32 bit systems with 36 bit addresses


// read state from hex string
// returns boolean result (if successful)
static int read_hex( char *str, char *state, int max_size )
{
  unsigned int val;
  for( ; *str != 0; )
  {
    if( sscanf( str, "%2X", &val ) != 1 )
      return 0;
    str += 2;
    if( max_size <= 0 )
      return 0;
    *state = (char)(unsigned char)val;
    state++;
    max_size--;
  }
  return 1;
}


int main( int argc, char **argv )
{
  char *bytecode_file = "", *bytecode_module = NULL, *graph_file = NULL, *ptr;
  unsigned int depth = 0;
  unsigned int lookup_addr = 0;
  unsigned long buffer_len = DEFAULT_BUFFER_LEN_M * 1048576;
  unsigned long hash_entries = DEFAULT_HASH_ENTRIES_K * 1024;
  unsigned long hash_retries = DEFAULT_HASH_RETRIES;
  unsigned long states_max = DEFAULT_STATES_MAX_K * 1024;
  unsigned long val;
  int have_bytecode_file, have_bytecode_module, breadth_first, have_depth, have_lookup;
  int random, rnd_quiet, print_hex, split, i, arg_err, graph;
  st_bytecode *bytecode;
  FILE *graph_out = NULL;
  char initial_state[1024];
  st_global_state_header *p_ini_state = NULL;

  printf( "NIPS VM version 1.2.8 date 2008-04-15\n"
          "Copyright (C) 2005,2008: Stefan Schuermans <stefan@schuermans.info>\n"
          "                         Michael Weber <Michael.Weber@cwi.nl>\n"
          "                         Lehrstuhl fuer Informatik II, RWTH Aachen\n"
          "Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html\n\n" );

  // initialize pseudo random number generator
  srand( time( NULL ) );

  // parse parameters
  arg_err = 0;
  have_bytecode_file = 0;
  have_bytecode_module = 0;
  breadth_first = 0;
  have_depth = 0;
  have_lookup = 0;
  random = 0;
  rnd_quiet = 0;
  print_hex = 0;
  split = 0;
  graph = 0;
  for( i = 1; i < argc && ! arg_err; i++ )
  {
    // breadth first search
    if( strcmp( argv[i], "-B" ) == 0 )
    {
      breadth_first = 1;
    }
    // depth first search
    else if( strcmp( argv[i], "-D" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        depth = (unsigned int)strtoul( argv[i], NULL, 0 );
        have_depth = 1;
      }
      else
	arg_err = 1;
    }
    // print states in hexadecimal
    else if( strcmp( argv[i], "-H" ) == 0 )
      print_hex = 1;
    // initial state
    else if( strcmp( argv[i], "-I" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        if( read_hex( argv[i], initial_state, sizeof( initial_state ) ) )
          p_ini_state = (st_global_state_header *)initial_state;
        else
	  arg_err = 1;
      }
      else
	arg_err = 1;
    }
    // lookup address
    else if( strcmp( argv[i], "-L" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        lookup_addr = (unsigned int)strtoul( argv[i], NULL, 0 );
        have_lookup = 1;
      }
      else
	arg_err = 1;
    }
    // random simulation
    else if( strcmp( argv[i], "-R" ) == 0 )
      random = 1, rnd_quiet = 0;
    // quiet random simulation
    else if( strcmp( argv[i], "-Rq" ) == 0 )
      random = 1, rnd_quiet = 1;
    // split up and reassemble states
    else if( strcmp( argv[i], "-S" ) == 0 )
      split = 1;
    // buffer size
    else if( strcmp( argv[i], "-b" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        val = strtoul( argv[i], NULL, 0 );
        if( val >= MINIMAL_BUFFER_LEN_M && val <= MAXIMAL_BUFFER_LEN_M )
          buffer_len = val * 1048576;
        else
          arg_err = 1;
      }
      else
	arg_err = 1;
    }
    // state graph output
    else if( strcmp( argv[i], "-g" ) == 0 )
    {
      graph = 1;
    }
    // state graph output to file
    else if( strcmp( argv[i], "-gf" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        graph = 1;
        graph_file = argv[i];
      }
      else
	arg_err = 1;
    }
    // hash table parameters
    else if( strcmp( argv[i], "-h" ) == 0 )
    {
      if( i + 2 < argc )
      {
        i++;
        val = strtoul( argv[i], NULL, 0 );
        if( val >= MINIMAL_HASH_ENTRIES_K && val <= MAXIMAL_HASH_ENTRIES_K )
          hash_entries = val * 1024;
        else
          arg_err = 1;
        i++;
        val = strtoul( argv[i], NULL, 0 );
        if( val >= MINIMAL_HASH_RETRIES && val <= MAXIMAL_HASH_RETRIES )
          hash_retries = val;
        else
          arg_err = 1;
      }
      else
	arg_err = 1;
    }
    // state pointer buffer entries
    else if( strcmp( argv[i], "-s" ) == 0 )
    {
      if( i + 1 < argc )
      {
        i++;
        val = strtoul( argv[i], NULL, 0 );
        if( val >= MINIMAL_STATES_MAX_K && val <= MAXIMAL_STATES_MAX_K )
          states_max = val * 1024;
        else
          arg_err = 1;
      }
      else
	arg_err = 1;
    }
    // unknown option
    else if( argv[i][0] == '-' )
      arg_err = 1;
    // bytecode file
    else if( ! have_bytecode_file )
    {
      bytecode_file = argv[i];
      have_bytecode_file = 1;
    }
    // bytecode module
    else if( ! have_bytecode_module )
    {
      bytecode_module = argv[i];
      have_bytecode_module = 1;
    }
    // unknown argument
    else
      arg_err = 1;
  }
  if( ! have_bytecode_file ) // no bytecode file -> ...
    arg_err = 1; // ... -> argument error

  // print usage in case of argument error  
  if( arg_err )
  {
    printf( "usage:\n"
            "  %s [options] <bytecode file> [<bytecode module>]\n\n"
	    "options:\n"
            "  -B                       perform breadth-first search\n"
            "  -D <depth>               perform depth-first search up to specified depth\n"
            "  -H                       print states in hexadecimal\n"
            "  -I <hex-state>           use supplied initial state (format: \"[0-9A-F]*\")\n"
            "  -L <address>             lookup source code location to address\n"
            "  -R                       perform random simulation\n"
            "  -Rq                      perform quiet random simulation\n"
            "  -S                       split up and reassemble states (not for -B or -D)\n"
            "  -b <buffer-size>         set buffer size to use\n"
            "                           (in MB, valid: %u..%u, default: %u)\n"
            "  -g                       output state graph in graphviz format (-B, -D)\n"
            "  -gf <file>               output state graph in graphviz format (-B, -D)\n"
            "  -h <entries> <retries>   set hash table parameters\n"
            "                           (in k entries, valid: %u..%u, default: %u,\n"
            "                            in retries, valid: %u..%u, default: %u)\n"
            "  -s <state-count>         set number of state pointer buffer entries\n"
            "                           (in k entries, valid: %u..%u, default: %u)\n\n",
	    argv[0],
            MINIMAL_BUFFER_LEN_M, MAXIMAL_BUFFER_LEN_M, DEFAULT_BUFFER_LEN_M,
            MINIMAL_HASH_ENTRIES_K, MAXIMAL_HASH_ENTRIES_K, DEFAULT_HASH_ENTRIES_K,
            MINIMAL_HASH_RETRIES, MAXIMAL_HASH_RETRIES, DEFAULT_HASH_RETRIES,
            MINIMAL_STATES_MAX_K, MAXIMAL_STATES_MAX_K, DEFAULT_STATES_MAX_K );
    return -1;
  }

  // load bytecode
  bytecode = bytecode_load_from_file( bytecode_file, bytecode_module );
  if( bytecode == NULL )
  {
    if( have_bytecode_module )
      fprintf( stderr, "could not load bytecode module \"%s\" from file \"%s\": %s\n\n",
                       bytecode_module, bytecode_file, strerror( errno ) );
    else
      fprintf( stderr, "could not load bytecode from file \"%s\": %s\n\n",
                       bytecode_file, strerror( errno ) );
    return -1;
  }

  // state graph output - generate filename
  i = 1;
  if( graph && graph_file == NULL )
  {
    if( have_bytecode_module )
      i = strlen( bytecode_file ) + strlen( bytecode_module ) + 16;
    else
      i = strlen( bytecode_file ) + 16;
  }
  char graph_file_buf[i]; // allocate buffer for filename
  if( graph && graph_file == NULL )
  {
    if( have_bytecode_module )
      sprintf( graph_file_buf, "%s_%s.dot", bytecode_file, bytecode_module );
    else
      sprintf( graph_file_buf, "%s.dot", bytecode_file );
    graph_file = graph_file_buf;
  }
  // state graph output - open file
  if( graph )
  {
    graph_out = fopen( graph_file, "wt" );
    if( graph_out == NULL )
      fprintf( stderr, "could not open graph output file \"%s\" (%s) -> no graph output\n", graph_file, strerror( errno ) );
  }
  // state graph output - print header
  if( graph_out != NULL )
  {
    fprintf( graph_out, "digraph \"" );
    for( ptr = bytecode_file; *ptr != 0; ptr++ )
      if( *ptr < ' ' || *ptr == '\'' || *ptr == '\"' || *ptr == '\\' )
        fputc( '_', graph_out );
      else
        fputc( *ptr, graph_out );
    if( have_bytecode_module )
    {
      fputc( '_', graph_out );
      for( ptr = bytecode_module; *ptr != 0; ptr++ )
        if( *ptr < ' ' || *ptr == '\'' || *ptr == '\"' || *ptr == '\\' )
          fputc( '_', graph_out );
        else
          fputc( *ptr, graph_out );
    }
    fprintf( graph_out, "\" {\n" );
  }

  // lookup address
  if( have_lookup )
  {
    st_src_loc loc = bytecode_src_loc( bytecode, lookup_addr );
    printf( "looked up address 0x%08X: line %d col %d\n\n", loc.addr, loc.line, loc.col );
  }

  // depth-first/breadth-first search 
  else if( breadth_first || have_depth )
    search( bytecode, breadth_first, depth, buffer_len, hash_entries, hash_retries,
            graph_out, print_hex, p_ini_state );

  // interactive/random simulation
  else
    interactive_simulate( bytecode, random, rnd_quiet, print_hex, split,
                          buffer_len, states_max, p_ini_state );

  // state graph output - finish and close file
  if( graph_out != NULL )
  {
    fprintf( graph_out, "}\n" );
    fclose( graph_out );
  }

  printf( "\n" );

  // unload bytecode
  bytecode_unload( bytecode );
  return 0;
}

