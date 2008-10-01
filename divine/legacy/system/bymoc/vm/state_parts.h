/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_state_parts
#define INC_state_parts


#ifdef __cplusplus
extern "C"
{
#endif


#include "state.h"


// get size of a global/process/channel part from pointer
extern unsigned int glob_part_size( char *p_glob_part );
extern unsigned int proc_part_size( char *p_proc_part );
extern unsigned int chan_part_size( char *p_chan_part );


// types for callbacks to split up state
//  - called for every part of the state
//  - part is read only and only valid during callback
typedef void (*t_glob_part_cb)( char *p_glob_part, unsigned int glob_part_size, void *p_ctx );
typedef void (*t_proc_part_cb)( char *p_proc_part, unsigned int proc_part_size, void *p_ctx );
typedef void (*t_chan_part_cb)( char *p_chan_part, unsigned int chan_part_size, void *p_ctx );


// split up a global state into its parts
//  - callbacks are called with pointer to the different parts
//    - 1x glob_cb, Nx proc_cb, Mx chan_cb
//    - memory pointed to is read only and only valid within callback
extern void state_parts( st_global_state_header *p_glob, // the global state to split up
                         t_glob_part_cb glob_cb, t_proc_part_cb proc_cb, t_chan_part_cb chan_cb, // the callbacks to call
                         void *p_ctx );


// types for callbacks to restore state
//  - must copy the part into the memory pointed to by p_buf of length buf_len
//  - must return number of bytes placed into buffer (0 in case of error)
typedef unsigned int (*t_glob_restore_cb)( char *p_buf, unsigned int buf_len, void *p_ctx );
typedef unsigned int (*t_proc_restore_cb)( char *p_buf, unsigned int buf_len, void *p_ctx );
typedef unsigned int (*t_chan_restore_cb)( char *p_buf, unsigned int buf_len, void *p_ctx );


// recreate a global state from its parts
//  - callbacks are called to retrieve parts of state
//    - 1x glob_cb, Nx proc_cb, Mx chan_cb
//  - state is restored in buffer pointed to by p_buf of length buf_len
//    - pointer to start of buffer is returned, or NULL if buffer too small
extern st_global_state_header * state_restore( char *p_buf, unsigned int buf_len,
                                               t_glob_restore_cb glob_cb, t_proc_restore_cb proc_cb, t_chan_restore_cb chan_cb, // the callbacks to call
                                               void *p_ctx );


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_state_parts
