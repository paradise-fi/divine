#include <assert.h>

#include "bytecode.h"
#include "instr_step.h"
#include "nipsvm.h"

struct nipsvm_transition_information_t {
	uint8_t label[2];
	t_flag_reg flags[2];
	unsigned int succ_cb_flags;
	st_instr_succ_output *p_succ_out;
};

static et_ic_status
simple_scheduler_cb( st_global_state_header *succ,
					 uint8_t label_1st, uint8_t label,
					 t_flag_reg flag_reg_1st, t_flag_reg flag_reg,
					 unsigned int succ_cb_flags,
					 st_instr_succ_output *p_succ_out,
					 void *priv_context )
{
	nipsvm_t *vm = (nipsvm_t *)priv_context;
	
	nipsvm_transition_information_t transition_info = {
		.label = { label_1st, label },
		.flags = { flag_reg_1st, flag_reg },
		.succ_cb_flags = succ_cb_flags,
		.p_succ_out = p_succ_out,
	};

	return (*vm->callback)(nipsvm_state_size (succ), succ,
						   &transition_info, vm->callback_context);
}

static et_ic_status
simple_error_cb (et_instr_err err, t_pid pid, t_pc pc, void *priv_context)
{
    nipsvm_t *vm = (nipsvm_t *)priv_context;
    return (*vm->error_callback)(err, pid, pc, vm->callback_context);
}


extern int
nipsvm_module_init() { return 0; }

extern int
nipsvm_init (nipsvm_t *vm,
			 nipsvm_bytecode_t *bytecode,
			 nipsvm_scheduler_callback_t *s_callback,
			 nipsvm_error_callback_t *e_callback)
{
	assert (vm != NULL);
	
	vm->callback = s_callback;
    vm->error_callback = e_callback != NULL ? e_callback : nipsvm_default_error_cb;

	instr_succ_context_default (&vm->insn_context, bytecode);
	vm->insn_context.priv_context = vm;
	vm->insn_context.succ_cb = &simple_scheduler_cb;
	vm->insn_context.err_cb  = &simple_error_cb;

	return 0;
}

extern void *
nipsvm_finalize (nipsvm_t *vm)
{
	return vm;
}

extern unsigned int
nipsvm_scheduler_iter (nipsvm_t *vm,
					   nipsvm_state_t *state,
					   void *callback_context)
{
	assert (vm != NULL);
	vm->callback_context = callback_context;
	return instr_succ (state, 0, &vm->insn_context);
}

extern nipsvm_state_t *
nipsvm_initial_state (nipsvm_t *vm) 
{
	assert (vm != NULL);
	static char initial_state_buf[NIPSVM_INITIAL_STATE_SIZE];
	char *buf = initial_state_buf;
	unsigned long sz = sizeof initial_state_buf;
	return global_state_initial (&buf, &sz);
}

// standard runtime error callback function
extern nipsvm_status_t
nipsvm_default_error_cb (nipsvm_errorcode_t err, nipsvm_pid_t pid,
						 nipsvm_pc_t pc, void *priv_context)
{
	char str[256];
	// print runtime error to stderr
	nipsvm_errorstring (str, sizeof str, err, pid, pc);
	fprintf (stderr, "RUNTIME ERROR: %s\n", str);
	// do not try to find successor states
	return IC_STOP;
	// keep compiler happy
	priv_context = NULL;
}
