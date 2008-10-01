/*!\file
 * The main contribution of this file is the abstact interface prob_system_t -
 * the class representing a model of the propabilistic system
 */
#ifndef DIVINE_PROB_SYSTEM_HH
#define DIVINE_PROB_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include "system/system.hh"
#include "system/prob_transition.hh"

//The main DiVinE namespace - we do not want Doxygen to see it
namespace divine {
#endif //DOXYGEN_PROCESSING

//predeclarations:
class prob_transition_t;

//!Abstract interface of a class representing a model of a system
/*!This class provides an interface to the model of system.
 * Depending on abilities (see get_abilities()) you can
 * more or less deeply analyse the model.
 *
 * Furthermore you can generate states of the system using
 * child class explitcit_system_t (and its children)
 */
class prob_system_t: public virtual system_t
 {
  private:
  array_t<prob_transition_t *> prob_transitions;
  array_t<size_int_t> map_trans_to_prob_trans;
  array_t<size_int_t> map_trans_to_index_in_prob_trans;
  
  public:
  
  //!A constructor.
  /*!\param evect =
   * the <b>error vector</b>, that will be used by created instance of system_t
   */
  prob_system_t(error_vector_t & evect = gerr):system_t(evect) { }
   
  //!A destructor.
  virtual ~prob_system_t() {};

  //!Returns \GID of probabilistic transition containing transition with \GID
  //! `trans_gid' or returns NO_ID
  /*!\param trans_gid = \GID of transition
   * \return \GID of probabilistic transition containing transition with
   * \GID \a trans_gid. If there is no such a probabilistic transition, it
   * returns NO_ID */
  size_int_t get_prob_trans_of_trans(const size_int_t trans_gid) const
  { return map_trans_to_prob_trans[trans_gid]; }
  
  //!Returns the index of transition with \GID \a `trans_gid' in a probabilistic
  //! transition
  /*!\param trans_gid = \GID of transition
   * \return index of transition with \GID \a trans_gid in a probabilistic
   * transition, which contains it. If there is no such a probabilistic
   * transition, it returns NO_ID */
  size_int_t get_index_of_trans_in_prob_trans(const size_int_t trans_gid) const
  { return map_trans_to_index_in_prob_trans[trans_gid]; }

  //!Returns a count of probabilistic transitions
  virtual size_int_t get_prob_trans_count() const { return prob_transitions.size(); }

  //!Returns the pointer to the probabilistic transition given by \GID
  /* \note \GID is from interval 0..(get_prob_trans_count()-1) */
  virtual prob_transition_t * get_prob_transition(size_int_t gid)
  { return prob_transitions[gid]; }

  //!Returns the pointer to the constant probabilistic transition given by \GID
  /* \note \GID is from interval 0..(get_prob_trans_count()-1) */
  virtual const prob_transition_t * get_prob_transition(size_int_t gid) const
  { return prob_transitions[gid]; }
  
  protected:
  void consolidate(); //!<Protected method called after reading of input
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#endif //DOXYGEN_PROCESSING

#endif











