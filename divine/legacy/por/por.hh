/*!\file por.hh
 *  * This file contains the class \ref por_t for utilization of partial order reduction. */

#ifndef DIVINE_POR_HH
#define DIVINE_POR_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <stack>
#include "system/explicit_system.hh"
#include "system/dve/dve_explicit_system.hh"
#include "common/bit_string.hh"
#include "por/spor.hh"

using namespace std;
namespace divine { //We want Doxygen not to see namespace `dve'
#endif //DOXYGEN_PROCESSING

//!Defines the type of choosing ample set - transitions from the <b>first</b> proper process will be taken
const std::size_t POR_FIRST     = 1;
//!Defines the type of choosing ample set - transitions from the <b>last</b> proper process will be taken
const std::size_t POR_LAST      = 2;
//!Defines the type of choosing ample set - transitions from the proper process with <b>smallest</b> number of transitions will be taken
const std::size_t POR_SMALLEST  = 3;
//!Defines the type of choosing ample set - only finds processes whose transitions can be taken as ample set
const std::size_t POR_FIND_ONLY = 4;

//!Class for utilization of partial order reduction.
/*! Class implements basic features necessary to using partial order reduction, i.e. static analysis findindg dependency and visibility relations, or choosing appropriate candidates for ample-sets. <b>The cycle condition is not ensured by this class and it has to be treated externally.</b>
 * 
*/

