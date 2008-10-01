#include "system/dve/dve_prob_explicit_system.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif

int dve_prob_explicit_system_t::get_succs(state_t state, prob_succ_container_t & succs, enabled_trans_container_t & etc)
{
  succs.clear(); etc.clear();
  only_succs.clear();
  int result = dve_explicit_system_t::get_succs(state, only_succs, etc);
  for (size_int_t i=0; i!=only_succs.size(); ++i)
   {
    size_int_t trans_gid = get_sending_or_normal_trans(etc[i])->get_gid();
    size_int_t property_trans_gid =
       (get_with_property()) ? get_property_trans(etc[i])->get_gid() :
                               NO_ID;
    size_int_t prob_trans_gid = get_prob_trans_of_trans(trans_gid);
    size_int_t weight = 0;
    size_int_t sum = 0;
    if (prob_trans_gid!=NO_ID)
     {
      prob_transition_t * prob_trans = get_prob_transition(prob_trans_gid);
      weight =
        prob_trans->get_weight(get_index_of_trans_in_prob_trans(trans_gid));
      sum = prob_trans->get_weight_sum();
     }
    succs.push_back(prob_succ_element_t(only_succs[i],weight,sum,
               prob_and_property_trans_t(prob_trans_gid, property_trans_gid)));
   }
  
  return result;
}

int dve_prob_explicit_system_t::get_succs_ordered_by_prob_and_property_trans(state_t state, prob_succ_container_t & succs)
{
  succs.clear();
  int result=get_succs(state,aux_prob_succs,*aux_enabled_trans);
  
  /*re-ordering:*/
  if (!succs_deadlock(result) && aux_prob_succs.size())
   {
    size_int_t P = aux_enabled_trans->get_property_succ_count();
    size_int_t M = aux_enabled_trans->size()/P;
    
//    cerr<< "P=" << P << ", M="<<M << endl;

    //write those with prob_trans_gid==NO_ID first
    for (size_int_t i=0; i!=P; ++i)
     {
      for (size_int_t j=0; j!=M; ++j)
       {
        size_int_t to_move = i+j*P;
        if (aux_prob_succs[to_move].prob_and_property_trans.prob_trans_gid==NO_ID)
         {//if it has no pr
          succs.push_back(aux_prob_succs[to_move]);
         }
       }
     }

    //write those with prob_trans_gid set..
    for (size_int_t i=0; i!=P; ++i)
     {
      for (size_int_t j=0; j!=M; ++j)
       {
        size_int_t to_move = i+j*P;
        if (aux_prob_succs[to_move].prob_and_property_trans.prob_trans_gid!=NO_ID)
         {
          succs.push_back(aux_prob_succs[to_move]);
         }
       }
     }
   }
  else //In case of deadlock..
   {
    //Copy it simply 
    for (size_int_t i=0; i!=aux_prob_succs.size(); ++i)
      succs.push_back(aux_prob_succs[i]);
   }
  
  return result;
}

