
#ifndef DIVINE_SPOR_HH
#define DIVINE_SPOR_HH

#include <sevine.h>
#include <list>
#include <set>

namespace divine {

enum change_var_on_cycle_t { CV_INCREASE, CV_DECREASE, CV_NOTHING, CV_ANYTHING };
enum computed_change_t { CC_INCREASE, CC_DECREASE, CC_POSITIVE, CC_NEGATIVE, CC_ZERO, CC_GOOD_VAR, CC_ANYTHING };

  struct loc_cycle_t //structure containing information about a local cycle
{
  size_int_t cycle_length; //size of the field "transitions"
  size_int_t *transitions; //field of all transitions on the local cycle
  change_var_on_cycle_t *vars_and_chans; //which variable or channel is changed on the cycle; vars_and_chans[0..get_variable_count-1] is used for variables,
};                                       //and vars_and_chans[get_variable_count..get_variable_count+get_channel_count-1] for channels

struct glob_cycle_t //global cycle contains several (i.e. field of) local cycles; local cycles are identified by iterators
{
  size_int_t cycles_length; //size of the field "transitions"
  size_int_t *transitions; //field of all transitions on the global cycle
};

class spor_t
{
public:
  ~spor_t();
  spor_t(dve_explicit_system_t* system);
  void get_sticky_transitions(std::set<size_t> &sticky);
private:  
  dve_symbol_table_t* Table;
  dve_explicit_system_t* System;
  std::list<loc_cycle_t> local_cycles; //list of all local cycles  
  std::list<glob_cycle_t> global_cycles; //list of all global cycles
  std::set<size_t> sticky_trans;
  size_int_t *index_of_var; ////index_of_var[gid] = my local index of variable (vectors are "unwrapped")
  size_int_t unwrapped_variables; //index_of_var is field of size equal to unwrapped_variables
  dve_process_t *act_process; //for DFS
  std::deque<size_int_t> *process_graph; //for DFS: adjacency matrix of a graph corresponding to the state space of the process
  size_int_t stack_size;
  size_int_t *stack_of_trans; //mimics DFS stack in field for better collection of transitions composing actually detected cycle
  size_int_t *state_on_stack; //state_on_stack[LID] = position where local state with lid=LID lies on stack (LID=MAX_ULONG_INT iff not in stack)
  void dfs(size_int_t state);
  void collect_local_cycle(size_int_t start_state);
  void guard_expr_to_vars(const expression_t *expr0, change_var_on_cycle_t* vars);
  void effect_expr_to_vars(const dve_expression_t *expr0, change_var_on_cycle_t* vars);
  bool compute_changes(const dve_expression_t *expr_on_left, const dve_expression_t *expr0, computed_change_t &change);
  void detect_local_cycles(void);
  void global_cycles_cover(void);
  void remove_irelevant_local_cycles(void);
  void detect_global_cycles(void);
  void compute_sticky_transitions(void);
  void complete_to_global_cycle(std::list<std::list<loc_cycle_t>::iterator> *candidate, change_var_on_cycle_t *vars_and_chans);
};


}

#endif

