/*!\file bymoc_process_decomposition.hh
 * Process graph decomposition into SCCs including types */
#ifndef _DIVINE_BYMOC_PROCESS_DECOMPOSITION_HH_
#define _DIVINE_BYMOC_PROCESS_DECOMPOSITION_HH_
 
#ifndef DOXYGEN_PROCESSING
#include "common/process_decomposition.hh"
#include "system/bymoc/bymoc_explicit_system.hh"
#include "system/bymoc/bymoc_process.hh"
#include "system/bymoc/vm/nipsvm.h"
namespace divine {
#endif

class bymoc_process_decomposition_t : public process_decomposition_t
{
protected:
    bymoc_explicit_system_t& system_;
    nipsvm_bytecode_t *bytecode_;
    int proc_id_;
public:
    bymoc_process_decomposition_t(bymoc_explicit_system_t&);
    
    void parse_process(std::size_t);
    void parse_monitor_process();
    int get_process_scc_id(state_t&);
    int get_process_scc_type(state_t&);    
    int get_scc_type(int);
    int get_scc_count();
    bool is_weak();
};


#ifndef DOXYGEN_PROCESSING
}
#endif
#endif
