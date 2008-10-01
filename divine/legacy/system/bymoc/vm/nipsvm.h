#ifndef NIPSVM_H
#define NIPSVM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
	
#include "state.h"
#include "instr.h"
	
typedef struct nipsvm_t nipsvm_t;
//typedef st_global_state_header nipsvm_state_t; // state.h
typedef struct nipsvm_transition_information_t nipsvm_transition_information_t;
typedef st_bytecode nipsvm_bytecode_t;
typedef et_ic_status nipsvm_status_t;
typedef et_instr_err nipsvm_errorcode_t;
typedef t_pid nipsvm_pid_t;
typedef t_pc nipsvm_pc_t;

typedef	nipsvm_status_t (nipsvm_scheduler_callback_t)(size_t, nipsvm_state_t *,
													  nipsvm_transition_information_t *,
													  void *context);
typedef	nipsvm_status_t (nipsvm_error_callback_t)(nipsvm_errorcode_t, nipsvm_pid_t,
												  nipsvm_pc_t, void *context);

/* must be called once before using any other function of this
 * module.
 * Idempotent. 
 */	
extern int
nipsvm_module_init();

/* Initialize given VM structure (allocated elsewhere).
 * Returns 0 on success.
 * 
 * Usage pattern:
 *   {  nipsvm_t vm;
 *      if (nipsvm_init (&vm, ...) != 0) error();
 *      ...
 *      nipsvm_finalize (&vm); }
 */
extern int
nipsvm_init (nipsvm_t *, nipsvm_bytecode_t *,
			 nipsvm_scheduler_callback_t *, nipsvm_error_callback_t *);
extern void *
nipsvm_finalize (nipsvm_t *);


/* returned initial state is transient, must be copied by caller */
extern nipsvm_state_t *
nipsvm_initial_state (nipsvm_t *);

/* calculates successor states of ``state'' and calls callback
 * previously registered in ``vm'' for each.
 * ``context'' is given to the callback verbatim.
 */
extern unsigned int
nipsvm_scheduler_iter (nipsvm_t *vm, nipsvm_state_t *state, void *context);

static inline size_t
nipsvm_state_size (nipsvm_state_t *state) {	return global_state_size (state); }

// Copy VM state into given buffer
// *pp_buf points to memory area of length *p_len to use for new state
// Returns NULL in case of error
extern nipsvm_state_t *
nipsvm_state_copy (size_t sz, nipsvm_state_t *p_glob, char **pp_buf,
				   unsigned long *p_buf_len);

// Compare two states
// Returns 0 if equal, -1 or 1 if not equal
static inline int
nipsvm_state_compare (nipsvm_state_t *p_glob1, nipsvm_state_t *p_glob2, size_t size)
{
  return memcmp (p_glob1, p_glob2, size);
}

// Check if the monitor process is in an accepting state
// Returns 0 if no monitor exists or it is not in an accepting state
// Returns 1 if a monitor exists and is in an accpeting state
extern int
nipsvm_state_monitor_accepting (nipsvm_state_t *);


// Check if the monitor process is terminated
// Returns 0 if no monitor exists or it is not terminated
// Returns 1 if a monitor exists and is in terminated
extern int
nipsvm_state_monitor_terminated (nipsvm_state_t *);


// Check if the monitor process is in an accepting state or terminated
// (This is the equivalent function of SPIN's accepting states in the never claim.)
// Returns 0 if no monitor exists or it is not in an accepting state and not terminated
// Returns 1 if a monitor exists and is in an accpeting state or terminated
extern int
nipsvm_state_monitor_acc_or_term (nipsvm_state_t *state);


extern int
nipsvm_errorstring (char *str, size_t size, nipsvm_errorcode_t err,
					nipsvm_pid_t pid, nipsvm_pc_t pc);

extern nipsvm_error_callback_t nipsvm_default_error_cb;
	
/* Internal */
struct nipsvm_t {
	nipsvm_scheduler_callback_t *callback;
	nipsvm_error_callback_t *error_callback;
	void *callback_context;
	st_instr_succ_context insn_context;
};

static inline size_t
sizeof_nipsvm_t() {	return sizeof (nipsvm_t); }
	
#ifdef __cplusplus
} // extern "C"
#endif

#endif