  class por_t {
  private:
    bool initialized; // If true, memory for dep and pre is allocated. Tested in destructor.
    std::size_t size_of_glob_vars; //System->get_global_variable_count() plus lengths of all variables vectors of the system plus number of channels (due to buffers :-( )
    std::size_t size_of_glob_vars_without_buffers; //System->get_global_variable_count() plus lengths of all variables vectors of the system
    std::size_t *index_of_glob_var; //index_of_glob_var[gid] = my local index of global variable (vectors are "unwrapped")
    dve_explicit_system_t *System;
    dve_symbol_table_t *Table;
    std::size_t choose_type;
    const process_t *prop_process;
    std::size_t trans_count, proc_count; //number of all transitions and processes (included property process)
    //two next items are computed statically in order to speedup the computation of ample sets (e.g. we move really slow dynamic_cast<..> to the static part of computation)
    std::size_t *process_of_trans; //process_of_trans[GID]=gid_of_process_of_transition_with_gid=GID
    size_int_t *state1_of_trans; //state1_of_trans[GID]=lid_of_local_outgoing_state_of_transition_with_gid=GID
    bit_string_t *dep; //dep[i] is bitvector of processes owning some transition depending on the transition with gid=i
    bit_string_t *pre; //pre[i] is bitvector of processes owning some transition which may enabled the transition with gid=i
    bit_string_t trans_is_visible; // is transition with given GID visible wrt. property process or not?
    void find_visible_symbols_from_ba(bool *glob_vars, bool *loc_states);
    void find_visible_symbols_from_expr(list<expression_t*>* vis_list, bool *glob_vars, bool *loc_states);
    void parse_effect_expr(const expression_t *expr0, bool *glob_vars);
    void parse_guard_expr(const expression_t *expr0, bool *glob_vars, bool *loc_states);
    bool set_pre(bool *trans_scan_loc_states, bool *trans_scan_vars, bool *trans_change_vars, const transition_t *trans1, const transition_t *trans2);
    std::size_t choose_ample_set(bool *ample_sets, enabled_trans_container_t *enabled_trans);
  public:
    bool count_approx_interuptions;
    unsigned *pre_interupted;
    unsigned *dep_interupted;
    unsigned *vis_interupted;
    bit_string_t *full_pre, *full_dep, full_vis;
    //!A creator
    inline por_t(void);
    //!Method for estimating of choosing specific ample set, parameter type is one of constants \ref POR_FIRST, \ref POR_LAST, \ref POR_SMALLEST or \ref POR_FIND_ONLY.
    void set_choose_type(std::size_t type) {choose_type = type; }
    //!Initialization and static analysis of the system, <b>necessary</b> to call.
    /*! If the system is without property, visibility is computed from the
     *  list of expressions given in the argument vis_list. For the systems
     *  with the property the parameter is omitted. */
    void init(explicit_system_t *S, list<expression_t*>* vis_list = NULL);
    //!Generates all enabled transitions and offers possible candidates to an ample set composed of several processes
    /*! Extension of classic ample_set notion: ample set can composed of
     *  several processes here.  \param s = state for which we generate
     *  ample set \param ample_sets = set of dimension equal to number of
     *  processes of the system. If ample_set[i]=true, then there exists an
     *  ample set A containing process i, where A is a proper subset of all
     *  enabled conditions.  \param ample_trans = array of
     *  enabled_trans_container_t of dimension equal to number of processes
     *  of the system. if ample_set[i]=true, then ample_trans[i] contains
     *  (enabled) transitions of an ample set which covers transitions of
     *  the i-th process.  \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and
     *  SUCC_DEADLOCK according to explicit_system_t::get_enabled_trans (use
     *  functions explicit_system_t::succs_normal(),
     *  explicit_system_t::succs_error() and
     *  explicit_system_t::succs_deadlock() for testing) */
    int generate_composed_ample_sets(state_t s, bool *ample_sets, enabled_trans_container_t **ample_trans, enabled_trans_container_t &all_enabled_trans);
    //!Generates all enabled transitions and offers possible candidates to an ample set.
    /*! \param s = state for which we generate ample set \param ample_sets =
     *  set of dimension equal to number of processes of the system. If
     *  ample_sets[i]=true, then the set of transitions of the i-th process
     *  is a candidate to ample set.  \param enabled_trans = container of
     *  all enabled transitions as generated by function
     *  explicit_system_t::get_async_enabled_trans (programmer don't have to
     *  generate them again, which would be time consumed) \return bitwise
     *  OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK according to
     *  explicit_system_t::get_enabled_trans (use functions
     *  explicit_system_t::succs_normal(), explicit_system_t::succs_error()
     *  and explicit_system_t::succs_deadlock() for testing) */
    int generate_ample_sets(state_t s, bool *ample_sets, enabled_trans_container_t &enabled_trans, std::size_t &ample_proc_gid);
    //!Generates all enabled transitions and proc_gid of ample set choosed by estimated heuristic.
    /*! If proc_gid==system_t::get_process_count(), then the state must be
     * fully expanded.  \param s = state for which we generate ample set
     * \param enabled_trans = container of all enabled transitions as
     * generated by function explicit_system_t::get_async_enabled_trans
     * (programmer hasn't generate them again, which would be time consumed)
     * \param proc_gid = \GID of process whose (enabled) transitions can be
     * taken as ample set. The process is choosed by estimated heuristic
     * (see a creator of this class por_t::por_t) \return bitwise OR of
     * SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK according to
     * explicit_system_t::get_enabled_trans (use functions
     * explicit_system_t::succs_normal(), explicit_system_t::succs_error()
     * and explicit_system_t::succs_deadlock() for testing) */
    int ample_set(state_t s, enabled_trans_container_t &enabled_trans, std::size_t &proc_gid); 
    //!Generates successors of ample set choosed by estimated heuristic
    /*! \param s = state for which we generate ample set \param succs =
     *  container of generated successors \return \GID of process whose
     *  (enabled) transitions can be taken as ample set. The process is
     *  choosed by estimated heuristic (see a creator of this class
     *  por_t::por_t). If proc_gid==system_t::get_process_count(), then the
     *  state is fully expanded (and parameter <it>succs</it> contains all
     *  enabled successors).  \return bitwise OR of SUCC_NORMAL, SUCC_ERROR
     *  and SUCC_DEADLOCK according to explicit_system_t::get_enabled_trans
     *  (use functions explicit_system_t::succs_normal(),
     *  explicit_system_t::succs_error() and
     *  explicit_system_t::succs_deadlock() for testing)
     */
    int ample_set_succs(state_t s, succ_container_t & succs, std::size_t &proc_gid);
    //!Returns a bit_string_t structure (see \ref bit_string_t class) containing information which transition is visible or not (trans[i]==true iff transition with \GID=i is visible).
    bit_string_t get_visibility();
    //!Adds to visible transitions so called sticky transitions in order to fulfill cycle condition (by static analysis)
    void static_c3();
    //!Method for changing of visibility relation (trans[i]==true iff transition with \GID=i is visible).
    void set_visibility(bit_string_t trans);
    //!Returns field of bit_string_t storing for all transition set of processes whose may enable this transition
    void get_pre(bit_string_t* result);
    //Method for changing of "pre" - field of bit_string_t storing for all transition set of processes whose may enable this transition <b>If you change pre, you have to change dep to satisfy pre[i] is a subset of dep[i], for all <it>i</it></b>
    void set_pre(bit_string_t* new_pre);
    //Returns field of bit_string_t storing for all transition set of processes whose execution can be dependent on this transition (e.g. it stores dependency relation)
    void get_dep(bit_string_t* result);
    //Method for changing of "dep" - field of bit_string_t storing for all transition set of processes whose execution can be dependent on this transition
    void set_dep(bit_string_t* new_dep);
    //!A destructor (frees some used memory).
    ~por_t(); 
  };

inline por_t::por_t(void)
{ choose_type=POR_FIND_ONLY; initialized = false;   
}

};


#endif //DIVINE_POR_HH
