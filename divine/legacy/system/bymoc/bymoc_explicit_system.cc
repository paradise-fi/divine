#include <exception>
#include <wibble/test.h> // for assert

#include "system/bymoc/bymoc_explicit_system.hh"
#include "system/bymoc/bymoc_system.hh"
#include "system/state.hh"
#include "common/bit_string.hh"
#include "common/error.hh"
#include "common/deb.hh"


#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif
using std::ostream;
using std::vector;
                                

////METHODS of explicit_system:
bymoc_explicit_system_t::bymoc_explicit_system_t(error_vector_t & evect):
  system_t(evect), explicit_system_t(evect), bymoc_system_t(evect)
{
  property_decomposition = 0;
  get_abilities().system_can_decompose_property = false;  
}

process_decomposition_t *
bymoc_explicit_system_t::get_property_decomposition()
{
    return property_decomposition;
}

bymoc_explicit_system_t::~bymoc_explicit_system_t()
{
 //empty destructor
}

bool bymoc_explicit_system_t::is_erroneous(state_t state)
{
 return false;
}

bool bymoc_explicit_system_t::is_accepting(state_t state, size_int_t acc_group, size_int_t pair_member)
{
 bool accepting = nipsvm_state_monitor_acc_or_term (NIPSVM_STATE(state.ptr));
 return accepting;
}

property_type_t bymoc_explicit_system_t::get_property_type()
{
  if (get_with_property())
    return BUCHI;
  else 
    return NONE;
}

size_int_t bymoc_explicit_system_t::get_preallocation_count() const
{
 return 10000;
}

void bymoc_explicit_system_t::print_state(state_t state,
                                          std::ostream & outs)
{
  	// render string state representation into buffer ``buf''.
	size_t needed_sz = global_state_to_str (NIPSVM_STATE(state.ptr), 1/* dotty */, NULL, 0) + 1;
	char buf[needed_sz];
	
	global_state_to_str (NIPSVM_STATE(state.ptr), 1/* dotty */, buf, sizeof buf);
	outs << buf;
}

state_t bymoc_explicit_system_t::get_initial_state()
{
 nipsvm_state_t *initial_state = nipsvm_initial_state (&nipsvm);
 size_t sz = nipsvm_state_size (initial_state);
 
 state_t s = new_state(sz);
 char *tmp = (char *)NIPSVM_STATE(s.ptr);
 unsigned long tmp_size = s.size;

 nipsvm_state_t *sptr = nipsvm_state_copy (sz, initial_state, &tmp, &tmp_size);
 assert(sptr != NULL);
 return s;
}

int bymoc_explicit_system_t::get_succs(state_t state, succ_container_t & succs)
{
 succs.clear();
 successor_state_ctx ctx(succs);
 ctx.system = this;

 nipsvm_scheduler_iter (&nipsvm, NIPSVM_STATE(state.ptr), reinterpret_cast<void *>(&ctx));
 
 int result = ctx.succ_generation_type;
 if (succs.size()==0 && get_with_property()==false)
   result |= SUCC_DEADLOCK;
 
 return result;
}

int bymoc_explicit_system_t::get_ith_succ(state_t state, const int i,
                                    state_t & succ)
{
 // time: as expensive as generating all successors //TODO
 terr << "NYI: " << __FILE__ << __LINE__ << thr();
 return 0; //unreachable
}

int bymoc_explicit_system_t::get_succs(state_t state, succ_container_t & succs,
              enabled_trans_container_t & etc)
{
 terr << "bymoc_explicit_system_t::get_succs(..., enabled_trans_container_t) "
         "not implemented" << thr();
 return 0; //unreachable
}

int bymoc_explicit_system_t::get_enabled_trans(const state_t state,
              enabled_trans_container_t & enb_trans)
{
 terr << "bymoc_explicit_system_t::get_enabled_trans not implemented" << thr();
 return 0; //unreachable
}

int bymoc_explicit_system_t::get_enabled_trans_count(const state_t state,
                                                     size_int_t & count)
{
 terr << "bymoc_explicit_system_t::get_enabled_trans_count not implemented"
      << thr();
 return 0; //unreachable
}

int bymoc_explicit_system_t::get_enabled_ith_trans(const state_t state,
                                                   const size_int_t i,
                                                   enabled_trans_t & enb_trans)
{
 terr << "bymoc_explicit_system_t::get_enabled_ith_trans not implemented"
      << thr();
 return 0; //unreachable
}
                                  
bool bymoc_explicit_system_t::get_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state)
{
 terr << "bymoc_explicit_system_t::get_enabled_trans_succ not implemented"
      << thr();
 return false; //unreachable
}

bool bymoc_explicit_system_t::get_enabled_trans_succs
   (const state_t state, succ_container_t & succs,
    const enabled_trans_container_t & enabled_trans)
{
 terr << "bymoc_explicit_system_t::get_enabled_trans_succs not implemented"
      << thr();
 return false; //unreachable
}


enabled_trans_t *  bymoc_explicit_system_t::new_enabled_trans() const
{
 terr << "bymoc_explicit_system_t::new_enabled_trans not implemented" << thr();
 return 0; //unreachable
}
 
bool bymoc_explicit_system_t::eval_expr(const expression_t * const expr,
                        const state_t state,
                        data_t & data) const
{
 terr << "bymoc_explicit_system_t::eval_expr not implemented" << thr();
 return false; //unreachable
}








