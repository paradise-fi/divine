#include <fstream>
#include <sstream>
#include "system/bymoc/bymoc_system.hh"
#include "system/bymoc/vm/nipsvm.h"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

//from unknown reason the structure is defined in system/bymoc/vm/nipsvm.c
//(not in header file) => copy of definition needed here
struct nipsvm_transition_information_t {
	uint8_t label[2];
	t_flag_reg flags[2];
	unsigned int succ_cb_flags;
	st_instr_succ_output *p_succ_out;
};

nipsvm_status_t
successor_state_callback (size_t sz, nipsvm_state_t *succ,
                          nipsvm_transition_information_t *t_info,
                          void *priv_context)
{
 successor_state_ctx *p_ctx =
   reinterpret_cast<successor_state_ctx *>(priv_context);

 state_t st = p_ctx->system->new_state (sz);
 unsigned long buf_len = st.size;
 char *space_ptr = (char *)NIPSVM_STATE(st.ptr);
 // copy privately allocated state for later use
 nipsvm_state_copy(sz, succ, &space_ptr, &buf_len);

 p_ctx->succs.push_back(st);
 if (t_info->succ_cb_flags & INSTR_SUCC_CB_FLAG_SYS_BLOCK)
   p_ctx->succ_generation_type |= SUCC_DEADLOCK;
 
 return IC_CONTINUE;
}

nipsvm_status_t
search_error_callback (nipsvm_errorcode_t err, nipsvm_pid_t pid,
                       nipsvm_pc_t pc, void *priv_context)
{
  char str[256]; // 2005-11-11, weber@cwi.nl: safe

  nipsvm_errorstring (str, sizeof str, err, pid, pc);
  gerr << "NIPS RUNTIME ERROR: " << str << thr();
  return IC_STOP;
}

//// Implementation of system_t virtual interface: ////

//Constructor - yes it is not virtual, but it is needed and it completes
//the necessary interface
bymoc_system_t::bymoc_system_t(error_vector_t & evect): system_t(evect)
{ }

bymoc_system_t::~bymoc_system_t()
{ }

slong_int_t bymoc_system_t::read(std::istream & ins)
{
 terr << "bymoc_system_t::read not implemented" << thr(); //TODO
 return 1; //unreachable;
}

slong_int_t bymoc_system_t::read(const char * const filename)
{
    nipsvm_bytecode_t *bytecode =
        bytecode_load_from_file(filename, 0);
    
    if (bytecode == NULL) {
        terr << "Error loading bytecode file" << thr();
        // NOT REACHED
        return ERR_FILE_NOT_OPEN;
    }
    set_with_property (bytecode_monitor_present (bytecode) != 0);

	// set up context
    if (nipsvm_init (&nipsvm, bytecode,
                     &successor_state_callback, &search_error_callback) != 0) {
        terr << "Failed to initialized VM" << thr();
        // NOT REACHED
    }
    
    return 0;
}

slong_int_t bymoc_system_t::from_string(const std::string str)
{
 std::istringstream str_stream(str);
 return read(str_stream);
}

void bymoc_system_t::write(std::ostream & outs)
{
 terr << "bymoc_system_t::write not implemented" << thr();  //TODO
}

bool bymoc_system_t::write(const char * const filename)
{
 std::ofstream f(filename);
 if (f)
  {
   write(f);
   f.close();
   return true;
  }
 else
  { return false; }
}

std::string bymoc_system_t::to_string()
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

   ///// Methods of the virtual interface of system_t
   ///// dependent on can_property_process():

process_t * bymoc_system_t::get_property_process()
{
 terr << "bymoc_system_t::get_property_process not implemented" << thr();
 return 0; //unreachable
}

const process_t * bymoc_system_t::get_property_process() const
{
 terr << "bymoc_system_t::get_property_process not implemented" << thr();
 return 0; //unreachable
}

size_int_t bymoc_system_t::get_property_gid() const
{
 terr << "bymoc_system_t::get_property_gid not implemented" << thr();
 return 0; //unreachable
}

void bymoc_system_t::set_property_gid(const size_int_t gid)
{
 terr << "bymoc_system_t::set_property_gid not implemented" << thr();
}


property_type_t bymoc_system_t::get_property_type()
{
 terr << "bymoc_system_t::get_property_type not implemented, use bymoc_explicit_system_t" << thr();
 return NONE; //unreachable
}  


   ///// Methods of the virtual interface of system_t
   ///// dependent on can_processes()

size_int_t bymoc_system_t::get_process_count() const
{
 terr << "bymoc_system_t::get_process_count not implemented" << thr();
 return 0; //unreachable
}

process_t * bymoc_system_t::get_process(const size_int_t gid)
{
 terr << "bymoc_system_t::get_process not implemented" << thr();
 return 0; //unreachable
}

const process_t * bymoc_system_t::get_process(const size_int_t id) const
{
 terr << "bymoc_system_t::get_process not implemented" << thr();
 return 0; //unreachable
}

   ///// Methods of the virtual interface of system_t
   ///// dependent on can_transitions():

size_int_t bymoc_system_t::get_trans_count() const
{
 terr << "bymoc_system_t::get_trans_count not implemented" << thr();
 return 0; //unreachable
}

transition_t * bymoc_system_t::get_transition(size_int_t gid)
{
 terr << "bymoc_system_t::get_transition not implemented" << thr();
 return 0; //unreachable
}

const transition_t * bymoc_system_t::get_transition(size_int_t gid) const
{
 terr << "bymoc_system_t::get_transition not implemented" << thr();
 return 0; //unreachable
}

   ///// Methods of the virtual interface of system_t
   ///// dependent on can_be_modified() are here:

void bymoc_system_t::add_process(process_t * const process)
{
 terr << "bymoc_system_t::add_process not implemented" << thr();
}

void bymoc_system_t::remove_process(const size_int_t process_id)
{
 terr << "bymoc_system_t::remove_process not implemented" << thr();
}

///// END of the implementation of the virtual interface of system_t

