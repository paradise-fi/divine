#include "bymoc_process_decomposition.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif

#include <climits>
#include <cassert>
#include "common/error.hh"

bymoc_process_decomposition_t::bymoc_process_decomposition_t(bymoc_explicit_system_t& system)
    : system_(system), bytecode_(system_.nipsvm.insn_context.bytecode),
      proc_id_(0)
{
    assert(bytecode_ != 0);
}

void
bymoc_process_decomposition_t::parse_process(std::size_t proc_id)
{
    assert(proc_id < INT_MAX);
    proc_id_ = (int)proc_id;
}

void
bymoc_process_decomposition_t::parse_monitor_process()
{
    proc_id_ = -1;
}

int
bymoc_process_decomposition_t::get_process_scc_id(state_t& st)
{
    if (proc_id_ == -1) {
        proc_id_ = be2h_pid (NIPSVM_STATE(st.ptr)->monitor_pid);
        if (proc_id_ == -1) {
            return -1;
        }
    }
    // get process
    st_process_header *p_proc =
        global_state_get_process (NIPSVM_STATE(st.ptr), proc_id_ );
    if (p_proc == 0) {
        return -1;
    }
    // get SCC id at PC
    st_scc_map *map = bytecode_scc_map (bytecode_, be2h_32(p_proc->pc));
    return map == 0 ? -1 : map->id;
}
    
int
bymoc_process_decomposition_t::get_process_scc_type(state_t& st)
{
    t_scc_type scc_id = get_process_scc_id(st);
    return bytecode_scc_type (bytecode_, scc_id);
}
    
int
bymoc_process_decomposition_t::get_scc_type(int scc_id)
{
    return bytecode_scc_type (bytecode_, scc_id);
}
    
int
bymoc_process_decomposition_t::get_scc_count()
{
    gerr << "bymoc_process_decomposition_t::get_scc_count not implemented" << thr(); //TODO
    return -1;
}
    
bool
bymoc_process_decomposition_t::is_weak()
{
    return bytecode_->scc_info.weak != 0;
}
