/* \file
 * The main contribution of this file is the abstact interface prob_transition_t.
 */
#ifndef DIVINE_PROB_TRANSITION_HH
#define DIVINE_PROB_TRANSITION_HH

#include "system/system.hh"
#include "system/prob_system.hh"

#ifndef DOXYGEN_PROCESSING

namespace divine { //We want Doxygen not to see namespace `divine'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING


//predeclarations of classes:
class system_t;
class prob_system_t;

//definition of the main class of this file:
//!Abstract interface of a class representing a probabilistic transition
/*!This class represents a probabilistic transition. It is a part of the model
 * of a probabilistic system.
 *
 * This class is derived from transition_t, although, in fact, it is really
 * different from the ordinary system transition:
 * - it consists of several ordinary transitions
 * - each ordinary transition contained in a probabilistic transition has a
 *   weight (probability of the i-th ordinary transition can be computed
 *   as <code>prob_trans.get_weight(i)/prob_trans.get_weight_sum()</code>
 */
class prob_transition_t: public virtual transition_t
{
 protected:  
  ulong_int_t weight_sum;
  array_t<ulong_int_t> prob_weights;
  array_t<transition_t*> trans;  
  
  //!A protected initializer
  void initialize();

 public:
  
  //!A constructor
  prob_transition_t();
  
  //!A constructor
  prob_transition_t(prob_system_t * const system);
  
  //!Returns a number of transition, which the probabilistic transition
  //! consists of
  size_int_t get_trans_count() const
  {
    return trans.size();
  }
  
  //!Returns a pointer to the `i'-th transition of the probabilistic transition
  transition_t * get_transition(const size_int_t i)
  {
    return trans[i];
  } 
  
  //!Returns a pointer to the constant `i'-th transition of the probabilistic
  //! transition
  const transition_t * get_transition(const size_int_t i) const
  {
    return trans[i];
  } 
  
  //!Returns a weight of `i'-th transition
  ulong_int_t get_weight(const size_int_t i) const
  {
    return prob_weights[i];
  }

  //!Returns a sum of weights of all transitions contained in the probabilistic
  //! transition
  /*!\note Usage of get_weight_sum() is faster than computation of the sum
   * explicitly using repeated call of get_weight(). get_weight_sum() returns
   * precomputed value.
   */
  ulong_int_t get_weight_sum() const
  {
    return weight_sum;
  }

  //!Sets a number of transitions contained in the probabilistic transition
  /*!\note
   * Existing transitions are not lost by setting new count of transitions.
   * Do not forget to delete all transitions, which you do not plan to use.
   */
  void set_trans_count(const size_int_t size);

  //!Sets `i'-th transition and its weight
  /*!\warning
   * If there has alreadty been set `i'-th transition before, this transition
   * is not deleted automatically. Thus, do not forget to call
   * <code>delete(pr_trans.get_transition(i))</code> before rewritting
   * it by the pointer to another transition.
   */
  void set_transition_and_weight(const size_int_t i,
                  transition_t * const p_trans, const ulong_int_t weight);

};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif











