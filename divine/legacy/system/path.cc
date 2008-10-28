#include "system/path.hh"

using namespace divine;
using namespace std;


string path_t::get_trans_string(state_t s1, state_t s2, bool with_prop) {
  if (!System->can_system_transitions()) return "Unknown system transition";
  succ_container_t succs; //list of enabled successors states
  enabled_trans_container_t enabled_trans(*System); //list of enabled transitions
    
  try  {
    System->get_succs(s1, succs, enabled_trans);
  }  catch (ERR_throw_t)  {
    for (ERR_nbr_t i = 0; i!=gerr.count(); i++)
    { gerr.perror("get_trans_string: successor error",i); }
    gerr.clear();
    return "Succ error";
  }
  std::size_t info_index=0;
  while ((info_index!=succs.size())&&(succs[info_index]!=s2))
    {
      info_index++;
    }
  
  for (succ_container_t::iterator i=succs.begin();i!=succs.end();i++)
    {
      delete_state(*i);
    }
  
  if (info_index==enabled_trans.size()) //there is no edge s1->s2, error
  { cerr << "get_trans_string: there is no desired transition to print." << endl;
    return "No such succ";
  }
  return enabled_trans[info_index].to_string();
}



void path_t::write_trans(std::ostream & out) {
  for(unsigned i=0; i<s_list.size(); i++) {
    if (i == cycle_start_index) {
      out << "================"<<endl;
    }
    if (i+1 < s_list.size()) {
      out << get_trans_string(s_list[i], s_list[i+1], false) << endl;
    }
  }
  if (cycle_start_index != -1) {
      succ_container_t succs;
    if (System->get_succs(s_list[cycle_start_index], succs) != SUCC_ERROR)
      {
        if (succs.size()) //property process is not in deadlock
	  out << get_trans_string(s_list[s_list.size()-1], s_list[cycle_start_index], false) << endl;
      }
    for (size_t i = 0; i < succs.size(); i++)
      {
	delete_state(succs[i]);
      }
  }
}

void path_t::write_states(std::ostream & out) {
  for(unsigned i=0; i<s_list.size(); i++) {
    if (i == cycle_start_index) {
      out << PATH_CYCLE_SEPARATOR <<endl<<endl;
    }
    System->print_state(s_list[i], out); out << endl;
  }
}

