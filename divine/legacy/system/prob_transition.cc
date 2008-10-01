#include "system/prob_transition.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

prob_transition_t::prob_transition_t(): transition_t()
{
 initialize();
}
  
prob_transition_t::prob_transition_t(prob_system_t * const system):
  transition_t(system)
{
 initialize();
}
  
void prob_transition_t::initialize()
{
 weight_sum = 0;
}

void prob_transition_t::set_trans_count(const size_int_t size)
{
 prob_weights.resize(size);
 trans.resize(size);
 for (size_int_t j=0; j<size; j++)
   {
     trans[j]=0;
     prob_weights[j]=0;
   }
 weight_sum = 0;
}

void prob_transition_t::set_transition_and_weight(const size_int_t i,
                     transition_t * const p_trans, const ulong_int_t weight)
{
 weight_sum -= prob_weights[i];
 prob_weights[i] = weight;
 trans[i] = p_trans;    
 weight_sum += weight;
}

