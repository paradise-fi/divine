/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_state
#define INC_state


#ifdef __cplusplus
extern "C"
{
#endif


#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "tools.h"

// types for process and channel identifiers
typedef uint8_t t_pid;
#define PID_MAX ((t_pid)-1)
#define PID_OK( pid ) ((pid) > 0)
#define h2be_pid( x ) (x)
#define be2h_pid( x ) (x)
typedef uint16_t t_chid; // channel-id = <pid of creating process> <1 byte to make channel-id unique>
#define CHID_MAX ((t_chid)-1)
#define CHID_OK( chid ) ((chid) > 0)
#define h2be_chid( x ) h2be_16( x )
#define be2h_chid( x ) be2h_16( x )

// type for program counter
typedef uint32_t t_pc;
#define PC_MAX ((t_pc)-1)
#define h2be_pc( x ) h2be_32( x )
#define be2h_pc( x ) be2h_32( x )

// type of flag register
typedef uint32_t t_flag_reg;
#define FLAG_REG_FLAG_CNT 32

// type for internal values (type for data stack and normal registers)
typedef int32_t t_val;
typedef uint32_t t_u_val;
// unaligned version of internal value
//  - realized as structure to be able to use PACKED
typedef struct { t_val val; } PACKED ua_t_val;


// process
typedef struct t_process_header
{
  t_pid pid; // process identifier (never 0)
  uint8_t flags; // flags (contain execution mode)
  uint8_t lvar_sz; // size of local variables in bytes
  t_pc pc; // program counter
} PACKED st_process_header;
// after header follows (if ! (flags & process_flags_active)):
//   variables (lvar_sz bytes, multi-byte values in big-endian format),

// process flags
#define PROCESS_FLAGS_MODE 0x03 // execution modes
#define PROCESS_FLAGS_MODE_NORMAL 0x00
#define PROCESS_FLAGS_MODE_ATOMIC 0x01
#define PROCESS_FLAGS_MODE_INVISIBLE 0x02
#define PROCESS_FLAGS_MODE_TERMINATED 0x03
#define PROCESS_FLAGS_MONITOR_ACCEPT 0x10 // if monitor process is in accepting state (only valid for monitor process)
#define PROCESS_FLAGS_ACTIVE 0x80 // if process is currently active (i.e. executing)

// active process
typedef struct t_process_active_header
{
  st_process_header proc; // normal process header (with flags & process_flags_active)
  t_val registers[8]; // virtual machine registers (host byte order)
  t_flag_reg flag_reg; // flag register (host byte order)
  uint8_t stack_cur; // current size of stack in entries
  uint8_t stack_max; // maximum size of stack in entries
} PACKED st_process_active_header;
// after header follows:
//   variables (proc.lvar_sz bytes, multi-byte values in big-endian format),
//   stack (stack_max ints in host byte order),


// channel
typedef struct t_channel_header
{
  t_chid chid; // channel identifier (never 0)
  uint8_t max_len; // maximum number of messages in channel (if 0, real max_len is 1)
  uint8_t cur_len; // current number of messages in channel
  uint8_t msg_len; // length of a single message in bytes
  uint8_t type_len; // number of entries of flat type
} PACKED st_channel_header;
// after header follows:
//   flat type of message (type_len bytes),
//    - bytes contain number of bits the entries use
//      (use -value+1 for negative values,
//       round up to 8, 16 or 32 and divide by 8 to get number of bytes)
//   messages (max( 1, max_len ) * msg_len bytes)
//            (multi-byte values in big-endian format)

// global state
#define PROC_CNT_MAX 255 // maximum number of processes (must fit into uint8_t, see below)
#define CHAN_CNT_MAX 255 // maximum number of channels (must fit into uint8_t, see below)
typedef struct t_global_state_header
{
  uint16_t gvar_sz; // size of global variables in bytes (big endian format)
  uint8_t proc_cnt; // number of process currently active
  t_pid excl_pid; // pid of the process executed exclusively or 0 if none
  t_pid monitor_pid; // pid of the monitor process or 0 if none
  uint8_t chan_cnt; // number of channels currently existing
} PACKED st_global_state_header;
// after header follows:
//   variables (gvar_sz bytes, multi-byte values in big-endian format),
//   processes (proc_cnt times a process),
//   channels (chan_cnt times a channel)

typedef st_global_state_header nipsvm_state_t;

// inline functions as "extern inline" for optimized compilation
#define STATE_INLINE extern inline
#include "state_inline.h"
#undef STATE_INLINE

// size of initial state
#define NIPSVM_INITIAL_STATE_SIZE (sizeof( st_global_state_header ) \
                                 + sizeof( st_process_header ))

static const size_t global_state_initial_size = NIPSVM_INITIAL_STATE_SIZE;

// generate initial state
// *pp_buf points to memory area of length *p_len to use for new state
// NULL is returned in case of error
extern st_global_state_header * global_state_initial( char **pp_buf, unsigned long *p_buf_len );


