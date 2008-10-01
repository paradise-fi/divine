#include "system/prob_system.hh"
#include "system/prob_process.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

void prob_system_t::consolidate()
{
   //initialization of array mapping transitions to probabilistic transitions
   map_trans_to_prob_trans.resize(get_trans_count());
   map_trans_to_index_in_prob_trans.resize(get_trans_count());
   for (size_int_t i=0; i!=map_trans_to_prob_trans.size(); ++i)
    {
     map_trans_to_prob_trans[i]=NO_ID; //initial value is NO_ID
     map_trans_to_index_in_prob_trans[i]=MAX_SIZE_INT;
    }
   
   //filling of arrays prob_process, map_trans_to_prob_trans and
   //map_trans_to_index_in_prob_trans
   for (size_int_t i=0; i!=get_process_count(); i++)
    {
     prob_process_t * prob_process =
       dynamic_cast<prob_process_t *>(get_process(i));
  
     if (prob_process)
      {
       for (size_int_t i=0; i!=prob_process->get_prob_trans_count(); ++i)
        {
         /* appending prob. transition to the list of prob. transitions
          * and setting its GID */
         prob_transition_t * prob_trans = prob_process->get_prob_transition(i);
         prob_trans->set_gid(prob_transitions.size());
         prob_transitions.push_back(prob_trans);
         
         /* update of map_trans_to_prob_trans and
          *    map_trans_to_index_in_prob_trans: */
         for (size_int_t j=0; j!=prob_trans->get_trans_count(); ++j)
          {
           size_int_t trans_gid = prob_trans->get_transition(j)->get_gid();
           map_trans_to_prob_trans[trans_gid] = prob_trans->get_gid();
           map_trans_to_index_in_prob_trans[trans_gid] = j;
          }
        }
      }
     else
       terr << "Creating list of probabilistic transitions: "
               "Probabilistic process (prob_process_t) expected" << thr();
    }
}




