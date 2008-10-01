/*!\file
 * This file contains definition of path_t class*/
#ifndef DIVINE_PATH_HH
#define DIVINE_PATH_HH

#ifndef DOXYGEN_PROCESSING
#include <deque>
#include "system/state.hh"
#include "system/explicit_system.hh"
#include "common/stateallocator.hh"

namespace divine {//The main DiVinE namespace - we do not want Doxygen to see it
#endif //DOXYGEN_PROCESSING

  /*! Class path_t is used to construct and print the reprezentation of path in the state space.
   * For building a 'normal' path, just keep adding states from the beginning or end of the path.
   * For building a path with cycle, start from one end, keep adding states on path and 
   * mark the state where the cycle starts. You add this state only one time.
   *
   * Programmer should take care that states are added correctly (such that transitions between them exists)
  */

#define PATH_CYCLE_SEPARATOR "================"

  class path_t : public LegacyStateAllocator {
  private:
    std::deque<state_t> s_list;
    explicit_system_t* System;
    int cycle_start_index;
    std::string get_trans_string(state_t s1, state_t s2, bool with_prop);

  public:
    //!A construcor.
    /*!\param S = system, which generates this path*/
    path_t(explicit_system_t* S = 0) { System = S; cycle_start_index = -1;}
    //!A destructor.
    ~path_t() { state_t s; while (!s_list.empty()) {s = s_list.front(); s_list.pop_front(); delete_state(s); }}
    //!Sets a system, which generates this path
    void set_system(explicit_system_t* S) { System = S; }
    //!Empties the path and deletes contained states
    void erase() {state_t s; while (!s_list.empty()) {s = s_list.front(); s_list.pop_front(); delete_state(s); } cycle_start_index= -1;}

    /*! Add state to the beginning of the path
     */
    void push_front(state_t s) { state_t t = duplicate_state(s); s_list.push_front(t); if (cycle_start_index != -1) cycle_start_index++;}
    
    /*! Add state to the end of the path
     */
    void push_back(state_t s) {  state_t t = duplicate_state(s); s_list.push_back(t); }
    
    /*! Mark the current 1st state of the path as the 1st state of the cycle
     */
    void mark_cycle_start_front() { cycle_start_index = 0;}
    
    /*! Mark the current last state of the path as the 1st state of the cycle
     */
    void mark_cycle_start_back() { cycle_start_index = s_list.size() -1;}

    /*! Write the sequence of transitions of the path to the ostream
     */
    void write_trans(std::ostream & out = std::cout);
    
    /*! Write the sequence of states of the path to the ostream
     */
    void write_states(std::ostream & out = std::cout);
    
    /*! Returns length of the path
     */
    int length() {return s_list.size(); }

    /*! Returns pointer to the list (deque) of states of the path
     */
    std::deque<state_t>* get_state_list() { return &s_list; }

    /*! Returns index of state where cycle starts (if path is not of the lasso shape type, it returns -1)
     */
    int get_cycle_start_index() { return cycle_start_index; }
  };

#ifndef DOXYGEN_PROCESSING
}//The main DiVinE namespace - we do not want Doxygen to see it
#endif //DOXYGEN_PROCESSING


#endif