// get a copy of global state
// *pp_buf points to memory area of length *p_len to use for new state
// NULL is returned in case of error
extern st_global_state_header * global_state_copy( st_global_state_header *p_glob,
                                                   char **pp_buf, unsigned long *p_buf_len );


// get copy of global state with resized global variables
// *pp_buf points to memory area of length *p_len to use for new state
// NULL is returned in case of error
extern st_global_state_header * global_state_copy_gvar_sz( st_global_state_header *p_glob,
                                                           uint16_t gvar_sz,
                                                           char **pp_buf, unsigned long *p_buf_len );


// get copy of global state with resized local variables
// *pp_buf points to memory area of length *p_len to use for new state
// NULL is returned in case of error
extern st_global_state_header * global_state_copy_lvar_sz( st_global_state_header *p_glob,
                                                           st_process_header *p_proc, uint8_t lvar_sz,
                                                           char **pp_buf, unsigned long *p_buf_len );


// get copy of global state with selected process activated
// *pp_buf points to memory area of length *p_len to use for new state
// *pp_proc is filled with the pointer to the activated process
// NULL is returned in case of error
extern st_global_state_header * global_state_copy_activate( st_global_state_header *p_glob, st_process_header *p_proc,
                                                            uint8_t stack_max, t_flag_reg flag_reg,
                                                            char **pp_buf, unsigned long *p_buf_len,
                                                            st_process_active_header **pp_proc );


// get copy of global state with an additional process
// *pp_buf points to memory area of length *p_len to use for new state
// *pp_proc is filled with the pointer to the new process
// NULL is returned in case of error
extern st_global_state_header * global_state_copy_new_process( st_global_state_header *p_glob,
                                                               t_pid new_pid, uint8_t lvar_sz,
                                                               char **pp_buf, unsigned long *p_buf_len,
                                                               st_process_header **pp_proc );


// get copy of global state with an additional channel
// *pp_buf points to memory area of length *p_len to use for new state
// the new channel is inserted at the right place according to its channel id
// *pp_chan is filled with the pointer to the new process
// NULL is returned in case of error
extern st_global_state_header * global_state_copy_new_channel( st_global_state_header *p_glob,
                                                               t_chid new_chid, uint8_t max_len, uint8_t type_len, uint8_t msg_len,
                                                               char **pp_buf, unsigned long *p_buf_len,
                                                               st_channel_header **pp_chan );


// deactivate selected process in global state
extern void global_state_deactivate( st_global_state_header *p_glob, st_process_active_header *p_proc );


// remove selected process in global state
extern void global_state_remove( st_global_state_header *p_glob, st_process_header *p_proc );


// count enabled processes (i.e. processes that are not yet terminated)
// returns number of processes
extern unsigned int global_state_count_enabled_processes( st_global_state_header *p_glob );


// get enabled processes (i.e. processes that may be activated)
// returns number of processes or (unsigned int)-1 if there are too many enabled processes
extern unsigned int global_state_get_enabled_processes( st_global_state_header *p_glob, st_process_header **p_procs, unsigned int proc_max );


/* DEPRECATED */ extern int global_state_monitor_accepting( st_global_state_header *p_glob );
/* DEPRECATED */ extern int global_state_monitor_terminated( st_global_state_header *p_glob );
/* DEPRECATED */ extern int global_state_monitor_acc_or_term( st_global_state_header *p_glob );

// TIP: the following 3 functions can be called with ( ..., NULL, 0 )
//      to obtain the needed buffer size
//      (add 1 to return value for the terminating 0 character)


// output a process to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
extern unsigned int process_to_str( st_process_header *p_proc, int dot_fmt, char *p_buf, unsigned int size );


// output a channel to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
extern unsigned int channel_to_str( st_channel_header *p_chan, int dot_fmt, char *p_buf, unsigned int size );


// output a global state to a string
// - possibly to output in graphviz dot format
// - puts up to size charactes into *p_buf (including temintaing 0 character)
// - returns number of characters needed (e.g. size or more if string is truncated)
extern unsigned int global_state_to_str( st_global_state_header *p_glob, int dot_fmt, char *p_buf, unsigned int size );

  
// print a process to a stream
// - possibly to output in graphviz dot format
extern void process_print_ex( FILE *out, st_process_header *p_proc, int dot_fmt );


// print a channel to a stream
// - possibly to output in graphviz dot format
extern void channel_print_ex( FILE *out, st_channel_header *p_chan, int dot_fmt );


// print a global state to a stream
// - possibly to output in graphviz dot format
extern void global_state_print_ex( FILE *out, st_global_state_header *p_glob, int dot_fmt );


// print a process to stdout
extern void process_print( st_process_header *p_proc );


// print a channel to stdout
extern void channel_print( st_channel_header *p_chan );


// print a global state to stdout
extern void global_state_print( st_global_state_header *p_glob );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_state
